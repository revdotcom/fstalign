/*
Walker.h
 JP Robichaud (jp@rev.com)
 2018

*/

#ifndef __WALKER_H__
#define __WALKER_H__

#include "AlignmentTraversor.h"
#include "FstLoader.h"
#include "IComposition.h"
#include "PathHeap.h"

class Walker {
 public:
  Walker();
  ~Walker();
  vector<shared_ptr<wer_alignment>> walkComposed(IComposition &fst, SymbolTable &symbol, FstAlignOption &options,
                                                 int numBests);
  int numberOfLoopsBeforePruning = 50;
  int pruningHeapSizeTarget = 20;

 private:
  map<int, float> *logbook;
  PathHeap *heapA;
  PathHeap *heapB;
  std::shared_ptr<spdlog::logger> logger;

  std::shared_ptr<ShortlistEntry> enqueueIfNeeded(std::shared_ptr<ShortlistEntry> currentStatePtr,
                                                  shared_ptr<MyArc> arc_ptr, bool isAnchor);
  shared_ptr<wer_alignment> GetDetailsFromTopCandidates(ShortlistEntry &currentState, SymbolTable &symbol,
                                                        FstAlignOption &options);
};

#endif  // __WALKER_H__
