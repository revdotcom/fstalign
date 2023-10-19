/*
FstLoader.h
 JP Robichaud (jp@rev.com)
 2018

*/

#ifndef __FSTLOADER_H_
#define __FSTLOADER_H_

#include <vector>
#include "utilities.h"

class FstLoader {
 protected:
  typedef std::vector<std::string> TokenType;
  TokenType mToken;

 public:
  FstLoader();
  virtual ~FstLoader();
  virtual void addToSymbolTable(fst::SymbolTable &symbol) const = 0;
  static void AddSymbolIfNeeded(fst::SymbolTable &symbol, std::string str_value);
  virtual fst::StdVectorFst convertToFst(const fst::SymbolTable &symbol, std::vector<int> map) const = 0;
  virtual std::vector<int> convertToIntVector(fst::SymbolTable &symbol) const = 0;

  static std::unique_ptr<FstLoader> MakeReferenceLoader(const std::string& ref_filename,
                                                        const std::string& wer_sidecar_filename,
                                                        const std::string& json_norm_filename,
                                                        bool use_punctuation,
                                                        bool use_case,
                                                        bool symbols_file_included);

  static std::unique_ptr<FstLoader> MakeHypothesisLoader(const std::string& hyp_filename,
                                                         const std::string& hyp_json_norm_filename,
                                                         bool use_punctuation,
                                                         bool use_case,
                                                         bool symbols_file_included);


};

#endif /* __FSTLOADER_H_ */
