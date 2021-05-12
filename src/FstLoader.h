/*
FstLoader.h
 JP Robichaud (jp@rev.com)
 2018

*/

#ifndef __FSTLOADER_H_
#define __FSTLOADER_H_

#include "utilities.h"

class FstLoader {
 protected:
  typedef std::vector<std::string> TokenType;
  TokenType mToken;
  bool keep_case_ = false;

 public:
  FstLoader();
  virtual ~FstLoader();
  virtual void addToSymbolTable(fst::SymbolTable &symbol) const = 0;
  static void AddSymbolIfNeeded(fst::SymbolTable &symbol, std::string str_value);
  virtual fst::StdVectorFst convertToFst(const fst::SymbolTable &symbol) const = 0;
};

#endif /* __FSTLOADER_H_ */
