/*
 * OneBestFstLoader.h
 * JP Robichaud (jp@rev.com)
 * 2018
 */

#ifndef ONEBESTFSTLOADER_H_
#define ONEBESTFSTLOADER_H_

#include "FstLoader.h"

class OneBestFstLoader : public FstLoader {
 public:
  OneBestFstLoader();
  virtual ~OneBestFstLoader();
  void LoadTextFile(const std::string filename);
  void BuildFromString(const std::string content);

  virtual void addToSymbolTable(fst::SymbolTable &symbol) const;
  virtual fst::StdVectorFst convertToFst(const fst::SymbolTable &symbol, std::vector<int> map) const;
  virtual const std::string &getToken(int index) const { return mToken.at(index); }
  virtual std::vector<int> convertToIntVector(fst::SymbolTable &symbol) const;
  int TokensSize() { return mToken.size(); }
};

#endif /* ONEBESTFSTLOADER_H_ */
