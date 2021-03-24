/*
 *
 * SynonymEngine.h
 *
 * JP Robichaud (jp@rev.com)
 * 2018
 *
 */
#ifndef __SYN_ENGINE_H
#define __SYN_ENGINE_H
#include <iomanip>

#include "utilities.h"

using namespace std;
using namespace fst;

typedef vector<string> SynKey;
typedef vector<vector<string>> SynVals;

class SynonymEngine {
 public:
  SynonymEngine(bool disable_cutoffs);
  void load_file(string filename);
  void parse_strings(vector<string> lines);
  void apply_to_fst(StdVectorFst &fst, SymbolTable &symbol);

 protected:
  map<SynKey, SynVals> synonyms;
  bool disable_cutoffs;
  std::shared_ptr<spdlog::logger> logger_;
};

#endif  // __SYN_ENGINE_H
