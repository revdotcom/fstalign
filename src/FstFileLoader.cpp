/*
 * FstFileLoader.cpp
 */
#include "FstFileLoader.h"

FstFileLoader::FstFileLoader(std::string filename) : FstLoader(), filename_(filename) {}

void FstFileLoader::addToSymbolTable(fst::SymbolTable& symbol) const { return; }

fst::StdVectorFst FstFileLoader::convertToFst(const fst::SymbolTable& symbol) const {
  auto logger = logger::GetOrCreateLogger("FstFileLoader");
  fst::StdVectorFst *transducer = fst::StdVectorFst::Read(filename_);
  logger->info("Total FST has {} states.", transducer->NumStates());
  return (*transducer);  
}

FstFileLoader::~FstFileLoader() {}
