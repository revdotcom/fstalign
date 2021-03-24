/*
ICompostion.h
 JP Robichaud (jp@rev.com)
 2021

  Custom interface to encapsulate various composition strategies

*/

#ifndef __ICOMPOSITION_H_
#define __ICOMPOSITION_H_

#include "utilities.h"
typedef fst::Fst<fst::StdArc>::StateId StateId;

class IComposition : public fst::VectorFst<fst::StdArc> {
 protected:
  float insertion_cost = 1;
  float deletion_cost = 1;
  float substitution_cost = 1.5;
  std::shared_ptr<spdlog::logger> logger_;

  fst::SymbolTable *symbols_;

  // TODO: make this settable/configurable
  int ins_label_id_ = 1;
  int del_label_id_ = 2;
  int sub_label_id_ = 3;

 public:
  IComposition() {}
  IComposition(const fst::StdFst &fstA, const fst::StdFst &fstB) {}
  IComposition(const fst::StdFst &fstA, const fst::StdFst &fstB, SymbolTable &symbols) {}

  virtual ~IComposition() {}
  virtual StateId Start() = 0;
  virtual fst::Fst<fst::StdArc>::Weight Final(StateId stateId) = 0;
  virtual bool TryGetArcsAtState(StateId fromStateId, vector<fst::StdArc> *out_vector) = 0;
};

#endif /*__ICOMPOSITION_H_ */
