/*
 * utilities.h
 *
 *  Created on: 2018-04-23
 *      Author: JP Robichaud (jp@rev.com)
 */

#ifndef UTILITIES_H_
#define UTILITIES_H_

#include <unicode/locid.h>
#include <unicode/unistr.h>
#include <unicode/ustream.h>
#include <algorithm>
#include <cctype>
#include <codecvt>
#include <cstdarg>
#include <cstring>
#include <iostream>
#include <iterator>
#include <locale>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <fst/fstlib.h>

#include "logging.h"
// #include "FstLoader.h"

#define quote(x) #x

using namespace std;
using namespace fst;

extern const string EPSILON;  // = "ε";
extern const string INS;      // = "ε";
extern const string DEL;      // = "ε";

extern const string TK_GLOBAL_CLASS;  // "global";
extern const string NOOP;

typedef struct wer_alignment wer_alignment;
typedef struct wer_alignment *WERAp;
typedef shared_ptr<wer_alignment> spWERA;

typedef float precision_t;
typedef float recall_t;

typedef unordered_map<string, uint64_t> bigrams;

typedef struct gram_error_counter {
  int correct = 0;
  int del = 0;
  int subst_fp = 0;
  int subst_fn = 0;
  int ins = 0;
  precision_t precision = 0.0f;
  recall_t recall = 0.0f;
  gram_error_counter(int c, int d, int sfp, int sfn, int i) : correct(c), del(d), subst_fp(sfp), subst_fn(sfn), ins(i) {}
} gram_error_counter;

struct wer_alignment {
  string classLabel;
  // int numErrors;
  int insertions = 0;
  int deletions = 0;
  int substitutions = 0;
  int numWordsInReference = 0;
  int numWordsInHypothesis = 0;

  vector<string> ref_words;
  vector<string> hyp_words;

  // we could perhaps get rid of these using <del> or <ins> in sub_words
  vector<string> del_words;
  vector<string> ins_words;
  vector<pair<string, string>> sub_words;

  precision_t precision;
  recall_t recall;
  // map<string, pair<precision_t, recall_t>> unigram_stats;
  vector<pair<string, gram_error_counter>> unigram_stats;

  // map<pair<string, string>, pair<float, float>>
  vector<pair<string, gram_error_counter>> bigrams_stats;
  bigrams ref_bigrams;
  bigrams hyp_bigrams;
  vector<pair<string, string>> bigram_tokens;

  vector<pair<string, string>> tokens;
  vector<wer_alignment> label_alignments;
  int NumErrors() { return insertions + substitutions + deletions; }
  /* can return infinity if numWordsInReference == 0 and numWordsInHypothesis > 0 */
  float WER() const {
    if (numWordsInReference > 0) {
      return (float)(insertions + deletions + substitutions) / (float)numWordsInReference;
    }

    if (numWordsInHypothesis > 0) {
      return numeric_limits<float>::infinity();
    }

    return 0;
  }

  void Reverse() {
    std::reverse(ref_words.begin(), ref_words.end());
    std::reverse(hyp_words.begin(), hyp_words.end());
    std::reverse(ins_words.begin(), ins_words.end());
    std::reverse(del_words.begin(), del_words.end());
    std::reverse(sub_words.begin(), sub_words.end());
    std::reverse(tokens.begin(), tokens.end());
    for (auto &a : label_alignments) {
      a.Reverse();
    }
  }
};

struct FstAlignOption {
  bool bForceEnterAndExit;

  float corCost;
  float insCost;
  float delCost;
  float subCost;

  string symEps;
  string symOov;
  string symIns;
  string symDel;
  string symSub;
  string symInaud;
  string symSil;
  string symUnk;

  int eps_idx;
  int oov_idx;
  int ins_idx;
  int del_idx;
  int sub_idx;
  int inaud_idx;
  int sil_idx;
  int unk_idx;

  FstAlignOption()
      : bForceEnterAndExit(false),
        corCost(0.0f),
        insCost(3.0f),
        delCost(3.0f),
        subCost(4.0f),
        symEps("<eps>"),
        symOov("<oov>"),
        symIns("<ins>"),
        symDel("<del>"),
        symSub("<sub>"),
        symInaud("<inaudible>"),
        symSil("<silence>"),
        symUnk("<unk>") {}

  void RegisterSymbols(fst::SymbolTable &symbol) {
    // int noSym = fst::kNoSymbol;
    int noSym = -1;
    eps_idx = symbol.Find(symEps);
    if (eps_idx == noSym) {
      eps_idx = symbol.AddSymbol(symEps);
    }

    oov_idx = symbol.Find(symOov);
    if (oov_idx == noSym) {
      oov_idx = symbol.AddSymbol(symOov);
    }

    ins_idx = symbol.Find(symIns);
    if (ins_idx == noSym) {
      ins_idx = symbol.AddSymbol(symIns);
    }

    del_idx = symbol.Find(symDel);
    if (del_idx == noSym) {
      del_idx = symbol.AddSymbol(symDel);
    }

    sub_idx = symbol.Find(symSub);
    if (sub_idx == noSym) {
      sub_idx = symbol.AddSymbol(symSub);
    }

    inaud_idx = symbol.Find(symInaud);
    if (inaud_idx == noSym) {
      inaud_idx = symbol.AddSymbol(symInaud);
    }

    sil_idx = symbol.Find(symSil);
    if (sil_idx == noSym) {
      sil_idx = symbol.AddSymbol(symSil);
    }

    unk_idx = symbol.Find(symUnk);
    if (unk_idx == noSym) {
      unk_idx = symbol.AddSymbol(symUnk);
    }
  }
};
/* printing FST on the console (or the specified logger) */
void printFst(const fst::StdFst *fst, const fst::SymbolTable *symbol);
void printFst(string loggerName, const fst::StdFst *fst, const fst::SymbolTable *symbol);

// from StackOverflow : nice way to get a function call when delimiters on a
// string are matched
template <typename StringFunction>
void splitString(const string &str, char delimiter, StringFunction f);

bool EndsWithCaseInsensitive(const string &value, const string &ending);
bool iequals(const std::string &, const std::string &);

// string manip
void ltrim(std::string &s);
void rtrim(std::string &s);
void trim(std::string &s);
std::string trim_copy(std::string s);

template <class Iter>
Iter splitStringIter(const std::string &s, const std::string &delim, Iter out);

std::string string_join(const std::vector<std::string> &elements, const char *const separator);

unordered_set<string> get_bigrams(wer_alignment &topAlignment);
bool isValidNgram(const string &token);
bool isEntityLabel(const string &token);
bool isSynonymLabel(const string &token);
bool IsNoisecodeToken(const string &token);
string getLabelIdFromToken(const string &token);
std::string GetEnv(const std::string &var, const std::string default_value);

// going from ___23_ORDINAL___  to ORDINAL
string GetLabelNameFromClassLabel(string classLabel);

string GetClassLabel(string best_label);

string UnicodeLowercase(string token);

#endif  // UTILITIES_H_
