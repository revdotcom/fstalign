/*
 *
 * StandardComposition.cpp
 *
 * JP Robichaud (jp@rev.com)
 * 2021
 *
 */

#include "StandardComposition.h"

StandardCompositionFst::StandardCompositionFst(const fst::StdFst &fstA, const fst::StdFst &fstB, SymbolTable &symbols) {
  symbols_ = &symbols;

  logger_ = logger::GetOrCreateLogger("StandardCompositionFst");
  logger_->set_level(spdlog::level::info);

  FstAlignOption options;
  sub_label_id_ = symbols.Find(options.symSub);
  del_label_id_ = symbols.Find(options.symDel);
  ins_label_id_ = symbols.Find(options.symIns);

  StdVectorFst halfEdit1;
  StdVectorFst halfEdit2;
  halfEdit1.SetInputSymbols(symbols_);
  halfEdit1.SetOutputSymbols(symbols_);
  halfEdit1.AddState();
  halfEdit1.SetStart(0);
  halfEdit1.SetFinal(0, 0);
  halfEdit1.AddArc(0, StdArc(0, ins_label_id_, insertion_cost / 2, 0));
  halfEdit2.SetInputSymbols(symbols_);
  halfEdit2.SetOutputSymbols(symbols_);
  halfEdit2.AddState();
  halfEdit2.SetStart(0);
  halfEdit2.SetFinal(0, 0);
  halfEdit2.AddArc(0, StdArc(del_label_id_, 0, deletion_cost / 2, 0));

  for (SymbolTableIterator siter(*symbols_); !siter.Done(); siter.Next()) {
    int64 sid = siter.Value();
    if (sid != 0 && sid != ins_label_id_ && sid != del_label_id_ && sid != sub_label_id_) {
      auto sym_tk = symbols_->Find(sid);
      bool isClassLabel = isEntityLabel(sym_tk);

      if (isClassLabel) {
        logger_->trace("Token class label found for {}", symbols_->Find(sid));
        // we have a label entry/exit arc
        // it can only be deleted at no cost
        halfEdit1.AddArc(0, StdArc(sid, sid, 0, 0));
        // setting a negative cost will cancel the positive cost from the self-loop.
        // this will make "deleting" the class labels arcs a no-op and
        // won't cause the algo to interfere with the sub/del/ins logic
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
  }

  StdVectorFst detRefFst;
  // fstA is assumed to be the reference FST
  Determinize(fstA, &detRefFst);
  logger_->info("detRefFst has {}", detRefFst.NumStates());

  // step 3: compose
  StdVectorFst halfCompose1;
  logger_->info("compose input1 o halfEdit1");
  // Compose(refFst, halfEdit1, &halfCompose1);
  Compose(detRefFst, halfEdit1, &halfCompose1);

  logger_->info("halfCompose1 has {} states", halfCompose1.NumStates());
  if (halfCompose1.NumStates() == 0) {
    logger_->warn("halfCompose1 (ref o edits) produced an FST with 0 state");
    logger_->warn("halEdit1 was:");
    printFst("fstalign", &halfEdit1, symbols_);
    return;
  }

  ArcSort(&halfCompose1, StdOLabelCompare());

  if (halfCompose1.NumStates() < 100) {
    printFst("fstalign", &halfCompose1, symbols_);
  }

  StdVectorFst halfCompose2;
  logger_->info("compose halfEdit2 o input2");
  // fstB is assumed to be the hypothesis fst
  Compose(halfEdit2, fstB, &halfCompose2);
  logger_->info("halfCompose2 has {} states", halfCompose2.NumStates());
  if (halfCompose2.NumStates() == 0) {
    logger_->warn("halfCompose2 (hyp o edits) produced an FST with 0 state");
    logger_->warn("halEdit2 was:");
    printFst("fstalign", &halfEdit2, symbols_);
    return;
  }

  ArcSort(&halfCompose2, StdILabelCompare());

  if (halfCompose2.NumStates() < 100) {
    printFst("fstalign", &halfCompose2, symbols_);
  }

  logger_->info("performing lazy composition");
  fstC_ = new fst::StdComposeFst(halfCompose1, halfCompose2);

  // initialize internal stores.  if we don't initialize the state iterator
  // (even if we don't really use it) then any call to ArcIterator(fst,
  // state_no) or fst.Final() will throw an exception
  StateIterator<fst::StdFst> siter(*fstC_);
}

StateId StandardCompositionFst::Start() { return (*fstC_).Start(); }

fst::Fst<fst::StdArc>::Weight StandardCompositionFst::Final(StateId stateId) { return (*fstC_).Final(stateId); }

bool StandardCompositionFst::TryGetArcsAtState(StateId fromStateId, vector<fst::StdArc> *out_vector) {
  assert(out_vector != NULL);

  for (ArcIterator<StdFst> aiter(*fstC_, fromStateId); !aiter.Done(); aiter.Next()) {
    const fst::StdArc &arc = aiter.Value();
    out_vector->push_back(arc);
  }

  return true;
}

StandardCompositionFst::~StandardCompositionFst() {
  if (fstC_ != NULL) {
    delete fstC_;
  }
}

void StandardCompositionFst::DebugComposedGraph(string debug_filename) {
  StdVectorFst composedFst(*fstC_);
  ofstream outfile(debug_filename);
  FstWriteOptions wopts;
  composedFst.SetInputSymbols(symbols_);
  composedFst.SetOutputSymbols(symbols_);
  wopts.write_isymbols = true;
  wopts.write_osymbols = true;
  wopts.write_header = true;
  composedFst.Write(outfile, wopts);
}
