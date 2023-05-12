/*
Walker.cpp
 JP Robichaud (jp@rev.com)
 2018

*/
#include "Walker.h"

#include "utilities.h"

using namespace std;
using namespace fst;

Walker::Walker() : heapA(&_heapA), heapB(&_heapB) {
  logger = logger::GetOrCreateLogger("walker");
}

vector<wer_alignment> Walker::walkComposed(IComposition &fst, SymbolTable &symbol, FstAlignOption &options,
                                                       int numBests) {
  logger->info("starting a walk in the park");

  vector<wer_alignment> topAlignments;
  // initialize internal stores.  if we don't initialize the state iterator
  // (even if we don't really use it) then any call to ArcIterator(fst,
  // state_no) or fst.Final() will throw an exception
  fst.Start();
  StateIterator<fst::StdFst> siter(fst);
  vector<ShortlistEntry> topEntries;
  set<int> visited_states;

  // starting from 1st node
  auto firstEntry = make_shared<ShortlistEntry>();

  firstEntry->currentState = 0;
  firstEntry->costSoFar = 0;
  firstEntry->costToGoThere = 0;
  firstEntry->whereTo = 0;
  firstEntry->numErrors = 0;
  firstEntry->numInsert = 0;
  firstEntry->numWords = 0;
  firstEntry->linkToHere = nullptr;

  heapA->insert(firstEntry);

  int loopSinceLastPruning = 0;
  int loopCount = 0;
  int last1kStage = 0;
  while (heapA->size() > 0 && topEntries.size() < numBests) {
    loopCount++;
    auto currentState_ptr = heapA->removeFirst();
    auto currentState = *currentState_ptr;
    int s = currentState.currentState;
    visited_states.insert(s);

    if (currentState.numWords / 1000 > last1kStage) {
      logger->info("approx. {} words processed", currentState.numWords);
      last1kStage = currentState.numWords / 1000;
    }

    int arcsLeaving = 0;
    vector<StdArc> arcs_leaving_state;
    if (!fst.TryGetArcsAtState(s, &arcs_leaving_state)) {
      logger->error("no arcs leaving state {}", s);
      continue;
    }

    for (vector<StdArc>::iterator iter = arcs_leaving_state.begin(); iter != arcs_leaving_state.end(); ++iter) {
      const fst::StdArc arc = *iter;
      if (arc.nextstate == s) {
        // if we're pointing to ourselves, let's ignore that
        continue;
      }

      arcsLeaving++;

      // let's reduce our dependency of fstarc objects
      auto local_arc_ptr = make_shared<MyArc>();
      local_arc_ptr->ilabel = arc.ilabel;
      local_arc_ptr->olabel = arc.olabel;
      local_arc_ptr->nextstate = arc.nextstate;
      local_arc_ptr->weight = arc.weight.Value();

      bool isAnchor = false;
      auto pp = enqueueIfNeeded(currentState_ptr, local_arc_ptr, isAnchor);

      if (pp != nullptr) {
        heapB->insert(pp);
      }
    }

    bool isFinal = fst.Final(s) != StdFst::Weight::Zero() ? true : false;
    if (isFinal) {
      double localWer = (double)currentState.numErrors / (double)currentState.numWords;
      logger->info("we reached a {}node with a wer of {}", isFinal ? "final " : "non-final! ", localWer);
      topEntries.push_back(currentState);
    }

    if (heapA->size() > 0) {
      // we still have some stuff to do with the current heap
      continue;
    }

    if (heapB->size() > 0) {
      if (loopSinceLastPruning >= numberOfLoopsBeforePruning) {
        SLE a = heapB->GetBestWerCandidate().get();
        heapB->prune(this->pruningHeapSizeTarget); 
        SLE b = heapB->GetBestWerCandidate().get();

        if (logger->should_log(spdlog::level::debug)) {
          logger->debug(
              "best wer before pruning = {} ({} errors, {} words, {} state "
              "visited)",
              (float)a->numErrors / (float)a->numWords, a->numErrors, a->numWords, visited_states.size());
          logger->debug(
              "best wer after pruning  = {} ({} errors, {} words, {} state "
              "visited)",
              (float)b->numErrors / (float)b->numWords, b->numErrors, b->numWords, visited_states.size());
        }
        loopSinceLastPruning = 0;
      }
    }
    loopSinceLastPruning++;

    // let's switch heaps
    auto heapTmp = heapA;
    heapA = heapB;
    heapB = heapTmp;
  }

  logger->info("we have {} candidates after {} loops", topEntries.size(), loopCount);
  if (topEntries.size() > 0) {
    int i = 0;
    for (auto &top : topEntries) {
      logger->info("getting details for candidate {}", i);
      // auto top = topEntries[0];
      auto align = GetDetailsFromTopCandidates(top, symbol, options);
      topAlignments.push_back(align);
      i++;
    }
  }

  return topAlignments;
}

std::shared_ptr<ShortlistEntry> Walker::enqueueIfNeeded(std::shared_ptr<ShortlistEntry> currentState,
                                                        shared_ptr<MyArc> arc_ptr, bool isAnchor) {
  shared_ptr<ShortlistEntry> enqueued = nullptr;

  auto arc = *arc_ptr;
  int target_state = arc.nextstate;

  if (target_state == currentState->currentState) {
    // we don't loop to ourselve, period...
    return enqueued;
  }

  bool enqueue = false;
  auto found = logbook.find(target_state);
  if (found == logbook.end()) {
    // we couldn't find a shortlist entry in the logbook, we'll have to create
    // one
    enqueue = true;
  } else {
    float oldCost = found->second;

    // should that just be > instead of >= ?
    if (oldCost >= currentState->costSoFar + arc.weight) {
      enqueue = true;
      //   logbook.erase(found);
    }
  }

  if (!enqueue) {
    return enqueued;
  }

  // we have decided that we needed to enqueue this new shortlist
  enqueued = make_shared<ShortlistEntry>();
  enqueued->currentState = target_state;
  enqueued->linkToHere = currentState;

  // reaching an anchor means having a cost of 0
  auto arcCost = arc.weight;

  enqueued->costToGoThere = isAnchor ? 0 : arcCost;
  enqueued->costSoFar = currentState->costSoFar + enqueued->costToGoThere;
  // enqueued->arcToGoThere = *arc_ptr;

  if (!isAnchor) {
    enqueued->numWords = currentState->numWords + 1;
    enqueued->numErrors = currentState->numErrors;
    if (arcCost > 0) {
      enqueued->numErrors++;
      if (arc.ilabel == 0) {
        enqueued->numInsert++;
      }
    }
  } else {
    // since we are at an anchor word, we don't increase the error count
    enqueued->numWords = currentState->numWords;
  }

  // let's be mindfull of how allocations are made
  enqueued->local_arc = arc_ptr;

  logbook[enqueued->currentState] = enqueued->costSoFar;

  return enqueued;
}

wer_alignment Walker::GetDetailsFromTopCandidates(ShortlistEntry &currentState, SymbolTable &symbol,
                                                              FstAlignOption &options) {
  logger->debug("GetDetailsFromTopCandidates()");
  // it's an approx wer because numWords is actually the number of arcs we
  // traversed, not the number of words in the reference.  We'll get to that.
  float approx_wer = (float)currentState.numErrors / (float)currentState.numWords;

  // we'll try to recover the exact amount of words chosen
  int numWordsInReference = 0;

  wer_alignment global_wer_alignment;

  // could be ambiguous if we have a lattice instead of a
  // flat list of words from CTM for example
  int numWordsInHypothesis = 0;

  MyArc arc;
  shared_ptr<ShortlistEntry> prev;
  SLE now = &currentState;

  std::unordered_set<int> special_symbols = {options.eps_idx, options.del_idx, options.ins_idx, options.sub_idx,
                                             options.oov_idx};

  spWERA class_label_wer_info = nullptr;

  while (now != nullptr && now->local_arc != nullptr) {
    auto local_arc = *(now->local_arc);
    string ilabel = symbol.Find(local_arc.ilabel);
    string olabel = symbol.Find(local_arc.olabel);

    bool isClassLabel_i = isEntityLabel(ilabel);
    // bool isClassLabel_o = olabel.find("___") == 0 ? true : false;

    if (logger->should_log(spdlog::level::trace)) {
      logger->trace("we have {}/{} with a weight of {}", ilabel, olabel, local_arc.weight);
    }

    if (isClassLabel_i) {
      if (class_label_wer_info == nullptr) {
        // we are entring a class label
        class_label_wer_info = shared_ptr<wer_alignment>(new wer_alignment());
        class_label_wer_info->classLabel = ilabel;
        global_wer_alignment.label_alignments.emplace_back(std::move(*class_label_wer_info));
        global_wer_alignment.tokens.push_back(make_pair(ilabel, olabel));
      } else if (ilabel == class_label_wer_info->classLabel) {
        // we're leaving a class label section
        class_label_wer_info = nullptr;
      }
      // Ignore nested classes.
      // Impossible to have overlap between synonyms and class labels, so we'll
      // just always favor the outermost label.

      now = now->linkToHere.get();
      continue;
    }

    /*
- if arc.ilabel == 0 and arc.olabel != 0 --> this is an insertion.  olabel
was in hyp and not in ref
- if arc.olabel == 0 and arc.ilabel != 0 --> this is a  deletion.  ilabel
was in ref and not in hyp
- if arc.ilabel != arc.olabel, we have substitution: ilabel was in rev,
olabel was in hyp
*/

    if (local_arc.ilabel != local_arc.olabel) {
      if (local_arc.ilabel == 0) {
        global_wer_alignment.insertions++;
        global_wer_alignment.numWordsInHypothesis++;

        global_wer_alignment.hyp_words.push_back(olabel);
        global_wer_alignment.ref_words.push_back(INS);

        // keep track of the attractors
        global_wer_alignment.ins_words.push_back(olabel);

        if (class_label_wer_info != nullptr) {
          class_label_wer_info->insertions++;
          class_label_wer_info->numWordsInHypothesis++;

          class_label_wer_info->hyp_words.push_back(olabel);
          class_label_wer_info->ref_words.push_back(INS);

          // keep track of the attractors
          class_label_wer_info->ins_words.push_back(olabel);
          class_label_wer_info->tokens.push_back(make_pair(INS, olabel));
        } else {
          global_wer_alignment.tokens.push_back(make_pair(INS, olabel));
        }
      } else if (local_arc.olabel == 0) {
        global_wer_alignment.deletions++;
        global_wer_alignment.numWordsInReference++;
        global_wer_alignment.ref_words.push_back(ilabel);
        global_wer_alignment.hyp_words.push_back(DEL);

        // keep track of the repellant words
        global_wer_alignment.del_words.push_back(ilabel);

        if (class_label_wer_info != nullptr) {
          class_label_wer_info->deletions++;
          class_label_wer_info->numWordsInReference++;
          class_label_wer_info->ref_words.push_back(ilabel);
          class_label_wer_info->hyp_words.push_back(DEL);

          // keep track of the repellant words
          class_label_wer_info->del_words.push_back(ilabel);
          class_label_wer_info->tokens.push_back(make_pair(ilabel, DEL));
        } else {
          global_wer_alignment.tokens.push_back(make_pair(ilabel, DEL));
        }
      } else {
        global_wer_alignment.substitutions++;
        global_wer_alignment.numWordsInReference++;
        global_wer_alignment.numWordsInHypothesis++;

        global_wer_alignment.ref_words.push_back(ilabel);
        global_wer_alignment.hyp_words.push_back(olabel);

        std::pair<string, string> pair;
        pair = std::make_pair(ilabel, olabel);
        global_wer_alignment.sub_words.push_back(pair);

        if (class_label_wer_info != nullptr) {
          class_label_wer_info->substitutions++;
          class_label_wer_info->numWordsInReference++;
          class_label_wer_info->numWordsInHypothesis++;

          class_label_wer_info->ref_words.push_back(ilabel);
          class_label_wer_info->hyp_words.push_back(olabel);

          class_label_wer_info->sub_words.push_back(pair);
          class_label_wer_info->tokens.push_back(pair);
        } else {
          global_wer_alignment.tokens.push_back(pair);
        }
      }
    } else if (!isClassLabel_i && special_symbols.find(local_arc.ilabel) == special_symbols.end() &&
               special_symbols.find(local_arc.olabel) == special_symbols.end()) {
      std::pair<string, string> pair;
      pair = std::make_pair(ilabel, olabel);
      global_wer_alignment.numWordsInHypothesis++;
      global_wer_alignment.numWordsInReference++;

      global_wer_alignment.ref_words.push_back(ilabel);
      global_wer_alignment.hyp_words.push_back(olabel);

      if (class_label_wer_info != nullptr) {
        class_label_wer_info->numWordsInHypothesis++;
        class_label_wer_info->numWordsInReference++;

        class_label_wer_info->ref_words.push_back(ilabel);
        class_label_wer_info->hyp_words.push_back(olabel);

        class_label_wer_info->tokens.push_back(pair);
      } else {
        global_wer_alignment.tokens.push_back(pair);
      }
    }

    now = now->linkToHere.get();
  }

  logger->info("approx WER was {}, real WER is {}", approx_wer,
               (float)(global_wer_alignment.insertions + global_wer_alignment.deletions +
                       global_wer_alignment.substitutions) /
                   (float)global_wer_alignment.numWordsInReference);

  // for now, everything is backward, let's proceed to reverse all vectors so that we are returning texts in the natural
  // order

  global_wer_alignment.Reverse();
  return global_wer_alignment;
}
