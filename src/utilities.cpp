
#include "utilities.h"

#include <iterator>

// controlling the graph
const std::string EPSILON = "<eps>";
const std::string INS = "<ins>";
const std::string DEL = "<del>";

// for the sticthing process
const std::string TK_GLOBAL_CLASS = "global";

// for fallback deletions
const std::string NOOP = "!!noop-token!!";

void printFst(const fst::StdFst *fst, const fst::SymbolTable *symbol) { printFst("console", fst, symbol); }

void printFst(std::string loggerName, const fst::StdFst *fst, const fst::SymbolTable *symbol) {
  auto log = logger::GetOrCreateLogger(loggerName);
  if (log->should_log(spdlog::level::info)) {
    for (fst::StateIterator<fst::StdFst> siter(*fst); !siter.Done(); siter.Next()) {
      fst::StdFst::StateId stateId = siter.Value();
      float end_state_weight = fst->Final(stateId).Value();

      for (fst::ArcIterator<fst::StdFst> aiter(*fst, stateId); !aiter.Done(); aiter.Next()) {
        const fst::StdArc &arc = aiter.Value();

        std::stringstream ss;
        std::stringstream ss1;
        ss << arc.ilabel << "/" << symbol->Find(arc.ilabel);
        std::string ilabel = ss.str();

        ss1 << arc.olabel << "/" << symbol->Find(arc.olabel);
        std::string olabel = ss1.str();

        log->info("{}\t{}\t{}\t{}\t{}", stateId, arc.nextstate, ilabel, olabel, arc.weight.Value());
      }

      if (end_state_weight != numeric_limits<float>::infinity() && end_state_weight != 0) {
        log->info("{}", stateId);
      }
    }
  }
}

template <typename StringFunction>
void splitString(const std::string &str, char delimiter, StringFunction f) {
  std::size_t from = 0;
  for (std::size_t i = 0; i < str.size(); ++i) {
    if (str[i] == delimiter) {
      f(str, from, i);
      from = i + 1;
    }
  }
  if (from <= str.size()) {
    f(str, from, str.size());
  }
}

struct iequal {
  bool operator()(int c1, int c2) const { return std::toupper(c1) == std::toupper(c2); }
};

bool iequals(const std::string &str1, const std::string &str2) {
  if (str1.size() != str2.size()) {
    return false;
  }

  if (str1 == str2) {
    return true;
  }

  return std::equal(str1.begin(), str1.end(), str2.begin(), iequal());
}

// trim from start (in place)
void ltrim(std::string &s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !std::isspace(ch); }));
}

// trim from end (in place)
void rtrim(std::string &s) {
  s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) { return !std::isspace(ch); }).base(), s.end());
}

// trim from both ends (in place)
void trim(std::string &s) {
  ltrim(s);
  rtrim(s);
}

// trim from both ends (copying)
std::string trim_copy(std::string s) {
  trim(s);
  return s;
}

template <class Iter>
Iter splitStringIter(const std::string &s, const std::string &delim, Iter out) {
  if (delim.empty()) {
    *out++ = s;
    return out;
  }
  size_t a = 0, b = s.find(delim);
  for (; b != std::string::npos; a = b + delim.length(), b = s.find(delim, a)) {
    *out++ = std::move(s.substr(a, b - a));
  }
  *out++ = std::move(s.substr(a, s.length() - a));
  return out;
}

std::string string_join(const std::vector<std::string> &elements, const char *const separator) {
  switch (elements.size()) {
    case 0:
      return "";
    case 1:
      return elements[0];
    default:
      std::ostringstream os;
      std::copy(elements.begin(), elements.end() - 1, std::ostream_iterator<std::string>(os, separator));
      os << *elements.rbegin();
      return os.str();
  }
}

bool isValidNgram(const string &token) {
  if ((token.find(INS) != string::npos) || (token.find(DEL) != string::npos) || (token.find(EPSILON) != string::npos) ||
      (token.find("___") != string::npos)) {
    return false;
  } else {
    return true;
  }
}

unordered_set<string> get_bigrams(const spWERA &topAlignment) {
  string bigram_ref = "";
  string bigram_hyp = "";
  unordered_set<string> all_bigrams;
  vector<string> bi_words;

  // Create a list of all tokens, flattening entity tokens
  vector<pair<string, string>> flattened_tokens;
  for (auto &tokens : topAlignment->tokens) {
    // handle entity labels
    if (isEntityLabel(tokens.first)) {
      auto class_label = tokens.first;

      for (auto &label_alignment : topAlignment->label_alignments) {
        if (label_alignment->classLabel == class_label) {
          for (auto &labelTokens : label_alignment->tokens) {
            flattened_tokens.push_back(labelTokens);
          }
        }
      }
    } else {
      flattened_tokens.push_back(tokens);
    }
  }

  for (auto it = flattened_tokens.begin(); it != std::prev(flattened_tokens.end()); ++it) {
    bi_words = {it->first, std::next(it)->first};
    bigram_ref = string_join(bi_words, " ");
    // cout << it  - topAlignment->tokens.begin() << " : "<< bigram_ref << " (" << it->first << " " <<
    // std::next(it)->first <<" )" << endl;
    if (isValidNgram(bigram_ref)) {
      topAlignment->ref_bigrams[bigram_ref] += 1;
      all_bigrams.insert(bigram_ref);
    }
    bi_words = {it->second, std::next(it)->second};
    bigram_hyp = string_join(bi_words, " ");
    // cout << it  - topAlignment->tokens.begin() << " : "<< bigram_hyp << " (" << it->second << " " <<
    // std::next(it)->second <<" )" << endl;
    if (isValidNgram(bigram_hyp)) {
      topAlignment->hyp_bigrams[bigram_hyp] += 1;
      all_bigrams.insert(bigram_hyp);
    }

    topAlignment->bigram_tokens.push_back(std::make_pair(bigram_ref, bigram_hyp));
  }
  return all_bigrams;
}

bool isEntityLabel(const string &token) { return token.find("___") == 0 ? true : false; }

bool isSynonymLabel(const string &token) {
  //   return token.find("___SYN-") == 0 ? true : false;
  return (token.find("___") == 0 && token.find("_SYN_") != std::string::npos) ? true : false;
}

bool IsNoisecodeToken(const string &token) { return token.find("<") == 0 && token.find(">") == token.length() - 1; }

string getLabelIdFromToken(const string &token) {
  if (!isEntityLabel(token)) {
    return "";
  }
  // Example label: ___0_CONTRACTION___

  // Trim the ___ from the start and end of the label string
  auto label_id = token.substr(3, token.size() - 6);

  // Isolate the ID at the start of the label, separated by _
  int p = label_id.find("_");
  if (p > 0) {
    label_id = label_id.substr(0, p);
  }

  return label_id;
}

std::string GetEnv(const std::string &var, const std::string default_value) {
  const char *val = std::getenv(var.c_str());
  if (val == nullptr) {  // invalid to assign nullptr to std::string
    return default_value;
  } else {
    return val;
  }
}

// going from ___23_ORDINAL___  to ORDINAL
string GetLabelNameFromClassLabel(string classLabel) {
  string label_id = classLabel.substr(3, classLabel.size() - 6);
  string label = label_id.substr(label_id.find("_") + 1);
  return label;
}

string GetClassLabel(string best_label) {
  if (best_label == "") {
    return "";
  }

  string classlabel = string("___" + best_label + "___");
  std::replace(classlabel.begin(), classlabel.end(), ':', '_');
  return classlabel;
}

string UnicodeLowercase(string token) {
  icu::UnicodeString utoken = icu::UnicodeString::fromUTF8(token);
  std::string lower_cased;
  utoken.toLower().toUTF8String(lower_cased);
  return lower_cased;
}

bool EndsWithCaseInsensitive(const string &value, const string &ending) {
  if (ending.size() > value.size()) {
    return false;
  }
  return equal(ending.rbegin(), ending.rend(), value.rbegin(),
               [](const char a, const char b) { return tolower(a) == tolower(b); });
}
