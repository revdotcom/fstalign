/*
FstLoader.cpp
 JP Robichaud (jp@rev.com)
 2018

*/

#include "FstLoader.h"
#include "utilities.h"

FstLoader::FstLoader() {
  // TODO Auto-generated constructor stub
}

FstLoader::~FstLoader() {
  // TODO Auto-generated destructor stub
}

void FstLoader::AddSymbolIfNeeded(fst::SymbolTable &symbol, std::string str_value) {
  if (symbol.Find(str_value) == -1) {
    symbol.AddSymbol(str_value);
  }
}
