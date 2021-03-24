/*
 * KaldiArchiveFstLoader.h
 * Quinn McNamara (quinn@rev.com)
 * 2020
 */

#ifndef KaldiArchiveFstLoader_H_
#define KaldiArchiveFstLoader_H_

#include <fstream>
#include <regex>
#include <stdexcept>

#include "FstLoader.h"
#include "utilities.h"

// Kaldi dependencies
#include "base/kaldi-common.h"
#include "fstext/fstext-lib.h"
#include "util/common-utils.h"

class KaldiArchiveFstLoader : public FstLoader {
 public:
  KaldiArchiveFstLoader(std::string filename);
  ~KaldiArchiveFstLoader();

  virtual void addToSymbolTable(fst::SymbolTable &symbol) const;
  virtual fst::StdVectorFst convertToFst(const fst::SymbolTable &symbol) const;

 private:
  std::string filename_;
};

#endif /* KaldiArchiveFstLoader_H_ */
