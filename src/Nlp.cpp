/*
 * Nlp.cpp
 *
 *  Created on: 2018-04-23
 *      Author: JP Robichaud (jp@rev.com)
 */
#include "Nlp.h"

#include <cstddef>
#include <fstream>

#include <csv/csv.h>
#include "utilities.h"

/***********************************
   NLP FstLoader class start
 ************************************/
NlpFstLoader::NlpFstLoader(std::vector<RawNlpRecord> &records, Json::Value normalization, Json::Value wer_sidecar)
    : NlpFstLoader(records, normalization, wer_sidecar, true) {}

NlpFstLoader::NlpFstLoader(std::vector<RawNlpRecord> &records, Json::Value normalization, Json::Value wer_sidecar,
                           bool processLabels, bool use_punctuation, bool use_case)
    : FstLoader() {
  mJsonNorm = normalization;
  mWerSidecar = wer_sidecar;
  mUseCase = use_case;

  std::string last_label;
  bool firstTk = true;

  // fuse multiple rows that have the same id/label into one entry only
  for (auto &row : records) {
    mNlpRows.push_back(row);
    auto curr_tk = row.token;
    auto curr_label = row.best_label;
    auto curr_label_id = row.best_label_id;
    auto punctuation = row.punctuation;
    auto curr_row_tags = row.wer_tags;

    // Update wer tags in records to real string labels
    vector<string> real_wer_tags;
    for (auto &tag : curr_row_tags) {
      auto real_tag = tag;
      if (mWerSidecar != Json::nullValue) {
        real_tag = "###" + real_tag + "_" + mWerSidecar[real_tag]["entity_type"].asString() + "###";
      }
      real_wer_tags.push_back(real_tag);
    }
    row.wer_tags = real_wer_tags;
    std::string speaker = row.speakerId;

    if (processLabels && curr_label != "") {
      if (firstTk || curr_label != last_label) {
        // string nickname = "___" + curr_label_id + "___";
        string nickname = string("___" + curr_label + "___");

        std::replace(nickname.begin(), nickname.end(), ':', '_');
        mToken.push_back(nickname);
        mSpeakers.push_back(speaker);
        // mToken.push_back("___" + curr_label_id + "___");
        // mToken.push_back("___" + curr_label + "___");
        //
        if (mJsonNorm != Json::nullValue) {
          // todo: add a new "candidate" where this token and the following (if
          // from the same label id) will also be available in the norm
          // candidates.  Right now, if we have 10$ marked as MONEY, the '10$' itself
          // won't be available for the alignment

          // vector<string> emptyVect;
          Json::Value elements;
          elements.append(curr_tk);
          Json::Value w_elements;
          w_elements["verbalization"] = elements;
          w_elements["probability"] = 0.0f;
          mJsonNorm[curr_label_id]["candidates"].append(w_elements);
          // mJsonNorm[curr_label_id]["candidates"][-1].append(curr_tk);
        }

      } else {
        // skip this label but keep the token in the norm candidates
        int last_idx = mJsonNorm[curr_label_id]["candidates"].size() - 1;
        mJsonNorm[curr_label_id]["candidates"][last_idx]["verbalization"].append(curr_tk);
      }
    } else {
      if (!mUseCase) {
        curr_tk = UnicodeLowercase(curr_tk);
      }
      mToken.push_back(curr_tk);
      mSpeakers.push_back(speaker);
    }
    if (use_punctuation && punctuation != "") {
      mToken.push_back(punctuation);
      mSpeakers.push_back(speaker);
      RawNlpRecord punc_row;
      punc_row.token = punc_row.punctuation;
      punc_row.speakerId = speaker;
      punc_row.punctuation = "";
      mNlpRows.push_back(punc_row);
    }
    firstTk = false;
    last_label = curr_label;
  }
}

void NlpFstLoader::addToSymbolTable(fst::SymbolTable &symbol) const {
  auto logger = logger::GetOrCreateLogger("NlpFstLoader");
  logger->info("addToSymbolTable() Building the FST from raw tokens");

  for (auto tk : mToken) {
    if (isEntityLabel(tk)) {
      // this token is a class, let's put the extra vocab
      // we also at it as is so that we have a way to find our way back to here
      AddSymbolIfNeeded(symbol, tk);
      std::string label_id = getLabelIdFromToken(tk);

      if (mJsonNorm != Json::nullValue) {
        auto candidates = mJsonNorm[label_id]["candidates"];
        logger->trace("for tk [{}] we have label_id [{}] and {} candidates", tk, label_id, candidates.size());
        for (Json::Value::ArrayIndex i = 0; i != candidates.size(); i++) {
          //
          auto candidate = candidates[i]["verbalization"];
          for (auto tk_itr : candidate) {
            std::string token = tk_itr.asString();
            if (!mUseCase) {
              token = UnicodeLowercase(token);
            }
            AddSymbolIfNeeded(symbol, token);
          }
        }
      }
    } else if (tk.find("<") == 0 && tk.find(">") == tk.length() - 1) {
      // we have something like <crosstalk> or <foreign:fr>
      // or the awful : <{note}>....
      auto colon_pos = tk.find(":");
      if (colon_pos == string::npos) {
        AddSymbolIfNeeded(symbol, tk);
      } else {
        auto effective_tk = tk.substr(0, colon_pos) + ">";
        logger->info("trimming noisecode from [{}] to [{}]", tk, effective_tk);
        AddSymbolIfNeeded(symbol, effective_tk);
      }
    } else {
      // mToken content is already lowercased
      AddSymbolIfNeeded(symbol, tk);
    }
  }
}

std::vector<int> NlpFstLoader::convertToIntVector(fst::SymbolTable &symbol) const {
  auto logger = logger::GetOrCreateLogger("NlpFstLoader");
  std::vector<int> vect;
  logger->info("convertToIntVector() Building a std::vector<int> from NLP rows");
  addToSymbolTable(symbol);
  int sz = mToken.size();
  vect.reserve(sz);

  FstAlignOption options;
  for (TokenType::const_iterator i = mToken.begin(); i != mToken.end(); ++i) {
    std::string token = *i;
    int token_sym = symbol.Find(token);
    if (token_sym == -1) {
      token_sym = symbol.Find(options.symUnk);
    }
    vect.emplace_back(token_sym);
  }

  return vect;
  // return std::move(vect);
}

fst::StdVectorFst NlpFstLoader::convertToFst(const fst::SymbolTable &symbol, std::vector<int> map) const {
  auto logger = logger::GetOrCreateLogger("NlpFstLoader");
  fst::StdVectorFst transducer;

  logger->info("convertToFst() Building the FST from NLP rows");

  FstAlignOption options;

  bool markLabels = true;
  int eps_sym = symbol.Find(options.symEps);
  // a 'real' eps transition?
  //   int eps_sym = fst::kNoLabel;

  // fst::kNoSymbol
  if (eps_sym == -1) {
    // symbol.AddSymbol(options.symEps);
    logger->warn("WARNING the eps symbol [{}] wasn't found, we'll assume its index is 0!", options.symEps);
    eps_sym = 0;
  }

  transducer.AddState();
  transducer.SetStart(0);

  int prevState = 0;
  int nextState = 1;
  int map_sz = map.size();
  int wc = 0;
  for (TokenType::const_iterator i = mToken.begin(); i != mToken.end(); ++i) {
    transducer.AddState();

    std::string token = *i;
    int token_sym = symbol.Find(token);
    if (token_sym == -1) {
      token_sym = symbol.Find(options.symUnk);
    }

    // logger->info("wc {}, token {}, map[wc] = {}, map_sz = {}", wc, token, map[wc], map_sz);
    if (map_sz > wc && map[wc] > 0) {
      transducer.AddArc(prevState, fst::StdArc(token_sym, token_sym, 1.0f, nextState));
    } else {
      transducer.AddArc(prevState, fst::StdArc(token_sym, token_sym, 0.0f, nextState));
    }
    wc++;

    if (isEntityLabel(token)) {
      /*
we used to do
2 -> 3  _0_ and then add the _0_ graph with an epsilon transition to '3'
but now we want _0_ -> graph -> _0_ again.  and these 2 deletions should
cost 0 and not cound as errors

so we add 2 states
*/

      int classPrevState, classNextState, classExitStartState, classExitEndState = 0;

      if (!markLabels) {
        /* original code */
        classPrevState = prevState;
        classNextState = nextState;
      } else {
        transducer.AddState();
        transducer.AddState();

        // basically, we want to attach the graph from this label
        // at the state after the _0_ label
        // classPrevState  -> where to start the arcs from for each of the alternative paths
        // classNextState  -> where to tie the end of each alternative paths with the eps transition
        classPrevState = nextState;
        classNextState = nextState + 1;

        classExitStartState = nextState + 1;
        classExitEndState = classExitStartState + 1;

        nextState = classExitEndState;

        transducer.AddArc(classExitStartState, fst::StdArc(token_sym, token_sym, 0.0f, classExitEndState));
      }

      auto label_id = getLabelIdFromToken(token);
      auto candidates = mJsonNorm[label_id]["candidates"];
      for (Json::Value::ArrayIndex i = 0; i != candidates.size(); i++) {
        //
        prevState = classPrevState;
        auto candidate = candidates[i]["verbalization"];
        for (auto tk_itr : candidate) {
          std::string ltoken = std::string(tk_itr.asString());
          if (!mUseCase) {
            ltoken = UnicodeLowercase(ltoken);
          }
          transducer.AddState();
          nextState++;

          int token_sym = symbol.Find(ltoken);
          if (token_sym == -1) {
            token_sym = symbol.Find(options.symUnk);
          }

          transducer.AddArc(prevState, fst::StdArc(token_sym, token_sym, 0.0f, nextState));

          prevState = nextState;
        }

        transducer.AddArc(prevState, fst::StdArc(eps_sym, eps_sym, 0.0f, classNextState));
      }

      if (!markLabels) {
        prevState = classNextState;
        nextState++;
      } else {
        prevState = classExitEndState;
        nextState++;
      }

    } else {
      // reguar token, we just progress of one state
      prevState = nextState;
      nextState++;
    }
  }

  int realFinal = transducer.AddState();
  transducer.AddArc(prevState, fst::StdArc(eps_sym, eps_sym, 0.0f, realFinal));
  logger->info("adding final Epsilon token from state {} to state {}", prevState, realFinal);
  logger->info("setting final state to {}", realFinal);

  transducer.SetFinal(realFinal, StdFst::Weight::One());
  return transducer;
}

int NlpFstLoader::GetProperSymbolId(const fst::SymbolTable &symbol, string token, string symUnk) const {
  int token_sym = symbol.Find(token);
  if (token_sym == -1) {
    if (token.find("<") == 0 && token.find(">") == token.length() - 1) {
      // we have something like <crosstalk> or <foreign:fr>
      // or the awful : <{note}>....
      auto colon_pos = token.find(":");
      if (colon_pos == string::npos) {
        token_sym = symbol.Find(symUnk);
      } else {
        auto effective_tk = token.substr(0, colon_pos) + ">";
        token_sym = symbol.Find(effective_tk);
      }
    } else {
      token_sym = symbol.Find(symUnk);
    }
  }
  return token_sym;
}

NlpFstLoader::~NlpFstLoader() {
  // TODO Auto-generated destructor stub
}

/*********************************** NLP FstLoader class end
 * ***********************************/

/***********************************
NLP Reader class start
 * ***********************************/
NlpReader::NlpReader() {
  // TODO Auto-generated constructor stub
}

NlpReader::~NlpReader() {
  // TODO Auto-generated destructor stub
}

std::vector<RawNlpRecord> NlpReader::read_from_disk(const std::string &filename) {
  std::vector<RawNlpRecord> vect;
  io::CSVReader<13, io::trim_chars<' ', '\t'>, io::no_quote_escape<'|'>> input_nlp(filename);
  // token|speaker|ts|endTs|punctuation|prepunctuation|case|tags|wer_tags|ali_comment|oldTs|oldEndTs
  input_nlp.read_header(io::ignore_missing_column | io::ignore_extra_column, "token", "speaker", "ts", "endTs",
                        "punctuation", "prepunctuation", "case", "tags", "wer_tags", "ali_comment", "oldTs", "oldEndTs",
                        "confidence");

  std::string token, speaker, ts, endTs, punctuation, prepunctuation, casing, tags, wer_tags, ali_comment, oldTs,
      oldEndTs, confidence;
  while (input_nlp.read_row(token, speaker, ts, endTs, punctuation, prepunctuation, casing, tags, wer_tags, ali_comment,
                            oldTs, oldEndTs, confidence)) {
    RawNlpRecord record;
    record.speakerId = speaker;
    record.casing = casing;
    record.punctuation = punctuation;
    if (input_nlp.has_column("prepunctuation")) {
      record.prepunctuation = prepunctuation;
    }
    record.ts = ts;
    record.endTs = endTs;
    record.best_label = GetBestLabel(tags);
    record.best_label_id = GetLabelId(record.best_label);
    record.labels = tags;
    record.token = token;
    if (input_nlp.has_column("wer_tags")) {
      record.wer_tags = GetWerTags(wer_tags);
    }
    if (input_nlp.has_column("confidence")) {
      record.confidence = confidence;
    }
    vect.push_back(record);
  }

  return vect;
}

std::string NlpReader::GetLabelId(std::string &label) {
  if (label.size() == 0) {
    return "";
  } else if (label.size() > 2) {
    auto pos = label.find(":");
    if (pos > 0) {
      return label.substr(0, pos);
    }
  }

  auto logger = logger::GetOrCreateLogger("NlpFstLoader");
  logger->error("about to throw because label = [{}]", label);
  throw "label not formatted as expected";
}

std::string NlpReader::GetBestLabel(std::string &labels) {
  if (labels == "[]") {
    return "";
  } else if (labels.size() > 2) {
    // raw tags string looks like : ['0:CARDINAL'] or ['0:CARDINAL','0:MONEY']
    std::string trimmed = labels.substr(1, labels.size() - 2);
    std::string first_label;

    // chop before the 1st comma, if needed
    auto pos = trimmed.find(",");
    if (pos > 0) {
      trimmed = trimmed.substr(0, pos - 2);
    }

    // remove the leading/ending single-quote
    return trimmed.substr(1, trimmed.size() - 2);
  }

  // TODO: add proper handling/logging
  // warning, it's not clear what to do at this point...
  return labels;
}

std::vector<std::string> NlpReader::GetWerTags(std::string &wer_tags_str) {
  std::vector<std::string> wer_tags;
  if (wer_tags_str == "[]") {
    return wer_tags;
  }
  // wer_tags_str looks like: ['89', '90', '100']
  int current_pos = 2;
  auto pos = wer_tags_str.find("'", current_pos);
  while (pos != -1) {
    std::string wer_tag = wer_tags_str.substr(current_pos, pos - current_pos);
    wer_tags.push_back(wer_tag);
    current_pos = wer_tags_str.find("'", pos + 1) + 1;
    if (current_pos == 0) {
      break;
    }
    pos = wer_tags_str.find("'", current_pos);
  }
  return wer_tags;
}

/*********************************** NLP Reader Class End
 * ***********************************/
