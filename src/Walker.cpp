/*
Walker.cpp
 JP Robichaud (jp@rev.com)
 2018

*/
#include "Walker.h"

#include "utilities.h"
#include <chrono>

using namespace std;
using namespace fst;
using namespace std::chrono;

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

  // Timing variables
  microseconds time_removing_from_heap(0);
  microseconds time_processing_arcs(0);
  microseconds time_pruning(0);
  auto loop_start_time = high_resolution_clock::now();

  while (heapA->size() > 0 && topEntries.size() < numBests) {
    loopCount++;

    // --- Timing: Remove from Heap ---
    auto remove_start = high_resolution_clock::now();
    auto currentState_ptr = heapA->removeFirst();
    auto remove_end = high_resolution_clock::now();
    time_removing_from_heap += duration_cast<microseconds>(remove_end - remove_start);
    // --- End Timing ---

    if (!currentState_ptr) { // Should not happen if heapA->size() > 0, but defensive check
        logger->warn("Removed null pointer from heapA, heap size was {}", heapA->size());
        // continue;
    }
    auto currentState = *currentState_ptr;
    int s = currentState.currentState;
    visited_states.insert(s);

    // --- Log Status Periodically ---
    // Check the flag FIRST
    if (this->enableDetailedWalkerLogging) {
        // Then check the loop count
        if (loopCount % 10000 == 0) { // Log every 10000 loops
            auto now = high_resolution_clock::now();
            auto elapsed_ms = duration_cast<milliseconds>(now - loop_start_time).count();
             logger->info(
                 "[Loop {}k] HeapA: {}, HeapB: {}, Logbook: {}, Visited: {}, Current Words: {}, Elapsed: {}ms",
                 loopCount / 1000, heapA->size(), heapB->size(), logbook.size(), visited_states.size(), currentState.numWords, elapsed_ms);
             logger->info(
                 "[Loop {}k Timings] Remove: {}us, Arcs: {}us, Pruning: {}us",
                 loopCount / 1000, time_removing_from_heap.count(), time_processing_arcs.count(), time_pruning.count());
             // Reset timers
             time_removing_from_heap = microseconds(0);
             time_processing_arcs = microseconds(0);
             time_pruning = microseconds(0);
             loop_start_time = now; // Reset start time for next interval
        }
    }
    // --- End Log Status ---

    if (currentState.numWords / 1000 > last1kStage) {
      logger->info("approx. {} words processed (Loop {})", currentState.numWords, loopCount);
      last1kStage = currentState.numWords / 1000;
       if (this->enableDetailedWalkerLogging) {
         logger->info(
               "[Words {}k Status] HeapA: {}, HeapB: {}, Logbook: {}, Visited: {}",
               last1kStage, heapA->size(), heapB->size(), logbook.size(), visited_states.size());
       }
    }

    // --- Timing: Processing Arcs ---
    auto arcs_proc_start = high_resolution_clock::now();
    int arcsLeaving = 0;
    vector<StdArc> arcs_leaving_state;
    // TODO: Consider timing fst.TryGetArcsAtState separately if suspected bottleneck
    if (!fst.TryGetArcsAtState(s, &arcs_leaving_state)) {
      // This might indicate an issue with the composition FST itself, though less likely the cause of *progressive* slowdown
      logger->warn("No arcs leaving state {} (final={})", s, fst.Final(s) != StdFst::Weight::Zero());
      // Continue processing final state check even if no arcs leave
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
      MyArc local_arc;
      local_arc.ilabel = arc.ilabel;
      local_arc.olabel = arc.olabel;
      local_arc.nextstate = arc.nextstate;
      local_arc.weight = arc.weight.Value();

      bool isAnchor = false; // Assuming isAnchor logic is handled elsewhere or not critical for timing now
      auto pp = enqueueIfNeeded(currentState_ptr, local_arc, isAnchor); // This includes logbook access

      if (pp != nullptr) {
        // Consider timing heapB->insert separately if needed
        heapB->insert(pp);
      }
    }
    auto arcs_proc_end = high_resolution_clock::now();
    time_processing_arcs += duration_cast<microseconds>(arcs_proc_end - arcs_proc_start);
    // --- End Timing ---

    bool isFinal = fst.Final(s) != StdFst::Weight::Zero() ? true : false;
    if (isFinal) {
      // Check if numWords is non-zero to avoid division by zero
      if (currentState.numWords > 0) {
            double localWer = (double)currentState.numErrors / (double)currentState.numWords;
            logger->debug("Reached final node (State {}) with wer {} ({} err / {} words)", s, localWer, currentState.numErrors, currentState.numWords);
      } else {
            logger->debug("Reached final node (State {}) with 0 words", s);
      }
      topEntries.push_back(currentState);
      // Sort topEntries if we want to stop early once numBests good paths are found? (Currently waits until heapA is empty)
      // std::sort(topEntries.begin(), topEntries.end(), [](const ShortlistEntry& a, const ShortlistEntry& b){ /* compare WER */ });
    }

    if (heapA->size() > 0) {
      // we still have some stuff to do with the current heap
      continue;
    }

    // HeapA is empty, check heapB and potentially prune/swap
    if (heapB->size() > 0) {
        // --- Timing: Pruning ---
        auto prune_start = high_resolution_clock::now();
        bool did_prune = false;
        if (loopSinceLastPruning >= numberOfLoopsBeforePruning) {
            did_prune = true;
            size_t size_before = heapB->size();
            // Optional: Get best WER before pruning for logging comparison
            // shared_ptr<ShortlistEntry> best_before_ptr = heapB->GetBestWerCandidate();
            // SLE best_before = best_before_ptr ? best_before_ptr.get() : nullptr;

            // *** CHOOSE PRUNING STRATEGY ***
            if (this->useRelativeBeamPruning) {
                 heapB->prune_relative(this->relativeBeamWidth);
            } else {
                 heapB->prune(this->pruningHeapSizeTarget);
            }
            // *******************************

            size_t size_after = heapB->size();
            // Optional: Get best WER after pruning
            // shared_ptr<ShortlistEntry> best_after_ptr = heapB->GetBestWerCandidate();
            // SLE best_after = best_after_ptr ? best_after_ptr.get() : nullptr;

            if (this->enableDetailedWalkerLogging && logger->should_log(spdlog::level::debug)) {
                 // Updated log message to reflect which strategy was used
                 logger->debug(
                     "Pruning HeapB (Loop {}) using {}: Size {} -> {}, Target/Beam: {:.1f}. Visited states: {}",
                     loopCount,
                     (this->useRelativeBeamPruning ? "Relative Beam" : "Fixed Target"),
                     size_before, size_after,
                     (this->useRelativeBeamPruning ? this->relativeBeamWidth : (float)this->pruningHeapSizeTarget),
                     visited_states.size());
                 // Add logging for best WER before/after if needed
            }
            loopSinceLastPruning = 0;
        }
        auto prune_end = high_resolution_clock::now();
        if (did_prune) {
            time_pruning += duration_cast<microseconds>(prune_end - prune_start);
        }
        // --- End Timing ---
    }
    loopSinceLastPruning++;

    // let's switch heaps
    auto heapTmp = heapA;
    heapA = heapB;
    heapB = heapTmp;
    // heapB is now empty after the swap, ready for next round's insertions
  }

  // Final log summary
  logger->info("Search finished. Loops: {}, Candidates found: {}", loopCount, topEntries.size());
  logger->info("Final Sizes - HeapA: {}, HeapB: {}, Logbook: {}, Visited: {}", heapA->size(), heapB->size(), logbook.size(), visited_states.size());

  logger->info("Reconstructing {} best alignments...", std::min((int)topEntries.size(), numBests));
  // Sort topEntries by WER before detailed reconstruction?
  std::sort(topEntries.begin(), topEntries.end(), [](const ShortlistEntry& a, const ShortlistEntry& b) {
      // Handle division by zero
      double wer_a = (a.numWords == 0) ? std::numeric_limits<double>::max() : (double)a.numErrors / a.numWords;
      double wer_b = (b.numWords == 0) ? std::numeric_limits<double>::max() : (double)b.numErrors / b.numWords;
      // Could add secondary sort key like costSoFar if WERs are equal
      return wer_a < wer_b;
  });

  if (topEntries.size() > 0) {
    int count = 0;
    for (auto &top : topEntries) {
       if (count >= numBests) break; // Only reconstruct the requested number
      logger->info("Getting details for candidate {}/{}: State {}, WER {:.4f} ({} err / {} words), Cost {}",
            count + 1, std::min((int)topEntries.size(), numBests),
            top.currentState,
            (top.numWords == 0) ? std::numeric_limits<double>::infinity() : (double)top.numErrors / top.numWords,
            top.numErrors, top.numWords, top.costSoFar);

      auto align = GetDetailsFromTopCandidates(top, symbol, options);
      topAlignments.push_back(align);
      count++;
    }
  } else {
       logger->warn("No final states reached or no paths survived pruning.");
  }

  return topAlignments;
}

std::shared_ptr<ShortlistEntry> Walker::enqueueIfNeeded(std::shared_ptr<ShortlistEntry> currentState,
                                                        const MyArc& arc, bool isAnchor) {
  shared_ptr<ShortlistEntry> enqueued = nullptr;

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
  enqueued->local_arc = arc;

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

  wer_alignment *class_label_wer_info = nullptr;

  while (now != nullptr) {
    const MyArc& local_arc = now->local_arc;
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
        global_wer_alignment.label_alignments.emplace_back();

        class_label_wer_info = &global_wer_alignment.label_alignments.back();
        class_label_wer_info->classLabel = ilabel;
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
