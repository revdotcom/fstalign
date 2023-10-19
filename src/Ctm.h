/*
 *
 * Ctm.h
 *
 * JP Robichaud (jp@rev.com)
 * (C) 2018
 *
 */

#ifndef __CTM_H__
#define __CTM_H__

#include "FstLoader.h"
#include "utilities.h"

using namespace std;
using namespace fst;

struct RawCtmRecord {
  string recording;
  string channel;
  float start_time_secs;
  float duration_secs;
  string word;
  float confidence;
};

class CtmFstLoader : public FstLoader {
 public:
  CtmFstLoader(std::vector<RawCtmRecord> &records, bool use_case = false);
  ~CtmFstLoader();
  vector<RawCtmRecord> mCtmRows;
  virtual void addToSymbolTable(fst::SymbolTable &symbol) const;
  virtual fst::StdVectorFst convertToFst(const fst::SymbolTable &symbol, std::vector<int> map) const;
  virtual std::vector<int> convertToIntVector(fst::SymbolTable &symbol) const;
  virtual const std::string &getToken(int index) const { return mToken.at(index); }
 private:
  bool mUseCase;
};

class CtmReader {
 public:
  CtmReader();
  vector<RawCtmRecord> read_from_disk(const std::string &filename);
};

#endif  // __CTM_H__
