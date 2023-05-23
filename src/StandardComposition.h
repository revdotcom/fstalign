/*
 *
 * StandardComposition.h
 *
 * JP Robichaud (jp@rev.com)
 * 2021
 *
 */

#ifndef __STANDARDCOMPOSITION_H__
#define __STANDARDCOMPOSITION_H__

#include <fst/fstlib.h>
#include <unordered_map>
#include <utility>

#include "IComposition.h"
#include "utilities.h"

/*
 * Calculates edit distance between two FSTs through two-step composition.
 * First, the reference FST is composed with all possible reference transformations (<sub>, <del>).
 * Second, the hypothesis FST is composed with all possible hypothesis transformations (<sub>, <ins>).
 * Then the two FSTs are composed using the standard OpenFST lazy composition.
 */
class StandardCompositionFst : public IComposition {
 protected:
  // Lazily composed fst, created during initialization
  std::unique_ptr<fst::StdComposeFst> fstC_;

 public:
  StandardCompositionFst(const fst::StdFst &fstA, const fst::StdFst &fstB);
  StandardCompositionFst(const fst::StdFst &fstA, const fst::StdFst &fstB, SymbolTable &symbols);
  ~StandardCompositionFst();

  StateId Start();
  fst::Fst<fst::StdArc>::Weight Final(StateId stateId);
  vector<fst::StdArc> GetArcsAtState(StateId fromStateId);
  virtual bool TryGetArcsAtState(StateId fromStateId, vector<fst::StdArc> *out_vector);

  /* useful for debugging *SMALL* graphs, performs full (non-lazy) composition */
  void DebugComposedGraph(string debug_filename);
};

#endif /* __STANDARDCOMPOSITION_H__ */
