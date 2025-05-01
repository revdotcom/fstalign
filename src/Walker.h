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
#include <unordered_map>

class Walker {
 public:
  Walker();
  ~Walker() = default;
  vector<wer_alignment> walkComposed(IComposition &fst, SymbolTable &symbol, FstAlignOption &options,
                                                 int numBests);
  int numberOfLoopsBeforePruning = 10;
  int pruningHeapSizeTarget = 100;
  bool useRelativeBeamPruning = true;
  float relativeBeamWidth = 20.0;

  // Logging configuration
  bool enableDetailedWalkerLogging = false;

 private:
  std::unordered_map<int, float> logbook;
  PathHeap _heapA;
  PathHeap _heapB;
  PathHeap *heapA;
  PathHeap *heapB;
  std::shared_ptr<spdlog::logger> logger;

  std::shared_ptr<ShortlistEntry> enqueueIfNeeded(std::shared_ptr<ShortlistEntry> currentStatePtr,
                                                  const MyArc& arc_ptr, bool isAnchor);
  wer_alignment GetDetailsFromTopCandidates(ShortlistEntry &currentState, SymbolTable &symbol,
                                                        FstAlignOption &options);
};

#endif  // __WALKER_H__
