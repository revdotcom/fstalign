/*
 *
 * SynonymEngine.cpp
 *
 * JP Robichaud (jp@rev.com)
 * 2018
 *
 */
#include "SynonymEngine.h"

#define strtk_no_tr1_or_boost
#include <strtk/strtk.hpp>

SynonymEngine::SynonymEngine(SynonymOptions syn_opts) {
  map<SynKey, SynVals> synonyms;
  opts_ = syn_opts;
  logger_ = logger::GetOrCreateLogger("SynonymEngine");
}

void SynonymEngine::LoadFile(string filename) {
  ifstream input(filename);

  if (!input.is_open()) throw std::runtime_error("Cannot open input file");

  vector<string> lines;
  string s;
  while (std::getline(input, s)) {
    lines.push_back(s);
  }
  input.close();

  ParseStrings(lines);
}

SynKey SynonymEngine::GetKeyFromString(string lhs) {
  //   SynKey k;
  vector<string> k;
  // splitStringIter2(lhs, " ", std::back_inserter(k));
  strtk::split(" ", lhs, strtk::range_to_type_back_inserter(k), strtk::split_options::compress_delimiters);

  return k;
}

SynVals SynonymEngine::GetValuesFromStrings(string rhs) {
  vector<string> alternatives;
  // splitStringIter2(rhs, "|", back_inserter(alternatives));
  strtk::split(";", rhs, strtk::range_to_type_back_inserter(alternatives), strtk::split_options::compress_delimiters);

  logger_->debug("with rhs = [{}], we have {} alternatives", rhs, alternatives.size());

  SynVals values;
  for (auto &alt : alternatives) {
    vector<string> tokens;
    // splitStringIter2(alt, " ", back_inserter(token));
    strtk::split(" ", trim_copy(alt), strtk::range_to_type_back_inserter(tokens),
                 strtk::split_options::compress_delimiters);

    logger_->debug("   for alt [{}], we have {} tokens", alt, tokens.size());
    values.push_back(tokens);
  }

  return values;
}

void SynonymEngine::ParseStrings(vector<string> lines) {
  vector<string> parts;
  for (auto &line : lines) {
    trim(line);
    if (line.empty() || line.find('#') == 0) {
      // skipping comments or empty lines
      continue;
    }

    parts.clear();
    strtk::split("|", line, strtk::range_to_type_back_inserter(parts), strtk::split_options::compress_delimiters);

    if (parts.size() != 2) {
      logger_->warn("strangly formatted line, skipping: {}", line);
      continue;
    }

    // LHS: the word(s) to match in the fst
    // RHS: a ;-delimited list of equivalent expressions
    string lhs = parts[0];
    string rhs = parts[1];
    // parts.erase(parts.begin());

    trim(lhs);
    auto key = GetKeyFromString(lhs);

    if (synonyms.find(key) == synonyms.end()) {
      // new entry
      auto values = GetValuesFromStrings(rhs);
      synonyms[key] = values;
    } else {
      logger_->warn(
          "WARNING: the entry [{}] was found multiple time, skipping "
          "redefinition",
          lhs);
    }
  }
}

vector<int> SeekForward(SynKey &lhs, int lhsPos, StdVectorFst &fst, int startingStateId, SymbolTable &symbol) {
  vector<int> lastStates;

  int stateId = startingStateId;
  if (lhsPos >= lhs.size()) {
    // we reached the end of the LHS stack, this node is our
    // destination
    lastStates.push_back(startingStateId);
    return lastStates;
  }

  int currKeyPartLabelId = symbol.Find(lhs[lhsPos]);
  int kNoSymbol = -1;
  if (currKeyPartLabelId == kNoSymbol) {
    return lastStates;
  }

  for (ArcIterator<StdFst> aiter(fst, stateId); !aiter.Done(); aiter.Next()) {
    const fst::StdArc &arc = aiter.Value();
    if (arc.nextstate == stateId) {
      // if we're pointing to ourselves, let's ignore that
      continue;
    }

    int label_id = arc.ilabel;
    if (label_id == currKeyPartLabelId) {
      auto endStates = SeekForward(lhs, lhsPos + 1, fst, arc.nextstate, symbol);
      copy(endStates.begin(), endStates.end(), back_inserter(lastStates));
    }
  }

  return lastStates;
}

void SynonymEngine::GenerateSynFromSymbolTable(SymbolTable &symbol) {
  logger_->debug("Adding synonyms dynamically from symbol table.");
  int kNoSymbol = -1;

  int cutoff_count = 0;
  int compound_hyphen_count = 0;

  SymbolTableIterator symIter(symbol);
  while (!symIter.Done()) {
    auto sym = symIter.Symbol();
    symIter.Next();
    int hyphen_idx = sym.find('-');
    if (!opts_.disable_cutoffs && hyphen_idx == sym.length() - 1) {
      // Cutoff rules take precedence
      auto new_word = sym.substr(0, sym.size() - 1);
      int id = symbol.Find(new_word);
      if (id == kNoSymbol) {
        id = symbol.AddSymbol(new_word);
      }
      auto key = GetKeyFromString(sym);
      auto values = GetValuesFromStrings(new_word);
      if (synonyms.find(key) == synonyms.end()) {
        // Only add the cutoff synonym if no synonym is already defined
        synonyms[key] = values;
      }
      cutoff_count++;
    } else if (!opts_.disable_hyphen_ignore && hyphen_idx != -1 && hyphen_idx != sym.length() - 1) {
      // Generate other hyphenation rules
      vector<string> new_words;
      strtk::split("-", trim_copy(sym), strtk::range_to_type_back_inserter(new_words),
                   strtk::split_options::compress_delimiters);

      vector<string> hyphenated = GetKeyFromString(sym);

      // Add new subwords if they didn't exist in the table
      for (auto w : new_words) {
        int id = symbol.Find(w);
        if (id == kNoSymbol) {
          id = symbol.AddSymbol(w);
        }
      }

      if (synonyms.find(hyphenated) == synonyms.end()) {
        // Add hyphenated --> unhyphenated synonym
        synonyms[hyphenated] = {new_words};
      }
      if (synonyms.find(new_words) == synonyms.end()) {
        // Add unhyphenated --> hyphenated synonym
        synonyms[new_words] = {hyphenated};
      }
      compound_hyphen_count++;
    }
  }
  logger_->debug("Found {} cutoff words and added their synonyms", cutoff_count);
  logger_->debug("Found {} compound hyphen words and added their synonyms", compound_hyphen_count);
  return;
}

void SynonymEngine::ApplyToFst(StdVectorFst &fst, SymbolTable &symbol) {
  int kNoSymbol = -1;

  // let's get the list of all words that 'start' a synonym
  // expression
  // set<string> firstWordInRules;
  map<int, vector<SynKey>> firstWordInRules;
  for (auto entry : synonyms) {
    auto tk = entry.first[0];
    // there's no point inserting synonym rules if
    // the original fst doesn't have it
    int id = symbol.Find(tk);
    if (id != kNoSymbol) {
      if (firstWordInRules.find(id) == firstWordInRules.end()) {
        firstWordInRules[id] = vector<SynKey>();
      }

      firstWordInRules[id].push_back(entry.first);
    }
  }

  logger_->info("we have {} registered first word rules label id", firstWordInRules.size());

  StateIterator<StdVectorFst> siter(fst);
  vector<pair<int, StdArc>> arcsToAdd;
  int synonym_replacement_id = 100000;
  while (!siter.Done()) {
    int stateId = siter.Value();
    siter.Next();
    for (ArcIterator<StdVectorFst> aiter(fst, stateId); !aiter.Done(); aiter.Next()) {
      const StdArc &arc = aiter.Value();
      if (arc.nextstate == stateId) {
        // if we're pointing to ourselves, let's ignore that
        continue;
      }

      int label_id = arc.ilabel;
      auto firstWordEntry = firstWordInRules.find(label_id);
      if (firstWordEntry == firstWordInRules.end()) {
        // no match, let's move
        continue;
      }

      // we have a match!
      // for each of the candidates, check if the graph supports it.
      // if it does, then add arcs from s to the last state required
      // to consume the LHS and add it to the arcsToAdd list
      vector<SynKey> candidates = (*firstWordEntry).second;
      logger_->debug("for state {} and label id {} we have {} candidates", stateId, label_id, candidates.size());
      for (auto &lhs : candidates) {
        auto lastTargetStates = SeekForward(lhs, 0, fst, stateId, symbol);
        if (lastTargetStates.size() == 0) {
          // no match, let's continue
          continue;
        }
        logger_->debug("for label id {} we have {} next states", label_id, lastTargetStates.size());

        for (auto &alternative : synonyms[lhs]) {
          int currState = stateId;
          int nextState = 0;

          string syn_classlabel = "___" + to_string(synonym_replacement_id++) + "_SYN_" + to_string(lhs.size()) + "-" +
                                  to_string(alternative.size()) + "___";

          int syn_classlabel_wid = symbol.AddSymbol(syn_classlabel);
          nextState = fst.AddState();
          StdArc newArc(syn_classlabel_wid, syn_classlabel_wid, 0.0f, nextState);
          pair<int, StdArc> local_pair;
          local_pair = make_pair(currState, newArc);
          arcsToAdd.push_back(local_pair);
          currState = nextState;

          // one alternative contains all the words for one synonym rule
          for (int j = 0; j < alternative.size(); j++) {
            auto w = alternative[j];
            int wid = symbol.Find(w);
            if (wid == kNoSymbol) {
              wid = symbol.AddSymbol(w);
              logger_->info("registering word {} as {}", w, wid);
            }

            nextState = fst.AddState();
            StdArc newArc(wid, wid, 0.0f, nextState);
            pair<int, StdArc> local_pair;
            local_pair = make_pair(currState, newArc);
            arcsToAdd.push_back(local_pair);
            currState = nextState;
          }
          // this is the last word of the synonym, we want to tie
          // it to all possible end states for the LHS
          for (auto &kk : lastTargetStates) {
            StdArc newArc(syn_classlabel_wid, syn_classlabel_wid, 0.0f, kk);
            pair<int, StdArc> local_pair;
            local_pair = make_pair(currState, newArc);
            arcsToAdd.push_back(local_pair);
          }
        }
      }
    }
  }

  for (auto &p : arcsToAdd) {
    logger_->debug("adding arc between {} and {} with label_id {}", p.first, p.second.nextstate, p.second.ilabel);
    fst.AddArc(p.first, p.second);
  }
}
