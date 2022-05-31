/*
 * Nlp.h
 *
 *  Created on: 2018-04-23
 *      Author: JP Robichaud (jp@rev.com)
 */
#ifndef NLP_H_
#define NLP_H_

#include <iomanip>

#include <json/json.h>

#include "FstLoader.h"

using namespace std;
using namespace fst;

struct RawNlpRecord {
  string token;
  string speakerId;
  string punctuation;
  string prepunctuation;
  string ts;
  string endTs;
  string casing;
  string labels;
  string best_label;
  string best_label_id;
  vector<string> wer_tags;
};

class NlpReader {
 public:
  NlpReader();
  virtual ~NlpReader();
  vector<RawNlpRecord> read_from_disk(const std::string &filename);
  string GetBestLabel(std::string &labels);
  vector<string> GetWerTags(std::string &wer_tags_str);
  string GetLabelId(std::string &label);
};

class NlpFstLoader : public FstLoader {
 public:
  NlpFstLoader(std::vector<RawNlpRecord> &records, Json::Value normalization, Json::Value wer_sidecar, bool processLabels);
  NlpFstLoader(std::vector<RawNlpRecord> &records, Json::Value normalization, Json::Value wer_sidecar);
  virtual ~NlpFstLoader();
  virtual void addToSymbolTable(fst::SymbolTable &symbol) const;
  virtual fst::StdVectorFst convertToFst(const fst::SymbolTable &symbol, std::vector<int> map) const;
  virtual std::vector<int> convertToIntVector(fst::SymbolTable &symbol) const;

  int GetProperSymbolId(const fst::SymbolTable &symbol, string token, string symUnk) const;
  vector<RawNlpRecord> mNlpRows;
  vector<std::string> mSpeakers;
  Json::Value mJsonNorm;
  Json::Value mWerSidecar;
  virtual const std::string &getToken(int index) const { return mToken.at(index); }
};

#endif /* NLP_H_ */
