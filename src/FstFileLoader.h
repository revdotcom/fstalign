/*
 * FstFileLoader.h
 * 
 * FstLoader for loading a serialized FST from disk.
 * 
 * Quinn McNamara (quinn@rev.com)
 * 2020
 */

#ifndef FstFileLoader_H_
#define FstFileLoader_H_

#include <fstream>
#include <stdexcept>
#include <vector>

#include "FstLoader.h"
#include "utilities.h"

class FstFileLoader : public FstLoader {
 public:
  FstFileLoader(std::string filename);
  ~FstFileLoader();

  virtual void addToSymbolTable(fst::SymbolTable &symbol) const;
  virtual fst::StdVectorFst convertToFst(const fst::SymbolTable &symbol, std::vector<int> map) const;
  virtual std::vector<int> convertToIntVector(fst::SymbolTable &symbol) const;

 private:
  std::string filename_;
};

#endif /* FstFileLoader_H_ */
