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
#include "fstalign.h"

/*
 * Calculates edit distance between two FSTs through two-step composition.
 * First, the reference FST is composed with all possible reference transformations (<sub>, <del>).
 * Second, the hypothesis FST is composed with all possible hypothesis transformations (<sub>, <ins>).
 * Then the two FSTs are composed using the standard OpenFST lazy composition.
 */
class StandardCompositionFst : public IComposition {
 protected:
  // Lazily composed fst, created during initialization
  std::unique_ptr<fst::Fst<fst::StdArc>> fstC_;
  // Add members to store options
  bool strict_punctuation_ = false;
  std::unordered_set<int> punctuation_ids_;
  // Favored substitutions
  bool use_favored_substitutions_ = false;
  float favored_substitution_cost_ = 0.1f;
  std::vector<int> favorable_substitution_map_;
  const fst::SymbolTable& symbols_; // Store symbols if needed for filtering

 public:
  StandardCompositionFst(const fst::StdFst &fstA, const fst::StdFst &fstB);
  StandardCompositionFst(const fst::StdFst &fstA, const fst::StdFst &fstB, const SymbolTable &symbols);
  StandardCompositionFst(const fst::StdFst &fstA, const fst::StdFst &fstB, const SymbolTable &symbols, const AlignerOptions& options);
  ~StandardCompositionFst();

  StateId Start();
  fst::Fst<fst::StdArc>::Weight Final(StateId stateId);
  virtual bool TryGetArcsAtState(StateId fromStateId, vector<fst::StdArc> *out_vector);

  /* useful for debugging *SMALL* graphs, performs full (non-lazy) composition */
  void DebugComposedGraph(string debug_filename);
};

#endif /* __STANDARDCOMPOSITION_H__ */
