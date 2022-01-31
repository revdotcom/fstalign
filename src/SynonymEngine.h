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

struct SynonymOptions {
  bool disable_cutoffs = false;
  bool disable_hyphen_ignore = false;
};

class SynonymEngine {
 public:
  SynonymEngine(SynonymOptions syn_opts);

  void LoadFile(string filename);
  SynKey GetKeyFromString(string lhs);
  SynVals GetValuesFromStrings(string rhs);
  void ParseStrings(vector<string> lines);
  void ApplyToFst(StdVectorFst &fst, SymbolTable &symbol);
  void GenerateSynFromSymbolTable(SymbolTable &symbol);

 protected:
  SynonymOptions opts_;
  map<SynKey, SynVals> synonyms;
  std::shared_ptr<spdlog::logger> logger_;
};

#endif  // __SYN_ENGINE_H
