/*
AdaptedComposition.cpp
 JP Robichaud (jp@rev.com)
 2021

*/

#define TRACE false

#include "AdaptedComposition.h"
#include <chrono>
#include <ctime>
#include "logging.h"

AdaptedCompositionFst::AdaptedCompositionFst(const fst::StdFst &fstA, const fst::StdFst &fstB)
    : fstA_{fstA}, fstB_{fstB}, symbols_{NULL} {
  logger_ = logger::GetOrCreateLogger("AdaptedCompositionFst");
  logger_->set_level(spdlog::level::info);
}

AdaptedCompositionFst::AdaptedCompositionFst(const fst::StdFst &fstA, const fst::StdFst &fstB, SymbolTable &symbols)
    : fstA_{fstA}, fstB_{fstB} {
  logger_ = logger::GetOrCreateLogger("AdaptedCompositionFst");
  logger_->set_level(spdlog::level::info);
  SetSymbols(&symbols);

  FstAlignOption options;
  sub_label_id_ = symbols.Find(options.symSub);
  del_label_id_ = symbols.Find(options.symDel);
  ins_label_id_ = symbols.Find(options.symIns);
}

AdaptedCompositionFst::~AdaptedCompositionFst() {}

bool AdaptedCompositionFst::IsEntityLabel(int labelId) {
  if (symbols_ != NULL) {
    return entity_label_ids[labelId];
  }
  return false;
}

bool AdaptedCompositionFst::IsSynonymLabel(int labelId) {
  if (symbols_ != NULL) {
    return synonyms_label_ids[labelId];
  }
  return false;
}

StateId AdaptedCompositionFst::Start() {  // add initialization code for the composition.  We should already have
                                          // composed the state (0, 0)

  StateId zeroA = fstA_.Start();
  StateId zeroB = fstB_.Start();

  StateId zeroC = GetOrCreateComposedState(zeroA, zeroB);

  return zeroC;
}

// incoming graphs referentials... could end up a preprocessor macro...
bool AdaptedCompositionFst::DoesComposedStateExist(StateId a, StateId b) {
  StatePair tuple = make_pair(a, b);
  auto itr = composed_states.find(tuple);
  if (itr == composed_states.end()) {
    return false;
  } else {
    return true;
  }
}

// composed-graph referential, could end up a preprocessor macro...
bool AdaptedCompositionFst::DoesComposedStateExist(StateId a) {
  if (TRACE) {
    logger_->trace("composed_state size {}, reverse_composed_state size {}, checking for {}", composed_states.size(),
                   reversed_composed_states.size(), a);
  }

  auto itr = reversed_composed_states.find(0);
  if (itr == reversed_composed_states.end()) {
    return false;
  } else {
    return true;
  }
}

// protected
StateId AdaptedCompositionFst::GetOrCreateComposedState(StateId a, StateId b) {
  if (TRACE) {
    logger_->trace("get_or_create_for {},{}, composed_state size {}", a, b, composed_states.size());
  }
  StatePair state_pair = make_pair(a, b);
  auto itr = composed_states.find(state_pair);
  if (itr == composed_states.end()) {
    if (TRACE) {
      logger_->trace("pair {},{} not found", a, b);
    }
    // the key was not found, let's insert a new state
    // composed_states[tuple] = current_composed_next_state_id++;
    StateId new_state_id = current_composed_next_state_id++;
    composed_states.emplace(state_pair, new_state_id);
    reversed_composed_states.emplace(new_state_id, state_pair);
    if (TRACE) {
      logger_->trace("returning new state {}", new_state_id);
    }

    return new_state_id;
  } else {
    auto ret = itr->second;
    if (TRACE) {
      logger_->trace("returning existing state {}", ret);
    }
    return ret;
  }
}

fst::Fst<fst::StdArc>::Weight AdaptedCompositionFst::Final(StateId stateId) {
  if (!DoesComposedStateExist(stateId)) {
    logger_->error("asking if non-defined state {} is final, returning that it is", stateId);
    return fst::Fst<fst::StdArc>::Weight::One();
  }

  auto ref_state_pair = reversed_composed_states[stateId];
  StateId refA = ref_state_pair.first;
  StateId refB = ref_state_pair.second;

  bool fstA_is_final = (fstA_.Final(refA) != fst::Fst<fst::StdArc>::Weight::Zero());
  bool fstB_is_final = (fstB_.Final(refB) != fst::Fst<fst::StdArc>::Weight::Zero());

  if (fstA_is_final && fstB_is_final) {
    return fst::Fst<fst::StdArc>::Weight::One();
  }

  return fst::Fst<fst::StdArc>::Weight::Zero();
}

/* this method assumes a wellformed reference fst where for every "enter-sign" entity label-id
we also have the corresponding "exit-sign" label-id.
For example, in the graph below, we enter the ___100000_SYN_1-2___ class on state 0
and we exit it on state 20 with an arc leaving state 20 to come back to state 1

0       1       i'm     i'm
0       18      ___100000_SYN_1-2___    ___100000_SYN_1-2___
1       2       not     not
2       3       sure    sure
3       4       i       i
4       5       want    want
4       21      ___100001_SYN_2-1___    ___100001_SYN_2-1___
5       6       to      to
6       7       spend   spend
7       8       ___1_MONEY___   ___1_MONEY___
8       11      fifty   fifty
8       12      fifty   fifty
8       14      50$     50$
9       10      ___1_MONEY___   ___1_MONEY___
10      15      on      on
11      9       <eps>   <eps>
12      13      dollars dollars
13      9       <eps>   <eps>
14      9       <eps>   <eps>
15      16      this    this
16      17      <eps>   <eps>
17      1
18      19      i       i
19      20      am      am
20      1       ___100000_SYN_1-2___    ___100000_SYN_1-2___
21      22      wanna   wanna
22      6       ___100001_SYN_2-1___    ___100001_SYN_2-1___

*/
bool AdaptedCompositionFst::IsEntityReacheable(int target_entity_label_id, StateId refA, StateId refB) {
  for (ArcIterator<StdFst> aiter(fstA_, refA); !aiter.Done(); aiter.Next()) {
    const fst::StdArc &arcA = aiter.Value();
    // logger_->info("{}:{} asking if {}/{} is reacheable from state {}", __FILE__ , __LINE__, target_entity_label_id,
    // symbols_->Find(target_entity_label_id), refA);

    if (arcA.olabel == target_entity_label_id) {
      // we found what we were looking for!
      entity_exit_states.emplace(refA, target_entity_label_id);
      return true;
    }

    if (arcA.olabel == 0) {
      // special case for eps transitions
      bool does_this_reach_the_end = IsEntityReacheable(target_entity_label_id, arcA.nextstate, refB);
      if (does_this_reach_the_end) {
        return true;
      }

      continue;
    }

    for (ArcIterator<StdFst> aiterB(fstB_, refB); !aiterB.Done(); aiterB.Next()) {
      const fst::StdArc &arcB = aiterB.Value();

      if (arcA.olabel == arcB.ilabel) {
        bool does_this_reach_the_end = IsEntityReacheable(target_entity_label_id, arcA.nextstate, arcB.nextstate);
        if (does_this_reach_the_end) {
          return true;
        }
      }
    }
  }

  return false;
}

bool AdaptedCompositionFst::TryGetArcsAtState(StateId fromStateId, vector<fst::StdArc> *out_vector) {
  assert(out_vector != NULL);

  if (TRACE) {
    logger_->trace(HERE_FMT "Entering TryGetArcsAtState()", HEREF2);
  }

  if (!DoesComposedStateExist(fromStateId)) {
    logger_->warn("composed_state size {}, reverse_composed_state size {}", composed_states.size(),
                  reversed_composed_states.size());
    logger_->warn("requested composed state [{}] doesn't exist", fromStateId);
    return false;
  } else {
    if (TRACE) {
      logger_->trace("desired composed state {} found", fromStateId);
    }
  }

  auto ref_state_pair = reversed_composed_states[fromStateId];
  StateId refA = ref_state_pair.first;
  StateId refB = ref_state_pair.second;

  // this is very naive and unoptimized for now
  int total_match = 0;
  auto num_ref_labels = fstA_.NumArcs(refA);

  auto here_time = std::chrono::system_clock::now();
  std::time_t here_snap = std::chrono::system_clock::to_time_t(here_time);
  dbg_count++;

  int num_match = 0;
  int num_entity = 0;

  for (ArcIterator<StdFst> aiter(fstA_, refA); !aiter.Done(); aiter.Next()) {
    const fst::StdArc &arcA = aiter.Value();

#if TRACE
    logger_->trace("{}/{} >] arcA.olabel = {}/{}", dbg_count, here_snap, arcA.olabel, symbols_->Find(arcA.olabel));
    logger_->trace("{}/{} >] state {} arcA.olabel = {}/{}", dbg_count, here_snap, refA, arcA.olabel,
                   symbols_->Find(arcA.olabel));
#endif

    // self-referencing arcs will be skipped
    if (arcA.nextstate == refA) {
      continue;
    }

    // <eps>
    // TODO: we should also prepare ourselves in case the hyp graph has ε transitions too
    if (arcA.olabel == 0) {
      StateId skip_eps_state_ref_id = GetOrCreateComposedState(arcA.nextstate, refB);
      out_vector->push_back(StdArc(0, 0, 0.0, skip_eps_state_ref_id));
      continue;
    }

    bool is_synonym = IsSynonymLabel(arcA.olabel);
    bool is_entity = is_synonym || IsEntityLabel(arcA.olabel);

    // ok, so the reference label
    if (is_entity) {
      bool is_exit_state = entity_exit_states.find(make_pair(refA, (int)arcA.olabel)) != entity_exit_states.end();

#if TRACE
      if (is_exit_state) {
        logger_->trace("we reached the exit state of {}", symbols_->Find(arcA.olabel));
      }
#endif
      // for now, we enforce reacheability only for synonyms
      if (is_exit_state || !is_synonym || IsEntityReacheable(arcA.olabel, arcA.nextstate, refB)) {
        // we have an arc like __SYN-1__ or __MONEY-1__
        num_entity++;
        num_match++;
        // let's keep the weight to 0, this isn't an error
        StateId del_state_ref_id = GetOrCreateComposedState(arcA.nextstate, refB);
        out_vector->push_back(StdArc(arcA.ilabel, del_label_id_, 0.0, del_state_ref_id));
      } else {
        // skipping this path already since we can't reach the end of it without deletions or insertions or
        // substitutions
        if (TRACE) {
          logger_->trace("skipping path for {} as we won't reach the end from ({}, {})", symbols_->Find(arcA.olabel),
                         refA, refB);
        }
      }

      continue;
    }

    for (ArcIterator<StdFst> aiterB(fstB_, refB); !aiterB.Done(); aiterB.Next()) {
      const fst::StdArc &arcB = aiterB.Value();

      // we have a matching label
      if (arcA.olabel == arcB.ilabel) {
        num_match++;

        StateId c = GetOrCreateComposedState(arcA.nextstate, arcB.nextstate);
        out_vector->push_back(StdArc(arcA.ilabel, arcB.olabel, 0.0, c));
      }

      if (TRACE) {
        logger_->trace("{}/{} >] for {}/{} vs {}/{}, we have num_match {} and num_entity {}", dbg_count, here_snap,
                       arcA.olabel, symbols_->Find(arcA.olabel), arcB.ilabel, symbols_->Find(arcB.ilabel), num_match,
                       num_entity);
      }

      if (num_match == 0) {
        // if (num_match == 0 || num_match == num_entity) {
        // this could be an insertion, this could be a substitution
        // TODO: we can be more clever here
        StateId ins_state_ref_id = GetOrCreateComposedState(refA, arcB.nextstate);
        StateId sub_state_ref_id = GetOrCreateComposedState(arcA.nextstate, arcB.nextstate);
#if TRACE
        logger_->trace("{}/{} adding ins/{}/{}", dbg_count, here_snap, arcB.olabel, symbols_->Find(arcB.olabel));
        logger_->trace("{}/{} adding sub/{}/{}", dbg_count, here_snap, arcA.ilabel, arcB.olabel);
#endif
        // out_vector->push_back(StdArc(ins_label_id, arcB.olabel, insertion_cost, ins_state_ref_id));
        out_vector->push_back(StdArc(0, arcB.olabel, insertion_cost, ins_state_ref_id));
        out_vector->push_back(StdArc(arcA.ilabel, arcB.olabel, substitution_cost, sub_state_ref_id));
      } else {
#if TRACE
        logger_->trace("a label match was found, not putting ins/sub arcs");
#endif
      }
    }

    if (num_match == 0) {
      // if (num_match == 0 || num_match == num_entity) {
      // let's add a potential deletion
      // TODO: we can be more clever here
      // out_vector->push_back(StdArc(arcA.ilabel, del_label_id, deletion_cost, del_state_ref_id));
      StateId del_state_ref_id = GetOrCreateComposedState(arcA.nextstate, refB);
      out_vector->push_back(StdArc(arcA.ilabel, 0, deletion_cost, del_state_ref_id));
      if (TRACE) {
        logger_->trace("{}/{} adding del/{}/{}", dbg_count, here_snap, arcA.ilabel, symbols_->Find(arcA.ilabel));
      }
    } else {
      if (TRACE) {
        logger_->trace("a label match was found, not putting del arc");
      }
    }
  }

  if (fstA_.NumArcs(refA) == 0) {
    // we reached the end of the A graph, but what about B?
    for (ArcIterator<StdFst> aiterB(fstB_, refB); !aiterB.Done(); aiterB.Next()) {
      const fst::StdArc &arcB = aiterB.Value();

      StateId ins_state_ref_id = GetOrCreateComposedState(refA, arcB.nextstate);
      // out_vector->push_back(StdArc(ins_label_id, arcB.olabel, insertion_cost, ins_state_ref_id));
      out_vector->push_back(StdArc(0, arcB.olabel, insertion_cost, ins_state_ref_id));
    }
  }

  return true;
}

void AdaptedCompositionFst::SetSymbols(fst::SymbolTable *symbols) {
  symbols_ = symbols;
  synonyms_label_ids.clear();
  synonyms_label_ids.resize(symbols->NumSymbols(), false);

  entity_label_ids.clear();
  entity_label_ids.resize(symbols->NumSymbols(), false);

  // mostly for optimization purpose
  logger_->info("{}:{} we created 2 vector<bool> of {} items", __FILE__, __LINE__, symbols->NumSymbols());

  for (SymbolTableIterator siter(*symbols); !siter.Done(); siter.Next()) {
    int64 sid = siter.Value();
    auto sym_tk = symbols->Find(sid);

    if (isSynonymLabel(sym_tk)) {
      synonyms_label_ids[sid] = true;
    } else {
      // will mark it as entity only if it isn't a synonym
      if (isEntityLabel(sym_tk)) {
        entity_label_ids[sid] = true;
      }
    }
  }
}

void AdaptedCompositionFst::DebugComposedGraph() {
  // DEBUG CODE : let's save the graph
  // should hide behind a switch or a shell variable
  auto logger = logger::GetOrCreateLogger("debug_composed");
  std::string debug_fst_composed_output_prefix = GetEnv("_COMPOSITION_DBG_PREFIX", "");
  if (debug_fst_composed_output_prefix != "") {
    StdVectorFst copy_fst;
    copy_fst.AddState();
    copy_fst.SetStart(0);
    vector<StateId> states_to_visit;
    set<StateId> state_visited;
    states_to_visit.push_back(this->Start());

    while (states_to_visit.size() > 0) {
      StateId s = states_to_visit.back();
      states_to_visit.pop_back();
      if (state_visited.find(s) != state_visited.end()) {
        logger->trace("already visited state {}", s);
        continue;
      }

      // mark this state as visited
      state_visited.insert(s);

      vector<StdArc> arcs_leaving_state;
      if (!this->TryGetArcsAtState(s, &arcs_leaving_state)) {
        logger->trace("no arcs leaving state {}, setting Final() weight", s);
        copy_fst.SetFinal(s, this->Final(s));
        continue;
      }

      for (vector<StdArc>::iterator iter = arcs_leaving_state.begin(); iter != arcs_leaving_state.end(); ++iter) {
        const fst::StdArc arc = *iter;
        copy_fst.AddArc(s, StdArc(arc.ilabel, arc.olabel, arc.weight, arc.nextstate));
        int num_states_to_add = arc.nextstate - copy_fst.NumStates() + 1;
        if (num_states_to_add > 0) {
          logger->trace("adding {} states", num_states_to_add);
          for (int i = 0; i < num_states_to_add; i++) {
            copy_fst.AddState();
          }
        }
        logger->trace("adding arc from state {} to state {}", s, arc.nextstate);
        if (state_visited.find(arc.nextstate) == state_visited.end()) {
          logger->trace("setting state {} to be visited", s);
          states_to_visit.push_back(arc.nextstate);
        }
      }
    }

    logger->info("copy_fst has {} states", copy_fst.NumStates());
    {
      string composed_fst_filename = fmt::format("{}.composed.fst", debug_fst_composed_output_prefix);
      copy_fst.SetInputSymbols(symbols_);
      copy_fst.SetOutputSymbols(symbols_);
      logger->info("writing copy of composedFst  fst to {}", composed_fst_filename);
      ofstream outfile(composed_fst_filename);
      FstWriteOptions wopts;
      wopts.write_isymbols = true;
      wopts.write_osymbols = true;
      wopts.write_header = true;
      copy_fst.Write(outfile, wopts);
    }
  }  // end of debug code
}