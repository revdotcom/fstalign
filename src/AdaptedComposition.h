/*
AdaptedComposition.h
 JP Robichaud (jp@rev.com)
 2021

*/

#ifndef __ADAPTEDCOMPOSITION_H__
#define __ADAPTEDCOMPOSITION_H__

#include <fst/fstlib.h>
#include <unordered_map>
#include <utility>
#include "IComposition.h"
#include "utilities.h"

using namespace std;

typedef fst::Fst<fst::StdArc>::StateId StateId;

typedef pair<uint32, uint32> StatePair;
struct key_hash : public std::unary_function<StatePair, std::size_t> {
  std::size_t operator()(const StatePair &k) const { return std::get<0>(k) ^ std::get<1>(k); }
};

// A hash function used to hash a pair of any kind, useful for unordered_map
struct hash_pair {
  template <class T1, class T2>
  size_t operator()(const pair<T1, T2> &p) const {
    auto hash1 = hash<T1>{}(p.first);
    auto hash2 = hash<T2>{}(p.second);
    return hash1 ^ hash2;
  }
};

/*
 * Calculates edit distance between two FSTs through manual single-step composition.
 * Optimizes the search space of the composed graph by greedily expanding composition states.
 * It is notably faster than the StandardCompositionFst alternative.
 * (in beta)
 */
class AdaptedCompositionFst : public IComposition {
 protected:
  map<StatePair, StateId> composed_states;
  map<StateId, StatePair> reversed_composed_states;

  set<pair<StateId, int>> entity_exit_states;

  StateId current_composed_next_state_id = 0;

  fst::SymbolTable *symbols_;
  std::vector<bool> synonyms_label_ids;
  std::vector<bool> entity_label_ids;

  int dbg_count = 0;

  // possible optimizations : limit to const FST or limit to StdVectorFst
  const fst::StdFst &fstA_;
  const fst::StdFst &fstB_;

  StateId GetOrCreateComposedState(StateId a, StateId b);
  bool IsEntityLabel(int labelId);
  bool IsSynonymLabel(int labelId);
  bool IsEntityReacheable(int target_entity_label_id, StateId refA, StateId refB);

 public:
  AdaptedCompositionFst(const fst::StdFst &fstA, const fst::StdFst &fstB);
  AdaptedCompositionFst(const fst::StdFst &fstA, const fst::StdFst &fstB, SymbolTable &symbols);
  ~AdaptedCompositionFst();

  StateId Start();
  fst::Fst<fst::StdArc>::Weight Final(StateId stateId);
  bool TryGetArcsAtState(StateId fromStateId, vector<fst::StdArc> *out_vector);

  // a and b are in the incoming graph referencials
  bool DoesComposedStateExist(StateId a, StateId b);

  // a is in the composed-graph referencial
  bool DoesComposedStateExist(StateId a);

  void SetSymbols(fst::SymbolTable *symbols);

  void DebugComposedGraph();
};

#endif