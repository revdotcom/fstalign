#include "wer.h"

#include "AlignmentTraversor.h"

using namespace std;

void RecordWerResult(Json::Value &json, WerResult wr) {
  json["insertions"] = wr.insertions;
  json["deletions"] = wr.deletions;
  json["substitutions"] = wr.substitutions;
  json["numWordsInReference"] = wr.numWordsInReference;
  json["numErrors"] = wr.NumErrors();
  json["wer"] = wr.WER();
}

void RecordWer(spWERA topAlignment) {
  auto logger = logger::GetOrCreateLogger("wer");
  logger->set_level(spdlog::level::info);

  if (topAlignment == nullptr) {
    logger->warn("No alignment produced, can't compute WER statistics");
    return;
  }

  logger->info("best WER: {0}/{1} = {2:.4f} (Total words in reference: {1})", topAlignment->NumErrors(),
               topAlignment->numWordsInReference, topAlignment->WER());
  logger->info("best WER: INS:{} DEL:{} SUB:{}", topAlignment->insertions, topAlignment->deletions,
               topAlignment->substitutions);
  logger->info("best WER: Precision:{:01.6f} Recall:{:01.6f}", topAlignment->precision, topAlignment->recall);

  jsonLogger::JsonLogger::getLogger().root["wer"]["bestWER"]["wer"] = topAlignment->WER();
  jsonLogger::JsonLogger::getLogger().root["wer"]["bestWER"]["numErrors"] = topAlignment->NumErrors();
  jsonLogger::JsonLogger::getLogger().root["wer"]["bestWER"]["numWordsInReference"] = topAlignment->numWordsInReference;
  jsonLogger::JsonLogger::getLogger().root["wer"]["bestWER"]["insertions"] = topAlignment->insertions;
  jsonLogger::JsonLogger::getLogger().root["wer"]["bestWER"]["deletions"] = topAlignment->deletions;
  jsonLogger::JsonLogger::getLogger().root["wer"]["bestWER"]["substitutions"] = topAlignment->substitutions;
  jsonLogger::JsonLogger::getLogger().root["wer"]["bestWER"]["precision"] = topAlignment->precision;
  jsonLogger::JsonLogger::getLogger().root["wer"]["bestWER"]["recall"] = topAlignment->recall;
  jsonLogger::JsonLogger::getLogger().root["wer"]["bestWER"]["meta"] = Json::objectValue;
  jsonLogger::JsonLogger::getLogger().root["wer"]["classWER"] = Json::objectValue;

  if (topAlignment->label_alignments.size() > 0) {
    logger->info(" -- class label information found --");
    // Maps label classes (CARDINAL, ORDINAL, etc.) to accumulated WER
    unordered_map<string, WerResult> class_results;
    int largestClassNameSize = 0;

    for (auto &a : topAlignment->label_alignments) {
      string class_label_id = a->classLabel;
      string label = GetLabelNameFromClassLabel(class_label_id);
      int numStitches = label.size();
      largestClassNameSize = std::max(largestClassNameSize, numStitches);

      if (class_results.find(label) == class_results.end()) {
        class_results[label] = {0, 0, 0, 0, 0};
      }

      class_results[label].numWordsInReference += a->numWordsInReference;
      class_results[label].insertions += a->insertions;
      class_results[label].deletions += a->deletions;
      class_results[label].substitutions += a->substitutions;
    }

    for (auto &a : class_results) {
      string label_name = a.first;
      WerResult wr = a.second;
      // logger->info("class {0:{4}}, WER: {1}/{2} = {3:.4f}", label_name, info.second,
      //  info.first, (float)info.second / (float)info.first, largestClassNameSize);
      logger->info("class {:{}} WER: {}/{} = {:.4f}", label_name, largestClassNameSize, wr.NumErrors(),
                   wr.numWordsInReference, wr.WER());
      RecordWerResult(jsonLogger::JsonLogger::getLogger().root["wer"]["classWER"][label_name], wr);
      jsonLogger::JsonLogger::getLogger().root["wer"]["classWER"][label_name]["meta"] = Json::objectValue;
    }
  }
}

void RecordSpeakerWer(vector<shared_ptr<Stitching>> stitches) {
  // Note: stitches must have already been aligned to NLP rows
  // Logic for segment boundaries copied from speaker switch WER code
  auto logger = logger::GetOrCreateLogger("wer");
  logger->set_level(spdlog::level::info);

  unordered_map<string, WerResult> speaker_wers;
  string last_speaker = "";
  int insertions = 0;
  int deletions = 0;
  int substitutions = 0;
  int num_words_in_reference = 0;
  for (auto &stitch : stitches) {
    string speaker_id = stitch->nlpRow.speakerId;
    if (!speaker_id.empty() && speaker_id != last_speaker) {
      if (!last_speaker.empty()) {
        // Accumulated counts for this segment (speaker switch boundaries)
        // We accumulate by segment to assign speakers to insertions
        if (speaker_wers.find(last_speaker) == speaker_wers.end()) {
          speaker_wers[last_speaker] = {0, 0, 0, 0, 0};
        }
        speaker_wers[last_speaker].insertions += insertions;
        speaker_wers[last_speaker].deletions += deletions;
        speaker_wers[last_speaker].substitutions += substitutions;
        speaker_wers[last_speaker].numWordsInReference += num_words_in_reference;

        insertions = 0;
        deletions = 0;
        substitutions = 0;
        num_words_in_reference = 0;
      }
      last_speaker = speaker_id;
    }

    insertions += stitch->comment.rfind("ins", 0) == 0;
    deletions += stitch->comment.rfind("del", 0) == 0;
    substitutions += stitch->comment.rfind("sub", 0) == 0;
    num_words_in_reference += speaker_id.empty() ? 0 : 1;
  }

  if (speaker_wers.find(last_speaker) == speaker_wers.end()) {
    speaker_wers[last_speaker] = {0, 0, 0, 0, 0};
  }
  speaker_wers[last_speaker].insertions += insertions;
  speaker_wers[last_speaker].deletions += deletions;
  speaker_wers[last_speaker].substitutions += substitutions;
  speaker_wers[last_speaker].numWordsInReference += num_words_in_reference;

  for (auto &a : speaker_wers) {
    string speaker_id = a.first;
    WerResult wr = a.second;
    logger->info("speaker {} WER: {}/{} = {:.4f}", speaker_id, wr.NumErrors(), wr.numWordsInReference, wr.WER());
    RecordWerResult(jsonLogger::JsonLogger::getLogger().root["wer"]["speakerWER"][speaker_id], wr);
    jsonLogger::JsonLogger::getLogger().root["wer"]["speakerWER"][speaker_id]["meta"] = Json::objectValue;
  }
}

void UpdateHypCorrectAndAllwords(const spWERA &topAlignment, map<string, uint64_t> &hyp_words_counts,
                                 map<string, uint64_t> &correct_words_counts, set<string> &all_words) {
  for (auto &tokens : topAlignment->tokens) {
    bool bigram_valid = isValidNgram(tokens.second);
    if ((tokens.first == tokens.second) && bigram_valid) {
      correct_words_counts[tokens.first] += 1;
      all_words.insert(tokens.first);
    }

    if ((tokens.first != tokens.second) && bigram_valid) {
      hyp_words_counts[tokens.second] += 1;
      all_words.insert(tokens.second);
    }

    // Handle entity labels
    if (isEntityLabel(tokens.first)) {
      auto class_label = tokens.first;

      for (auto &label_alignment : topAlignment->label_alignments) {
        if (label_alignment->classLabel == class_label) {
          UpdateHypCorrectAndAllwords(label_alignment, hyp_words_counts, correct_words_counts, all_words);
          break;
        }
      }
    }
  }
}

vector<int> GetSpeakerSwitchIndices(vector<shared_ptr<Stitching>> stitches) {
  // Note: stitches must have already been aligned to NLP rows
  vector<int> speakerSwitches;
  std::string lastSpeaker = "";
  int nlpRowIndex = 0;
  for (auto &stitch : stitches) {
    std::string speaker = stitch->nlpRow.speakerId;
    if (!speaker.empty() && speaker != lastSpeaker) {
      if (!lastSpeaker.empty()) {
        speakerSwitches.push_back(nlpRowIndex);
      }
      lastSpeaker = speaker;
    }
    if (stitch->comment.rfind("ins", 0) != 0) {
      nlpRowIndex++;
    }
  }
  return speakerSwitches;
}

void RecordSpeakerSwitchWer(vector<shared_ptr<Stitching>> stitches, int speaker_switch_context_size) {
  // Speaker switch WER is logged to logger and JSON logger
  auto logger = logger::GetOrCreateLogger("wer");
  logger->set_level(spdlog::level::info);

  vector<int> switch_indices = GetSpeakerSwitchIndices(stitches);

  int nlpRowIndex = 0;
  int switchNum = 0;
  WerResult wer = {0, 0, 0, 0, 0};
  for (auto &stitch : stitches) {
    if (switchNum < switch_indices.size()) {
      bool del = stitch->comment.rfind("del", 0) == 0;
      bool ins = stitch->comment.rfind("ins", 0) == 0;
      bool sub = stitch->comment.rfind("sub", 0) == 0;
      if (nlpRowIndex >= switch_indices[switchNum] - speaker_switch_context_size &&
          nlpRowIndex < switch_indices[switchNum] + speaker_switch_context_size) {
        if (!ins) {
          wer.numWordsInReference++;
        }

        if (del) {
          wer.deletions++;
        } else if (sub) {
          wer.substitutions++;
        } else if (ins && nlpRowIndex != switch_indices[switchNum] - speaker_switch_context_size) {
          // Edge case: insertions at the start of the speaker switch context (nlp row hasn't officially started)
          wer.insertions++;
        }
      }
      if (!ins) {
        nlpRowIndex++;
      }
      if (nlpRowIndex >= switch_indices[switchNum] + speaker_switch_context_size) {
        switchNum++;
      }
    } else {
      break;
    }
  }

  logger->info("Speaker switch WER: {0}/{1} = {2:.4f} (Total reference words: {1})", wer.NumErrors(),
               wer.numWordsInReference, wer.WER());
  logger->info("Speaker switch WER: INS:{} DEL:{} SUB:{}", wer.insertions, wer.deletions, wer.substitutions);
  RecordWerResult(jsonLogger::JsonLogger::getLogger().root["wer"]["speakerSwitchWER"], wer);
  jsonLogger::JsonLogger::getLogger().root["wer"]["speakerSwitchWER"]["meta"]["windowSize"] =
      speaker_switch_context_size;
}

void RecordCaseWer(vector<shared_ptr<Stitching>> aligned_stitches) {
  auto logger = logger::GetOrCreateLogger("wer");
  logger->set_level(spdlog::level::info);
  int true_positive = 0;  // h is upper and r is upper
  int false_positive = 0;  // h is upper and r is lower
  int false_negative = 0;  // h is lower and r is upper
  int sub_tp(0), sub_fp(0), sub_fn(0);

  for (auto &&stitch : aligned_stitches) {
    string hyp = stitch->hyp_orig;
    string ref = stitch->nlpRow.token;
    string reftk = stitch->reftk;
    string hyptk = stitch->hyptk;
    string ref_casing = stitch->nlpRow.casing;

    if (hyptk == DEL || reftk == INS) {
      continue;
    }

    // Calculate false_positive/true_positive/false_negative at token level instead of character level
    // This is to keep it more consistent with WER, which is also at token level
    // A false positive is where there are more uppercase in the hypothesis than the ref
    // A false negative is the opposite
    // A true positive matches
    if (reftk == hyptk) {
      if (ref_casing == "LC") {
        for (auto &&c : hyp) {
          if (isupper(c)) {
            false_positive++;
            break;
          }
        }
      } else {
        int ref_upper = 0;
        for (int i = 0; i < ref.size(); i++) {
          if (isupper(ref[i])) ref_upper++;
        }
        int hyp_upper = 0;
        for (int i = 0; i < hyp.size(); i++) {
          if (isupper(hyp[i])) hyp_upper++;
        }
        if (ref_upper == hyp_upper) true_positive++;
        else if (ref_upper > hyp_upper) false_negative++;
        else if (ref_upper < hyp_upper) false_positive++;
      }
    } else {
      if (ref_casing == "LC") {
        for (auto &&c : hyp) {
          if (isupper(c)) {
            sub_fp++;
            break;
          }
        }
      } else {
        int ref_upper = 0;
        for (int i = 0; i < ref.size(); i++) {
          if (isupper(ref[i])) ref_upper++;
        }
        int hyp_upper = 0;
        for (int i = 0; i < hyp.size(); i++) {
          if (isupper(hyp[i])) hyp_upper++;
        }
        if (ref_upper == hyp_upper) sub_tp++;
        else if (ref_upper > hyp_upper) sub_fn++;
        else if (ref_upper < hyp_upper) sub_fp++;
      }
    }
  }

  float base_precision = float(true_positive) / float(true_positive + false_positive);
  float base_recall = float(true_positive) / float(true_positive + false_negative);
  float precision_with_sub = float(true_positive + sub_tp) / float(true_positive + sub_tp + false_positive + sub_fp);
  float recall_with_sub = float(true_positive + sub_tp) / float(true_positive + sub_tp + false_negative + sub_fn);

  logger->info("case WER, (matching words only): Precision:{:01.6f} Recall:{:01.6f}", base_precision, base_recall);
  logger->info("case WER, (all including substitutions): Precision:{:01.6f} Recall:{:01.6f}", precision_with_sub, recall_with_sub);

  jsonLogger::JsonLogger::getLogger().root["wer"]["caseWER"]["matching"]["precision"] = base_precision;
  jsonLogger::JsonLogger::getLogger().root["wer"]["caseWER"]["matching"]["recall"] = base_recall;
  jsonLogger::JsonLogger::getLogger().root["wer"]["caseWER"]["matching"]["true_positive"] = true_positive;
  jsonLogger::JsonLogger::getLogger().root["wer"]["caseWER"]["matching"]["false_positive"] = false_positive;
  jsonLogger::JsonLogger::getLogger().root["wer"]["caseWER"]["matching"]["false_negative"] = false_negative;

  jsonLogger::JsonLogger::getLogger().root["wer"]["caseWER"]["all"]["precision"] = precision_with_sub;
  jsonLogger::JsonLogger::getLogger().root["wer"]["caseWER"]["all"]["recall"] = recall_with_sub;
  jsonLogger::JsonLogger::getLogger().root["wer"]["caseWER"]["all"]["true_positive"] = sub_tp + true_positive;
  jsonLogger::JsonLogger::getLogger().root["wer"]["caseWER"]["all"]["false_positive"] = sub_fp + false_positive;
  jsonLogger::JsonLogger::getLogger().root["wer"]["caseWER"]["all"]["false_negative"] = sub_fn + false_negative;
}

void RecordTagWer(vector<shared_ptr<Stitching>> stitches) {
  // Record per wer_tag ID stats
  auto logger = logger::GetOrCreateLogger("wer");
  logger->set_level(spdlog::level::info);
  std::map<std::string, WerResult> wer_results;

  for (auto &stitch : stitches) {
    if (!stitch->nlpRow.wer_tags.empty()) {
      for (auto wer_tag : stitch->nlpRow.wer_tags) {
        int tag_start = wer_tag.find_first_not_of('#');
        int tag_end = wer_tag.find('_');
        string wer_tag_id = wer_tag.substr(tag_start, tag_end - tag_start);
        wer_results.insert(std::pair<std::string, WerResult>(wer_tag_id, {0, 0, 0, 0, 0}));
        // Check with rfind since other comments can be there
        bool del = stitch->comment.rfind("del", 0) == 0;
        bool ins = stitch->comment.rfind("ins", 0) == 0;
        bool sub = stitch->comment.rfind("sub", 0) == 0;
        wer_results[wer_tag_id].insertions += ins;
        wer_results[wer_tag_id].deletions += del;
        wer_results[wer_tag_id].substitutions += sub;
        if (!ins) {
          wer_results[wer_tag_id].numWordsInReference += 1;
        }
      }
    }
  }
  for (auto &a : wer_results) {
    string wer_id = a.first;
    WerResult wr = a.second;
    logger->info("Wer Entity ID {} WER: {}/{} = {:.4f}", wer_id, wr.NumErrors(), wr.numWordsInReference, wr.WER());
    RecordWerResult(jsonLogger::JsonLogger::getLogger().root["wer"]["wer_tag"][wer_id], wr);
    jsonLogger::JsonLogger::getLogger().root["wer"]["wer_tag"][wer_id]["meta"] = Json::objectValue;
  }
}

void CalculatePrecisionRecall(spWERA &topAlignment, int threshold) {
  int correct = topAlignment->numWordsInReference - topAlignment->deletions - topAlignment->substitutions;
  topAlignment->precision = (float)correct / (correct + topAlignment->insertions + topAlignment->substitutions);
  topAlignment->recall = (float)correct /  // Recall = C /(C + S + D)
                         (correct + topAlignment->substitutions + topAlignment->deletions);

  int total = 0;  //= topAlignment->ref_words.size();
  correct = 0;
  int del = 0;
  int subst_fp = 0;
  int subst_fn = 0;
  int ins = 0;
  int hyp = 0;
  precision_t precision = 0;
  recall_t recall = 0;
  set<string> all_words;
  // parse words (unigrams) and get its p&r
  map<string, uint64_t> correct_words_counts;
  map<string, uint64_t> hyp_words_counts;
  map<string, uint64_t> del_words_counts;
  map<string, uint64_t> subst_fp_words_counts;
  map<string, uint64_t> subst_fn_words_counts;
  map<string, uint64_t> ins_words_counts;
  UpdateHypCorrectAndAllwords(topAlignment, hyp_words_counts, correct_words_counts, all_words);

  for (auto &word : topAlignment->del_words) {
    del_words_counts[word] += 1;
    if (all_words.find(word) == all_words.end()) all_words.insert(word);
  }
  for (auto &word : topAlignment->sub_words) {
    subst_fn_words_counts[word.first] += 1;
    subst_fp_words_counts[word.second] += 1;
    if (all_words.find(word.first) == all_words.end()) all_words.insert(word.first);
    if (all_words.find(word.second) == all_words.end()) all_words.insert(word.second);
  }
  for (auto &word : topAlignment->ins_words) {
    ins_words_counts[word] += 1;
    if (all_words.find(word) == all_words.end()) {
      all_words.insert(word);
    }
  }

  for (auto &w : all_words) {
    correct = del = subst_fp = subst_fn = ins = 0;

    if (correct_words_counts.find(w) != correct_words_counts.end()) correct = correct_words_counts.find(w)->second;
    if (del_words_counts.find(w) != del_words_counts.end()) del = del_words_counts.find(w)->second;
    if (subst_fn_words_counts.find(w) != subst_fn_words_counts.end()) subst_fn = subst_fn_words_counts.find(w)->second;
    if (subst_fp_words_counts.find(w) != subst_fp_words_counts.end()) subst_fp = subst_fp_words_counts.find(w)->second;
    if (ins_words_counts.find(w) != ins_words_counts.end()) ins = ins_words_counts.find(w)->second;

    gram_error_counter error_counter(correct, del, subst_fp, subst_fn, ins);
    if (correct + ins + subst_fp != 0)
      error_counter.precision = (float)((float)correct / (float)(correct + ins + subst_fp)) * 100;
    if ((correct + del + subst_fn) != 0)
      error_counter.recall = (float)((float)correct / (float)(correct + del + subst_fn)) * 100;

    if ((correct + ins + del + subst_fp + subst_fn) >= threshold) {
      topAlignment->unigram_stats.push_back(std::make_pair(w, error_counter));
    }
  }

  // sort in asceding order with precision then recall then word
  auto precision_recall_compare = [](const pair<string, gram_error_counter> &l,
                                     const pair<string, gram_error_counter> &r) {
    if (l.second.precision != r.second.precision) {
      return l.second.precision < r.second.precision;
    } else if (l.second.recall != r.second.recall) {
      return l.second.recall < r.second.recall;
    } else {
      return l.first < r.first;
    }
  };

  std::sort(topAlignment->unigram_stats.begin(), topAlignment->unigram_stats.end(), precision_recall_compare);

  unordered_set<string> all_bigrams = get_bigrams(topAlignment);
  map<string, uint64_t> correct_bigram_counts;
  map<string, uint64_t> del_bigram_counts;
  map<string, uint64_t> ins_bigram_counts;
  map<string, uint64_t> subst_fn_bigram_counts;
  map<string, uint64_t> subst_fp_bigram_counts;

  for (auto &bigram_tokens : topAlignment->bigram_tokens) {
    auto bigram_ref = bigram_tokens.first;
    auto bigram_hyp = bigram_tokens.second;

    if (bigram_ref == bigram_hyp) {
      correct_bigram_counts[bigram_ref] += 1;
    } else if (isValidNgram(bigram_ref) && isValidNgram(bigram_hyp)) {
      subst_fn_bigram_counts[bigram_ref] += 1;
      subst_fp_bigram_counts[bigram_hyp] += 1;
    }
    if (bigram_hyp.find(DEL) != string::npos) {
      del_bigram_counts[bigram_ref] += 1;
    }
    if (bigram_ref.find(INS) != string::npos) {
      ins_bigram_counts[bigram_hyp] += 1;
    }
  }
  // parse words, get bigrams and get its p&r
  for (auto &b : all_bigrams) {
    correct = del = subst_fn = subst_fp = ins = 0;

    if (correct_bigram_counts.find(b) != correct_bigram_counts.end()) {
      correct = correct_bigram_counts[b];
    }
    if (del_bigram_counts.find(b) != del_bigram_counts.end()) {
      del = del_bigram_counts[b];
    }
    if (ins_bigram_counts.find(b) != ins_bigram_counts.end()) {
      ins = ins_bigram_counts[b];
    }
    if (subst_fn_bigram_counts.find(b) != subst_fn_bigram_counts.end()) {
      subst_fn = subst_fn_bigram_counts[b];
    }
    if (subst_fp_bigram_counts.find(b) != subst_fp_bigram_counts.end()) {
      subst_fp = subst_fp_bigram_counts[b];
    }

    gram_error_counter error_counter(correct, del, subst_fp, subst_fn, ins);

    if (correct + ins + subst_fp != 0) error_counter.precision = ((float)correct / (float)(correct + ins + subst_fp)) * 100;
    if ((correct + del + subst_fn) != 0) error_counter.recall = ((float)correct / (float)(correct + del + subst_fn)) * 100;
    if ((correct + ins + del + subst_fn + subst_fp) >= threshold) {
      topAlignment->bigrams_stats.push_back(std::make_pair(b, error_counter));
    }
  }

  // sort in asceding order with precision then recall
  std::sort(topAlignment->bigrams_stats.begin(), topAlignment->bigrams_stats.end(), precision_recall_compare);

  return;
}

void AddErrorGroup(ErrorGroups &groups, size_t &line, string &ref, string &hyp) {
  // fill for empty sides
  if (ref.empty()) {
    ref = "*** ";
  }
  if (hyp.empty()) {
    hyp = " ***";
  }

  // add group
  groups.emplace_back(line, fmt::format("{0}<->{1}", ref, hyp));

  // init for next group
  line = 0;
  ref = "";
  hyp = "";
}

void WriteSbs(spWERA topAlignment, vector<shared_ptr<Stitching>> stitches, string sbs_filename) {
  auto logger = logger::GetOrCreateLogger("wer");
  logger->set_level(spdlog::level::info);

  ofstream myfile;
  myfile.open(sbs_filename);

  AlignmentTraversor visitor(topAlignment);
  triple *tk_pair = new triple();
  string prev_tk_classLabel = "";
  logger->info("Side-by-Side alignment info going into {}", sbs_filename);
  myfile << fmt::format("{0:>20}\t{1:20}\t{2}\t{3}\t{4}", "ref_token", "hyp_token", "IsErr", "Class", "Wer_Tag_Entities") << endl;

  // keep track of error groupings
  ErrorGroups groups_err;
  size_t line_err = 0;
  string ref_err = "";
  string hyp_err = "";

  std::set<std::string> op_set = {"<ins>", "<del>", "<sub>"};

  size_t offset = 2;  // line number in output file where first triple starts
  for (auto p_stitch: stitches) {
    string tk_classLabel = p_stitch->classLabel;
    string tk_wer_tags = "";
    auto wer_tags = p_stitch->nlpRow.wer_tags;
    for (auto wer_tag: wer_tags) {
      tk_wer_tags = tk_wer_tags + wer_tag + "|";
    }
    string ref_tk = p_stitch->reftk;
    string hyp_tk = p_stitch->hyptk;
    string tag = "";

    if (ref_tk == NOOP) {
      continue;
    }

    // is this an error
    if (ref_tk != hyp_tk) {
      tag = "ERR";

      // is this the start of a new group
      if (line_err == 0) {
        line_err = offset;
      }

      // keep track of non-op words
      if (!ref_tk.empty() && op_set.find(ref_tk) == op_set.end()) {
        ref_err += ref_tk + " ";
      }
      if (!hyp_tk.empty() && op_set.find(hyp_tk) == op_set.end()) {
        hyp_err += " " + hyp_tk;
      }
    } else if (line_err > 0) {
      AddErrorGroup(groups_err, line_err, ref_err, hyp_err);
    }

    string eff_class = "";
    if (tk_classLabel != TK_GLOBAL_CLASS) {
      eff_class = tk_classLabel;
    }

    myfile << fmt::format("{0:>20}\t{1:20}\t{2}\t{3}\t{4}", ref_tk, hyp_tk, tag, eff_class, tk_wer_tags) << endl;
    offset++;
  }

  // add final group (if any)
  if (line_err > 0) {
    AddErrorGroup(groups_err, line_err, ref_err, hyp_err);
  }

  // output error groups
  myfile << string(60, '-') << endl << fmt::format("{0:>20}\t{1:20}", "Line", "Group") << endl;

  for (const auto &group : groups_err) {
    myfile << fmt::format("{0:>20}\t{1}", group.first, group.second) << endl;
  }

  for (const auto &a : topAlignment->unigram_stats) {
    string word = a.first;
    gram_error_counter u = a.second;
    jsonLogger::JsonLogger::getLogger().root["wer"]["unigrams"][word]["correct"] = u.correct;
    jsonLogger::JsonLogger::getLogger().root["wer"]["unigrams"][word]["insertions"] = u.ins;
    jsonLogger::JsonLogger::getLogger().root["wer"]["unigrams"][word]["deletions"] = u.del;
    jsonLogger::JsonLogger::getLogger().root["wer"]["unigrams"][word]["substitutions_fp"] = u.subst_fp;
    jsonLogger::JsonLogger::getLogger().root["wer"]["unigrams"][word]["substitutions_fn"] = u.subst_fn;
    jsonLogger::JsonLogger::getLogger().root["wer"]["unigrams"][word]["precision"] = u.precision;
    jsonLogger::JsonLogger::getLogger().root["wer"]["unigrams"][word]["recall"] = u.recall;
  }
  // output error unigrams
  myfile << string(60, '-') << endl << fmt::format("{0:>20}\t{1:10}\t{2:10}", "Unigram", "Prec.", "Recall") << endl;
  for (const auto &a : topAlignment->unigram_stats) {
    string word = a.first;
    gram_error_counter u = a.second;
    myfile << fmt::format("{0:>20}\t{1}/{2} ({3:.1f} %)\t{4}/{5} ({6:.1f} %)", word, u.correct,
                          (u.correct + u.ins + u.subst_fp), (float)u.precision, u.correct, (u.correct + u.del + u.subst_fn),
                          (float)u.recall)
           << endl;
  }

  myfile << string(60, '-') << endl << fmt::format("{0:>20}\t{1:20}\t{2:20}", "Bigram", "Precision", "Recall") << endl;

  for (const auto &a : topAlignment->bigrams_stats) {
    string word = a.first;
    gram_error_counter u = a.second;
    jsonLogger::JsonLogger::getLogger().root["wer"]["bigrams"][word]["correct"] = u.correct;
    jsonLogger::JsonLogger::getLogger().root["wer"]["bigrams"][word]["insertions"] = u.ins;
    jsonLogger::JsonLogger::getLogger().root["wer"]["bigrams"][word]["deletions"] = u.del;
    jsonLogger::JsonLogger::getLogger().root["wer"]["bigrams"][word]["substitutions_fp"] = u.subst_fp;
    jsonLogger::JsonLogger::getLogger().root["wer"]["bigrams"][word]["substitutions_fn"] = u.subst_fn;
    jsonLogger::JsonLogger::getLogger().root["wer"]["bigrams"][word]["precision"] = u.precision;
    jsonLogger::JsonLogger::getLogger().root["wer"]["bigrams"][word]["recall"] = u.recall;
  }
  for (const auto &a : topAlignment->bigrams_stats) {
    string word = a.first;
    gram_error_counter u = a.second;
    myfile << fmt::format("{0:>20}\t{1}/{2} ({3:.1f} %)\t{4}/{5} ({6:.1f} %)", word, u.correct,
                          (u.correct + u.ins + u.subst_fp), (float)u.precision, u.correct, (u.correct + u.del + u.subst_fn),
                          (float)u.recall)
           << endl;
  }

  myfile.close();
}
