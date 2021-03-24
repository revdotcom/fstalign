/*
PathHeap.cpp
 JP Robichaud (jp@rev.com)
 2018

*/

#include "PathHeap.h"

using namespace std;
using namespace fst;

PathHeap::PathHeap() {
  // creating the set
  heap = new set<shared_ptr<ShortlistEntry>, shortlistComparatorSharedPtr>();
}

void PathHeap::insert(shared_ptr<ShortlistEntry> entry) {
  // just add it to the set, leaving to the comparator to do its job
  heap->insert(entry);
}

shared_ptr<ShortlistEntry> PathHeap::removeFirst() {
  // we want to take the 1st element and remove it
  auto logbookIter = heap->begin();
  auto currentState_ptr = *logbookIter;
  heap->erase(logbookIter);
  return currentState_ptr;
}

int PathHeap::size() { return heap->size(); }

shared_ptr<ShortlistEntry> PathHeap::GetBestWerCandidate() {
  set<shared_ptr<ShortlistEntry>, shortlistComparatorSharedPtr>::iterator iter = heap->begin();

  shared_ptr<ShortlistEntry> best = nullptr;
  float bestWer = std::numeric_limits<float>::quiet_NaN();
  while (iter != heap->end()) {
    auto entry = *iter;
    float local_wer = (float)entry->numErrors / (float)entry->numWords;

    if (best == nullptr) {
      best = entry;
      bestWer = local_wer;
      continue;
    }

    if (local_wer < bestWer) {
      bestWer = local_wer;
      best = entry;
    }

    iter++;
  }

  return best;
}

int PathHeap::prune(int targetSz) {
  set<shared_ptr<ShortlistEntry>, shortlistComparatorSharedPtr>::iterator iter = heap->begin();
  float wer0, wer_last;
  int sz = heap->size();
  for (int i = 0; i < targetSz && i < sz; i++) {
    float local_wer = (float)(*iter)->numErrors / ((float)(*iter)->numWords);
    if (i == 0) {
      wer0 = local_wer;
    }

    wer_last = local_wer;

    iter++;
  }

  auto last_wer_index = iter;
  last_wer_index--;
  auto logger = logger::GetOrCreateLogger("pathheap");
  // logger->set_level(spdlog::level::debug);
  logger->debug("==== pruning starting =====");
  logger->debug("pruning to {} items -> top wer was {} and last wer was {}.  We have {} items in the heap.", targetSz,
                wer0, wer_last, heap->size());

  /* TODO:  make sure we don't prune paths that have the same length/error-count
as the last one kept at 'targetSz'
*/

  int numErrorsWithoutInsertions = (*last_wer_index)->numErrors - (*last_wer_index)->numInsert;
  int pruned = 0;
  while (iter != heap->end()) {
    auto p = *iter;
    float local_wer = (float)(*iter)->numErrors / ((float)(*iter)->numWords);
    logger->debug(
        "candidate for prunung: wer0 {4:.4f}, wer_last {0:.4f} {2} words, current candidate {1:.4f}, {3} words",
        wer_last, local_wer, (*last_wer_index)->numWords, (*iter)->numWords, wer0);

    int localCoreErr = (*iter)->numErrors - (*iter)->numInsert;
    /* various strategies :
bool pruneMe = (*last_wer_index)->numErrors * 1.2 < (*iter)->numErrors; -> slow on larger files
bool pruneMe = (*last_wer_index)->numErrors * 1.1 < (*iter)->numErrors; -> slightly better on larger files
bool pruneMe = numErrorsWithoutInsertions * 1.1 < localCoreErr; --> a bit agressive
bool  pruneMe = (*last_wer_index)->numErrors + 20 < (*iter)->numErrors; --> seems to work resonably well
*/
    // TODO: make this '20' configurable.  Also consider using (numErrors -
    // numInsertion) + 20
    bool pruneMe = (*last_wer_index)->numErrors + 20 < (*iter)->numErrors;
    logger->debug("{} + 20 < {} = {}", numErrorsWithoutInsertions, localCoreErr, pruneMe);
    if (pruneMe) {
      heap->erase(iter++);
      pruned++;
    } else {
      iter++;
    }
  }
  logger->debug("after pruning we have {} items in the heap", heap->size());
  logger->debug("-----");

  return pruned;
}
