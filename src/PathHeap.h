/*
PathHeap.h
 JP Robichaud (jp@rev.com)
 2018

*/

#ifndef __PATH_HEAP_H__
#define __PATH_HEAP_H__

#include <limits>

#include "utilities.h"

using namespace std;
using namespace fst;

typedef struct ShortlistEntry ShortlistEntry;
typedef struct ShortlistEntry* SLE;
typedef struct MyArc MyArc;
typedef struct MyArc* MyArcPtr;
typedef shared_ptr<ShortlistEntry> spSLE;

struct MyArc {
  int ilabel;
  int olabel;
  float weight;
  int nextstate;
};

struct ShortlistEntry {
  int currentState = 0;
  int whereTo = 0;
  int numErrors = 0;
  int numWords = 0;
  int numInsert = 0;
  double costToGoThere = 0;
  float costSoFar = 0;
  shared_ptr<MyArc> local_arc;
  shared_ptr<ShortlistEntry> linkToHere = nullptr;
};

struct shortlistComparatorSharedPtr {
  bool operator()(const shared_ptr<ShortlistEntry>& a, const shared_ptr<ShortlistEntry>& b) {
    if (a->numWords == b->numWords) {
      if (a->numErrors == b->numErrors) {
        if (a->costSoFar == b->costSoFar) {
          return a->currentState < b->currentState;
        }

        return a->costSoFar < b->costSoFar;
      }

      return a->numErrors < b->numErrors;
    }

    return a->numWords < b->numWords;
  }
};

class PathHeap {
 public:
  PathHeap();
  void insert(std::shared_ptr<ShortlistEntry> entry);
  shared_ptr<ShortlistEntry> removeFirst();
  int prune(int targetSz);
  int size();
  std::shared_ptr<ShortlistEntry> GetBestWerCandidate();
  int pruningErrorOffset = 20;
  bool pruningIncludeInsInThreshold = true;

 private:
  set<std::shared_ptr<ShortlistEntry>, shortlistComparatorSharedPtr>* heap;
};
#endif  // __PATH_HEAP_H__