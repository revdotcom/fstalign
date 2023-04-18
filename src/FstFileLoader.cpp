/*
 * FstFileLoader.cpp
 */
#include "FstFileLoader.h"

FstFileLoader::FstFileLoader(std::string filename) : FstLoader(), filename_(filename) {}

void FstFileLoader::addToSymbolTable(fst::SymbolTable& symbol) const { return; }

fst::StdVectorFst FstFileLoader::convertToFst(const fst::SymbolTable& symbol, std::vector<int> map) const {
  auto logger = logger::GetOrCreateLogger("FstFileLoader");
  fst::StdVectorFst* transducer = fst::StdVectorFst::Read(filename_);
  logger->info("Total FST has {} states.", transducer->NumStates());
  return (*transducer);
}

std::vector<int> FstFileLoader::convertToIntVector(fst::SymbolTable& symbol) const {
  auto logger = logger::GetOrCreateLogger("FstFileLoader");
  std::vector<int> vect;
  logger->error("convertToIntVector isn't implemented for FST inputs");
  vect.reserve(0);
  vect.resize(0);
  return vect;
}

FstFileLoader::~FstFileLoader() {}
