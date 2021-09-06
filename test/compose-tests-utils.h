
#ifndef __COMPOSE_TEST_UTILITIES_H__
#define  __COMPOSE_TEST_UTILITIES_H__ 1 

#include "src/OneBestFstLoader.h"
#include "src/AdaptedComposition.h"
#include "src/logging.h"

// helper methods

StdVectorFst GetFstFromString(SymbolTable *symbols, const std::string str) {
  OneBestFstLoader loader;
  loader.BuildFromString(str);
  loader.addToSymbolTable(*symbols);
  std::vector<int> map;
  return  loader.convertToFst(*symbols, map);
}

StdVectorFst GetStdFstA() {
  StdVectorFst a;
  a.AddState();  // 0
  a.AddState();  // 1
  a.AddState();  // 2
  a.AddState();  // 3

  a.SetStart(0);
  a.SetFinal((StateId)3, StdArc::Weight::One());
  // Arc constructor args: ilabel, olabel, weight, dest state ID.

  // 0 -> 1:1 -> 1 -> 2:2 -> 2 -> 3:3

  a.AddArc(0, StdArc(1, 1, 0, 1));
  a.AddArc(1, StdArc(2, 2, 0, 2));
  a.AddArc(2, StdArc(3, 3, 0, 3));

  return a;
}

StdVectorFst GetStdFstB() {
  StdVectorFst a;
  a.AddState();  // 0
  a.AddState();  // 1
  a.AddState();  // 2
  a.AddState();  // 3

  a.SetStart(0);
  a.SetFinal((StateId)3, StdArc::Weight::One());
  // Arc constructor args: ilabel, olabel, weight, dest state ID.

  // 0 -> 1:1 -> 1 -> 2:2 -> 2 -> 3:3

  a.AddArc(0, StdArc(1, 4, 0, 1));
  a.AddArc(1, StdArc(2, 5, 0, 2));
  a.AddArc(2, StdArc(3, 6, 0, 3));

  return a;
}


#endif