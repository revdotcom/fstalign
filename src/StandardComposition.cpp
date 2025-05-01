/*
 *
 * StandardComposition.cpp
 *
 * JP Robichaud (jp@rev.com)
 * 2021
 *
 */

#include "StandardComposition.h"
#include <fstream>
#include <memory>
#include <limits> // For numeric_limits

using fst::StdArc;
using fst::StdVectorFst;
using fst::SymbolTable;
using fst::TropicalWeight;
using fst::SymbolTableIterator;
using fst::ArcIterator;
using fst::StateIterator;
using fst::FstWriteOptions;
using fst::kNoStateId;
// StateId is defined via typedef in IComposition.h
using std::vector;
using std::string;
using std::ofstream;

// --- Constructors (ensure symbols_ is initialized) ---
StandardCompositionFst::StandardCompositionFst(const fst::StdFst &fstA, const fst::StdFst &fstB)
    : StandardCompositionFst(fstA, fstB, *(fstA.InputSymbols()), AlignerOptions()) // Use default AlignerOptions
{
    if (fstA.InputSymbols() == nullptr) {
         throw std::runtime_error("StandardCompositionFst requires symbol table. Attach symbols to fstA or provide explicitly.");
    }
}

StandardCompositionFst::StandardCompositionFst(const fst::StdFst &fstA, const fst::StdFst &fstB, const SymbolTable &symbols)
    : StandardCompositionFst(fstA, fstB, symbols, AlignerOptions()) {} // Use default AlignerOptions

StandardCompositionFst::StandardCompositionFst(const fst::StdFst &fstA, const fst::StdFst &fstB, const SymbolTable &symbols, const AlignerOptions& options)
  : strict_punctuation_(options.strict_punctuation),
    punctuation_ids_(options.punctuation_ids),
    symbols_(symbols), // Initialize member reference
    use_favored_substitutions_(options.use_favored_substitutions),
    favored_substitution_cost_(options.favored_substitution_cost),
    favorable_substitution_map_(options.favorable_substitution_map)
{
    auto logger_ = logger::GetOrCreateLogger("StandardCompositionFst");
    logger_->set_level(spdlog::level::info);

    logger_->info("Starting Standard Composition. fstA.Start: {}, fstB.Start: {}", fstA.Start(), fstB.Start());

    FstAlignOption fst_options; // Assuming this defines symSub, symDel, symIns strings
    auto sub_label_id_ = symbols_.Find(fst_options.symSub);
    auto del_label_id_ = symbols_.Find(fst_options.symDel);
    auto ins_label_id_ = symbols_.Find(fst_options.symIns);
    if (sub_label_id_ == fst::kNoSymbol || del_label_id_ == fst::kNoSymbol || ins_label_id_ == fst::kNoSymbol) {
        logger_->error("Could not find special edit symbols ('{}', '{}', '{}') in the symbol table.", fst_options.symSub, fst_options.symDel, fst_options.symIns);
        throw std::runtime_error("Missing special symbols in symbol table.");
    }

    // --- Create Edit FSTs (halfEdit1, halfEdit2) ---
    StdVectorFst halfEdit1;
    StdVectorFst halfEdit2;
    halfEdit1.SetInputSymbols(&symbols_);
    halfEdit1.SetOutputSymbols(&symbols_);
    halfEdit1.AddState();
    halfEdit1.SetStart(0);
    halfEdit1.SetFinal(0, TropicalWeight::One());
    halfEdit1.AddArc(0, StdArc(0, ins_label_id_, insertion_cost / 2, 0)); // eps:ins

    halfEdit2.SetInputSymbols(&symbols_);
    halfEdit2.SetOutputSymbols(&symbols_);
    halfEdit2.AddState();
    halfEdit2.SetStart(0);
    halfEdit2.SetFinal(0, TropicalWeight::One());
    halfEdit2.AddArc(0, StdArc(del_label_id_, 0, deletion_cost / 2, 0)); // del:eps

    for (SymbolTableIterator siter(symbols_); !siter.Done(); siter.Next()) {
        int64_t sid = siter.Value();
        if (sid == 0 || sid == ins_label_id_ || sid == del_label_id_ || sid == sub_label_id_) {
            continue;
        }

        auto sym_tk = symbols_.Find(sid);
        bool isClassLabel = isEntityLabel(sym_tk);
        // Simplified: Check for entity labels if needed (same as develop version)
        // bool isClassLabel = false; // isEntityLabel(symbols_.Find(sid)); in develop
        if (isClassLabel) {
                // Same handling as in develop version
            logger_->info("Token class label found for {}", symbols.Find(sid));
            halfEdit1.AddArc(0, StdArc(sid, sid, 0, 0));
            halfEdit1.AddArc(0, StdArc(sid, del_label_id_, -deletion_cost / 2, 0));
            halfEdit2.AddArc(0, StdArc(sid, sid, 0, 0));
        } else {
            // Standard symbol edits - exactly as in develop version
            halfEdit1.AddArc(0, StdArc(sid, sid, 0, 0)); // id:id
            halfEdit1.AddArc(0, StdArc(sid, sub_label_id_, substitution_cost / 2, 0)); // id:sub
            halfEdit1.AddArc(0, StdArc(sid, del_label_id_, deletion_cost / 2, 0)); // id:del

            halfEdit2.AddArc(0, StdArc(sid, sid, 0, 0)); // id:id
            halfEdit2.AddArc(0, StdArc(sub_label_id_, sid, substitution_cost / 2, 0)); // sub:id
            halfEdit2.AddArc(0, StdArc(ins_label_id_, sid, insertion_cost / 2, 0)); // ins:id
        }
    }
    
    logger_->info("Created halfEdit FSTs with self-loops");

    // Step 1: Determinize the reference FST
    StdVectorFst detRefFst;
    Determinize(fstA, &detRefFst);
    logger_->info("Determinized fstA has {} states", detRefFst.NumStates());

    // Step 2: Compose the first half
    StdVectorFst halfCompose1;
    logger_->info("Composing detRefFst o halfEdit1");
    Compose(detRefFst, halfEdit1, &halfCompose1);
    logger_->debug("halfCompose1 has {} states", halfCompose1.NumStates());
    
    // Check if first composition worked
    if (halfCompose1.NumStates() == 0) {
        logger_->warn("halfCompose1 (ref o edits) produced an FST with 0 states");
        logger_->warn("halEdit1 was:");
        printFst("fstalign", &halfEdit1, &symbols_);
        return;
    }

    // Sort for composition
    ArcSort(&halfCompose1, fst::StdOLabelCompare());

    if (halfCompose1.NumStates() < 100) {
        printFst("fstalign", &halfCompose1, &symbols_);
    }

    // Step 3: Compose the second half
    StdVectorFst halfCompose2;
    logger_->info("Composing halfEdit2 o fstB");
    Compose(halfEdit2, fstB, &halfCompose2);
    logger_->debug("halfCompose2 has {} states", halfCompose2.NumStates());
    
    // Check if second composition worked
    if (halfCompose2.NumStates() == 0) {
        logger_->warn("halfCompose2 (edits o hyp) produced an FST with 0 states");
        logger_->warn("halEdit2 was:");
        printFst("fstalign", &halfEdit2, &symbols_);
        return;
    }

    // Sort for composition
    ArcSort(&halfCompose2, fst::StdILabelCompare());
    if (halfCompose2.NumStates() < 100) {
      logger_->info("halfCompose2 has {} states", halfCompose2.NumStates());
      printFst("fstalign", &halfCompose2, &symbols_);
    } else {
      logger_->info("halfCompose2 is too large to print, it has {} states", halfCompose2.NumStates());
    }

    // Step 4: Final lazy composition
    logger_->info("Performing lazy composition");
    fstC_ = std::make_unique<fst::StdComposeFst>(halfCompose1, halfCompose2);
    
    StateIterator<fst::StdFst> siter(*fstC_);
    logger_->info("Standard composition complete");
}

StateId StandardCompositionFst::Start() {
    return fstC_->Start();
}

fst::Fst<fst::StdArc>::Weight StandardCompositionFst::Final(StateId stateId) {
    return fstC_->Final(stateId);
}

bool StandardCompositionFst::TryGetArcsAtState(StateId fromStateId, vector<fst::StdArc> *out_vector) {
    assert(out_vector != NULL);
    // out_vector->clear();

    for (ArcIterator<fst::StdFst> aiter(*fstC_, fromStateId); !aiter.Done(); aiter.Next()) {
      const fst::StdArc &arc = aiter.Value();
      out_vector->push_back(arc);
    }

    return true;
}

StandardCompositionFst::~StandardCompositionFst() {}

void StandardCompositionFst::DebugComposedGraph(string debug_filename) {
  StdVectorFst composedFst(*fstC_);
  ofstream outfile(debug_filename);
  FstWriteOptions wopts;
  composedFst.SetInputSymbols(&symbols_);
  composedFst.SetOutputSymbols(&symbols_);
  wopts.write_isymbols = true;
  wopts.write_osymbols = true;
  wopts.write_header = true;
  composedFst.Write(outfile, wopts);
}