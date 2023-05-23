/*
AlignmentTraversor.h
 JP Robichaud (jp@rev.com)
 2021

*/

#ifndef __ATRAVERSOR_H__
#define __ATRAVERSOR_H__

#include "utilities.h"

struct triple {
  string ref;
  string hyp;
  string classLabel;
};

class AlignmentTraversor {
 public:
  AlignmentTraversor(wer_alignment &topLevel);
  bool NextTriple(triple &triple);
  void Restart();

 private:
  wer_alignment &root;
  int currentPosInRoot = -1;
  int currentPosInSubclass;
  wer_alignment *currentSubclass;
};

#endif  // __ATRAVERSOR_H__
