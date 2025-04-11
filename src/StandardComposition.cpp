/*
 *
 * StandardComposition.cpp
 *
 * JP Robichaud (jp@rev.com)
 * 2021
 *
 */

#include "StandardComposition.h"
#include "logging.h" // For logger
#include "fstalign.h" // Include for AlignerOptions, FstAlignOption
#include <fst/determinize.h> // For Determinize
#include <fst/compose.h> // For Compose
#include <fst/arcsort.h> // For ArcSort

StandardCompositionFst::StandardCompositionFst(const fst::StdFst &fstA, const fst::StdFst &fstB)
    : StandardCompositionFst(fstA, fstB, *(fstA.InputSymbols()), AlignerOptions())
{
     if (fstA.InputSymbols() == nullptr) {
         throw std::runtime_error("StandardCompositionFst requires symbol table if not provided explicitly (or attach symbols to fstA).");
    }
}

StandardCompositionFst::StandardCompositionFst(const fst::StdFst &fstA, const fst::StdFst &fstB, const SymbolTable &symbols)
    : StandardCompositionFst(fstA, fstB, symbols, AlignerOptions()) {}


StandardCompositionFst::StandardCompositionFst(const fst::StdFst &fstA, const fst::StdFst &fstB, const SymbolTable &symbols, const AlignerOptions& options)
  : strict_punctuation_(options.strict_punctuation),
    punctuation_ids_(options.punctuation_ids),
    symbols_(symbols)
{
    auto logger_ = logger::GetOrCreateLogger("StandardCompositionFst");
    logger_->set_level(spdlog::level::info);

    FstAlignOption fst_options;
    auto sub_label_id_ = symbols_.Find(fst_options.symSub);
    auto del_label_id_ = symbols_.Find(fst_options.symDel);
    auto ins_label_id_ = symbols_.Find(fst_options.symIns);

    float insertion_cost = 1.0;
    float deletion_cost = 1.0;
    float substitution_cost = 1.0;

    StdVectorFst halfEdit1;
    StdVectorFst halfEdit2;
    halfEdit1.SetInputSymbols(&symbols_);
    halfEdit1.SetOutputSymbols(&symbols_);
    halfEdit1.AddState();
    halfEdit1.SetStart(0);
    halfEdit1.SetFinal(0, 0);
    halfEdit1.AddArc(0, StdArc(0, ins_label_id_, insertion_cost / 2, 0));

    halfEdit2.SetInputSymbols(&symbols_);
    halfEdit2.SetOutputSymbols(&symbols_);
    halfEdit2.AddState();
    halfEdit2.SetStart(0);
    halfEdit2.SetFinal(0, 0);
    halfEdit2.AddArc(0, StdArc(del_label_id_, 0, deletion_cost / 2, 0));

    for (SymbolTableIterator siter(symbols_); !siter.Done(); siter.Next()) {
         int64_t sid = siter.Value();
         if (sid == 0 || sid == ins_label_id_ || sid == del_label_id_ || sid == sub_label_id_) {
            continue;
         }
         auto sym_tk = symbols_.Find(sid);
         bool isClassLabel = isEntityLabel(sym_tk);

         if (isClassLabel) {
            halfEdit1.AddArc(0, StdArc(sid, sid, 0, 0));
            halfEdit1.AddArc(0, StdArc(sid, del_label_id_, -deletion_cost / 2, 0));
            halfEdit2.AddArc(0, StdArc(sid, sid, 0, 0));
         } else {
            halfEdit1.AddArc(0, StdArc(sid, sid, 0, 0));
            halfEdit1.AddArc(0, StdArc(sid, sub_label_id_, substitution_cost / 2, 0));
            halfEdit1.AddArc(0, StdArc(sid, del_label_id_, deletion_cost / 2, 0));
            halfEdit2.AddArc(0, StdArc(sid, sid, 0, 0));
            halfEdit2.AddArc(0, StdArc(sub_label_id_, sid, substitution_cost / 2, 0));
            halfEdit2.AddArc(0, StdArc(ins_label_id_, sid, insertion_cost / 2, 0));
         }
    }

    StdVectorFst detRefFst;
    Determinize(fstA, &detRefFst);

    StdVectorFst halfCompose1;
    Compose(detRefFst, halfEdit1, &halfCompose1);
    if (halfCompose1.NumStates() == 0) {
        logger_->error("halfCompose1 (ref o edits) produced an FST with 0 states. Ref states: {}, Edit1 states: {}", detRefFst.NumStates(), halfEdit1.NumStates());
        throw std::runtime_error("Composition failed: halfCompose1 has 0 states");
    }
    ArcSort(&halfCompose1, fst::OLabelCompare<StdArc>());

    StdVectorFst halfCompose2;
    Compose(halfEdit2, fstB, &halfCompose2);
     if (halfCompose2.NumStates() == 0) {
        logger_->error("halfCompose2 (edits o hyp) produced an FST with 0 states. Edit2 states: {}, Hyp states: (Input FST, size unknown)", halfEdit2.NumStates());
        throw std::runtime_error("Composition failed: halfCompose2 has 0 states");
    }
    ArcSort(&halfCompose2, fst::ILabelCompare<StdArc>());

    logger_->info("Performing lazy composition");
    fstC_ = std::make_unique<fst::StdComposeFst>(halfCompose1, halfCompose2);

    if (!fstC_) {
         throw std::runtime_error("Lazy composition object creation failed in StandardCompositionFst");
    }
    if (fstC_->Start() == fst::kNoStateId) {
        logger_->error("Lazy composition resulted in an FST with no start state.");
        throw std::runtime_error("Composition failed: resulting FST has no start state");
    }
    logger_->info("Standard composition setup complete. Composed FST start state: {}", fstC_->Start());
}

StateId StandardCompositionFst::Start() { return (*fstC_).Start(); }

fst::Fst<fst::StdArc>::Weight StandardCompositionFst::Final(StateId stateId) { return (*fstC_).Final(stateId); }

bool StandardCompositionFst::TryGetArcsAtState(StateId fromStateId, vector<fst::StdArc> *out_vector) {
  assert(out_vector != NULL);

  for (ArcIterator<StdFst> aiter(*fstC_, fromStateId); !aiter.Done(); aiter.Next()) {
    const fst::StdArc &arc = aiter.Value();
    
    // --- Strict Punctuation Check --- 
    if (strict_punctuation_) {
        // Skip arcs representing word <-> punctuation substitutions
        // Epsilon (0) is allowed (handled by punctuation_ids_ containing 0)
        bool ilabel_is_punct = (punctuation_ids_.count(arc.ilabel) > 0);
        bool olabel_is_punct = (punctuation_ids_.count(arc.olabel) > 0);
        
        if (ilabel_is_punct != olabel_is_punct) {
             // One is punctuation, the other is not. This is a disallowed substitution.
             // Note: This assumes the underlying composition correctly assigned non-zero
             // weight to substitutions. If a word-punct sub somehow had weight 0 here,
             // this logic wouldn't catch it based on cost, but the label check works.
             continue; // Skip adding this arc to the output
        }
    }
    // --- End Strict Punctuation Check ---
    
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
