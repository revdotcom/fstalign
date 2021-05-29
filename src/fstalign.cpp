/*
fstalign.cpp
 JP Robichaud (jp@rev.com)
 2018

*/

#include "fstalign.h"

#include <stdio.h>

#include <spdlog/fmt/fmt.h>

#include "AdaptedComposition.h"
#include "OneBestFstLoader.h"
#include "StandardComposition.h"
#include "Walker.h"
#include "json_logging.h"
#include "utilities.h"
#include "wer.h"

using namespace std;
using namespace fst;

// Compare class for comparing output labels of arcs.
template <class Arc>
class ReverseOLabelCompare {
 public:
  constexpr ReverseOLabelCompare() {}

  constexpr bool operator()(const Arc &lhs, const Arc &rhs) const {
    return std::forward_as_tuple(lhs.olabel, lhs.ilabel) > std::forward_as_tuple(rhs.olabel, rhs.ilabel);
  }

  constexpr uint64 Properties(uint64 props) const {
    return (props & kArcSortProperties) | kOLabelSorted | (props & kAcceptor ? kILabelSorted : 0);
  }
};

using StdReverseOlabelCompare = ReverseOLabelCompare<StdArc>;

bool sort_alignment(spWERA a, spWERA b) { return a->WER() < b->WER(); }

spWERA Fstalign(FstLoader *refLoader, FstLoader *hypLoader, SynonymEngine *engine, int numBests,
                string symbols_filename, string composition_approach) {
  auto logger = logger::GetOrCreateLogger("fstalign");
  FstAlignOption options;
  SymbolTable symbol;
  if (!symbols_filename.empty()) {
    std::ifstream strm(symbols_filename, std::ios_base::in);
    symbol = *(SymbolTable::ReadText(strm, "symbols"));
  } else {
    symbol = SymbolTable("symbols");
  }
  options.RegisterSymbols(symbol);

  refLoader->addToSymbolTable(symbol);
  hypLoader->addToSymbolTable(symbol);

  auto refFst = refLoader->convertToFst(symbol);
  auto hypFst = hypLoader->convertToFst(symbol);
  if (engine != nullptr) {
    logger->info("applying ref synonyms on ref fst");
    engine->apply_to_fst(refFst, symbol);
    ArcSort(&refFst, StdILabelCompare());
  }

  logger->info("printing ref fst");
  if (refFst.NumStates() > 100) {
    logger->info("fst is too large to be printed on the console");
  } else {
    printFst("fstalign", &refFst, &symbol);
  }

  logger->info("printing hyp fst");
  if (hypFst.NumStates() > 100) {
    logger->info("fst is too large to be printed on the console");
  } else {
    printFst("fstalign", &hypFst, &symbol);
  }

  vector<shared_ptr<wer_alignment>> best_alignments;
  if (composition_approach == "standard") {
    StandardCompositionFst composed_fst(refFst, hypFst, symbol);
    Walker walker;
    best_alignments = walker.walkComposed(composed_fst, symbol, options, numBests);
  } else if (composition_approach == "adapted") {
    RmEpsilon(&refFst, true);
    ReverseOLabelCompare<StdArc> comparer;
    ArcSort(&refFst, comparer);
    AdaptedCompositionFst composed_fst(refFst, hypFst, symbol);
    Walker walker;
    best_alignments = walker.walkComposed(composed_fst, symbol, options, numBests);
  } else {
    throw std::runtime_error("invalid composition approach specified");
  }

  logger->info("done walking the graph");
  if (best_alignments.size() > 0) {
    sort(best_alignments.begin(), best_alignments.end(), sort_alignment);
    return best_alignments[0];
  }

  throw std::runtime_error("no alignment produced");
}

vector<shared_ptr<Stitching>> make_stitches(spWERA alignment, vector<RawCtmRecord> hyp_ctm_rows = {},
                                            vector<RawNlpRecord> hyp_nlp_rows = {}, vector<string> one_best_tokens = {}) {
  auto logger = logger::GetOrCreateLogger("fstalign");

  // Go through top alignment and create stitches
  vector<shared_ptr<Stitching>> stitches;
  AlignmentTraversor visitor(alignment);
  triple *tk_pair = new triple();

  int alignedTokensIndex = 0;
  int alignedTokensMaxRow = alignment->tokens.size();

  int hypRowIndex = 0;

  string prev_tk_classLabel = "";
  while (visitor.NextTriple(tk_pair)) {
    string tk_classLabel = tk_pair->classLabel;
    string ref_tk = tk_pair->ref;
    string hyp_tk = tk_pair->hyp;

    // next turn, we want to process the next token pair...
    shared_ptr<Stitching> part(new Stitching());
    stitches.push_back(part);
    part->classLabel = tk_classLabel;
    part->reftk = ref_tk;
    part->hyptk = hyp_tk;
    bool del = false, ins = false, sub = false;
    if (ref_tk == INS) {
      part->comment = "ins";
    } else if (hyp_tk == DEL) {
      part->comment = "del";
    } else if (hyp_tk != ref_tk) {
      part->comment = "sub(" + part->hyptk + ")";
    }

    // for classes, we will have only one token in the global vector
    // member and we don't want to increment alignedTokensIndex
    // more than once for classes so that we can verify that we're
    // always in sync between the stitches and the global
    // alignment vector
    if (tk_classLabel == TK_GLOBAL_CLASS || tk_classLabel != prev_tk_classLabel) {
      alignedTokensIndex++;
    }

    if (logger->should_log(spdlog::level::trace) && tk_classLabel != TK_GLOBAL_CLASS) {
      logger->trace("row {}, classLabel = {}", alignedTokensIndex, tk_classLabel);
    }

    prev_tk_classLabel = tk_classLabel;

    if (hyp_tk == DEL) {
      // this is a deletion, the CTM info won't be available
      part->comment = "del";
      continue;
    }

    if (ref_tk == NOOP) {
      // this is a noop, the alignment favors deletion
      continue;
    }

    if (!hyp_ctm_rows.empty()) {
      auto ctmPart = hyp_ctm_rows[hypRowIndex];
      part->start_ts = ctmPart.start_time_secs;
      part->duration = ctmPart.duration_secs;
      part->end_ts = ctmPart.start_time_secs + ctmPart.duration_secs;
      part->confidence = ctmPart.confidence;

      part->hyp_orig = ctmPart.word;
      // sanity check
      std::string ctmCopy = ctmPart.word;
      std::transform(ctmCopy.begin(), ctmCopy.end(), ctmCopy.begin(), ::tolower);
      if (hyp_tk != ctmCopy) {
        logger->warn(
            "hum, looks like the ctm and the alignment got out of sync? [{}] vs "
            "[{}]",
            hyp_tk, ctmCopy);
      }
    }

    if (!hyp_nlp_rows.empty()) {
      auto hypNlpPart = hyp_nlp_rows[hypRowIndex];
      part->hyp_orig = hypNlpPart.token;
      if (!hypNlpPart.ts.empty() && !hypNlpPart.endTs.empty()) {
        float ts = stof(hypNlpPart.ts);
        float endTs = stof(hypNlpPart.endTs);

        part->start_ts = ts;
        part->end_ts = endTs;
        part->duration = endTs - ts;
      } else if (!hypNlpPart.ts.empty()) {
        float ts = stof(hypNlpPart.ts);

        part->start_ts = ts;
        part->end_ts = ts;
        part->duration = 0.0;
      } else if (!hypNlpPart.endTs.empty()) {
        float endTs = stof(hypNlpPart.endTs);

        part->start_ts = endTs;
        part->end_ts = endTs;
        part->duration = 0.0;
      }
    }

    if (!one_best_tokens.empty()) {
      auto token = one_best_tokens[hypRowIndex];
      part->hyp_orig = token;

      // sanity check
      std::string token_copy = token;
      std::transform(token_copy.begin(), token_copy.end(), token_copy.begin(), ::tolower);
      if (hyp_tk != token_copy) {
        logger->warn(
            "hum, looks like the text and the alignment got out of sync? [{}] vs "
            "[{}]",
            hyp_tk, token_copy);
      }
    }

    hypRowIndex++;
  }

  if (alignedTokensIndex != alignedTokensMaxRow) {
    logger->warn("we didn't finished the first pass consuming all pairs or ctm rows");
    logger->warn("alignments row {}, expected {}", alignedTokensIndex, alignedTokensMaxRow);
  }

  return stitches;
}

void align_stitches_to_nlp(NlpFstLoader *refLoader, vector<shared_ptr<Stitching>> *stitches) {
  /* now, lets process NLP info */
  auto logger = logger::GetOrCreateLogger("fstalign");
  logger->set_level(spdlog::level::info);

  auto nlpRows = refLoader->mNlpRows;
  int alignedTokensIndex = 0;
  int nlpRowIndex = 0;
  int nlpMaxRow = nlpRows.size();
  int numStitches = stitches->size();
  while (alignedTokensIndex < numStitches) {
    auto stitch = (*stitches)[alignedTokensIndex];

    if (stitch->comment.find("ins") == 0) {
      // there's no nlp row info for such case, let's skip over it
      alignedTokensIndex++;
      continue;
    }

    if (nlpRowIndex >= nlpMaxRow) {
        logger->warn("Ran out of nlp rows. {} rows, {} stitches", nlpMaxRow, numStitches);
        break;
    }
    auto nlpPart = nlpRows[nlpRowIndex];
    string nlp_classLabel = GetClassLabel(nlpPart.best_label);

    // sanity check
    bool labelsMatch = true;
    bool inClassLabel = false;
    bool inSynonymPath = false;

    if (stitch->classLabel.find("_SYN_") != string::npos) {
      // logger->trace("for class label {}, the _SYN_ was found at {}", stitch->classLabel,
      // stitch->classLabel.find("_SYN_"));
      inSynonymPath = true;
    } else if (stitch->classLabel != TK_GLOBAL_CLASS) {
      inClassLabel = true;
      if (nlp_classLabel != stitch->classLabel) {
        labelsMatch = false;
      }
    } else if (nlp_classLabel != "") {
      inClassLabel = true;
      labelsMatch = false;
    }

    if (!labelsMatch) {
      logger->warn(
          "NLP stitching problem: the stitch has a class of [{}] and the nlp "
          "part "
          "has [{}]",
          stitch->classLabel, nlpPart.best_label);
    }

    // if we're not in a class, just attach the nlp row and move on
    if (inClassLabel == false && inSynonymPath == false) {
      stitch->nlpRow = nlpPart;
      alignedTokensIndex++;
      nlpRowIndex++;
      continue;
    }

    int classLabelRowsInStitches = 1;
    int classLabelRowsInNlp = 1;

    if (inSynonymPath) {
      int tmpid;
      sscanf(stitch->classLabel.c_str(), "___%d_SYN_%d-%d___", &tmpid, &classLabelRowsInNlp, &classLabelRowsInStitches);
      // We need the logic below in case there were insertions in the stitches
      while (alignedTokensIndex + classLabelRowsInStitches < numStitches) {
        int p = alignedTokensIndex + classLabelRowsInStitches;
        if (stitch->classLabel == (*stitches)[p]->classLabel) {
          classLabelRowsInStitches++;
          continue;
        }

        break;
      }
    } else {
      while (alignedTokensIndex + classLabelRowsInStitches < numStitches) {
        int p = alignedTokensIndex + classLabelRowsInStitches;
        if (stitch->classLabel == (*stitches)[p]->classLabel) {
          classLabelRowsInStitches++;
          continue;
        }

        break;
      }

      while (nlpRowIndex + classLabelRowsInNlp < nlpMaxRow) {
        int p = nlpRowIndex + classLabelRowsInNlp;
        if (nlp_classLabel == GetClassLabel(nlpRows[p].best_label)) {
          classLabelRowsInNlp++;
          continue;
        }

        break;
      }
    }

    // handling various alignment cases:

    // one row for both, easy peasy...
    if (classLabelRowsInNlp == classLabelRowsInStitches && classLabelRowsInStitches == 1) {
      stitch->nlpRow = nlpPart;
      alignedTokensIndex++;
      nlpRowIndex++;
      continue;
    }

    // same number of rows.  We'll make the /assumption/ that a
    // direct alignment make sense
    if (classLabelRowsInNlp == classLabelRowsInStitches) {
      stitch->nlpRow = nlpPart;
      stitch->comment += ",direct";
      alignedTokensIndex++;
      nlpRowIndex++;

      for (int i = 1; i < classLabelRowsInNlp; i++) {
        (*stitches)[alignedTokensIndex]->nlpRow = nlpRows[nlpRowIndex];
        (*stitches)[alignedTokensIndex]->comment += ",direct";
        alignedTokensIndex++;
        nlpRowIndex++;
      }
      continue;
    }

    // only one nlp row for many words :
    // Miguel's suggestion: put punct/case info on the last word
    // Revision: put case info on the 1st word and punct info on the last word
    if (classLabelRowsInNlp == 1) {
      RawNlpRecord *newRecord = new RawNlpRecord();
      newRecord->best_label = nlpPart.best_label;
      newRecord->best_label_id = nlpPart.best_label_id;
      newRecord->labels = nlpPart.labels;
      newRecord->wer_tags = nlpPart.wer_tags;
      newRecord->speakerId = nlpPart.speakerId;
      newRecord->token = nlpPart.token;
      newRecord->ts = nlpPart.ts;
      newRecord->endTs = nlpPart.endTs;
      newRecord->punctuation = "";
      newRecord->casing = "LC";

      stitch->nlpRow = *newRecord;
      // if we have a UC, we want that info to be transferred to the 1st word
      stitch->nlpRow.casing = nlpPart.casing;
      stitch->comment += ",push_last";
      alignedTokensIndex++;

      if (classLabelRowsInStitches > 2) {
        for (int i = 1; i < classLabelRowsInStitches - 1; i++) {
          (*stitches)[alignedTokensIndex]->nlpRow = *newRecord;
          (*stitches)[alignedTokensIndex]->comment += ",push_last";
          alignedTokensIndex++;
        }
      }

      (*stitches)[alignedTokensIndex]->nlpRow = nlpPart;
      // setting last word casing to LC
      (*stitches)[alignedTokensIndex]->nlpRow.casing = "LC";
      (*stitches)[alignedTokensIndex]->comment += ",push_last";
      alignedTokensIndex++;
      nlpRowIndex++;
      continue;
    }

    // many nlp rows and many stitches rows...
    // Miguel's suggestion: first nlp to the first word
    //                      last nlp to the last word...
    // I guess we could try to be smart and align tokens with reco/ref words...
    stitch->nlpRow = nlpPart;
    stitch->comment += ",split_worst";

    auto lastNlpPartInClass = nlpRows[nlpRowIndex + classLabelRowsInNlp - 1];
    // setting the index properly for the next turn...
    nlpRowIndex += classLabelRowsInNlp;

    // special case: multiple nlp rows mapped to one row in the CTM
    if (classLabelRowsInStitches == 1) {
      // so we have > 1 nlp row but one single row in the ctm...
      // let's use the punctuation info of the last nlp row
      // and call it good
      stitch->nlpRow.punctuation = lastNlpPartInClass.punctuation;
      alignedTokensIndex++;
      continue;
    }

    // TODO: check if there isn't any punct/case info lost and log it

    alignedTokensIndex++;
    for (int i = 1; i < classLabelRowsInStitches - 1; i++) {
      (*stitches)[alignedTokensIndex]->nlpRow = RawNlpRecord();
      (*stitches)[alignedTokensIndex]->nlpRow.speakerId = nlpPart.speakerId;
      (*stitches)[alignedTokensIndex]->nlpRow.punctuation = "";
      (*stitches)[alignedTokensIndex]->nlpRow.casing = "LC";
      (*stitches)[alignedTokensIndex]->nlpRow.labels = nlpPart.labels;
      (*stitches)[alignedTokensIndex]->nlpRow.wer_tags = nlpPart.wer_tags;
      (*stitches)[alignedTokensIndex]->comment += ",split_worst";
      alignedTokensIndex++;
    }

    (*stitches)[alignedTokensIndex]->nlpRow = lastNlpPartInClass;
    (*stitches)[alignedTokensIndex]->comment += ",split_worst";
    alignedTokensIndex++;

    /* end of special handling of nlp rows vs stitches  */
  }
}

void write_stitches_to_nlp(vector<shared_ptr<Stitching>> stitches, ofstream &output_nlp_file, Json::Value norm_json) {
  auto logger = logger::GetOrCreateLogger("fstalign");
  logger->info("Writing nlp output");
  // write header; 'comment' is there to store information about how well the alignment went
  // for this word
  output_nlp_file << "token|speaker|ts|endTs|punctuation|case|tags|wer_tags|oldTs|"
                     "oldEndTs|ali_comment"
                  << endl;
  for (auto &stitch : stitches) {
    // if the comment starts with 'ins'
    if (stitch->comment.find("ins") == 0) {
      // there's no nlp row info for such case, let's skip over it
      if (stitch->confidence >= 1) {
        logger->warn("an insertion with high confidence was found for {}@{}", stitch->hyptk, stitch->start_ts);
      }

      continue;
    }

    string original_nlp_token = stitch->nlpRow.token;
    string ref_tk = stitch->reftk;

    // trying to salvage some of the original punctuation in a relatively safe manner
    if (iequals(ref_tk, original_nlp_token)) {
      // Get the original casing
      ref_tk = original_nlp_token;
      auto curr_label_id = stitch->nlpRow.best_label_id;
      if (norm_json[curr_label_id]["candidates"].size() > 0) {
        logger->warn("an unnormalized token was found: {}", ref_tk);
      }
    } else if (IsNoisecodeToken(original_nlp_token)) {
      // if we have a noisecode  <.*> in the nlp token, we inject it here
      if (stitch->comment.length() == 0) {
        if (ref_tk == DEL || ref_tk == "") {
          stitch->comment = "sub(<eps>)";
        } else {
          stitch->comment = "sub(" + ref_tk + ")";
        }
      }

      ref_tk = original_nlp_token;
    }

    if (ref_tk == NOOP) {
      continue;
    }

    output_nlp_file << ref_tk << "|" << stitch->nlpRow.speakerId << "|";
    if (stitch->hyptk == DEL) {
      // we have no ts/endTs data to put...
      output_nlp_file << "||";
    } else {
      output_nlp_file << fmt::format("{0:.4f}", stitch->start_ts) << "|" << fmt::format("{0:.4f}", stitch->end_ts)
                      << "|";
    }

    output_nlp_file << stitch->nlpRow.punctuation << "|" << stitch->nlpRow.casing << "|" << stitch->nlpRow.labels << "|"
                    << "[";
    /* for (auto wer_tag : nlpRow.wer_tags) { */
    for (auto it = stitch->nlpRow.wer_tags.begin(); it != stitch->nlpRow.wer_tags.end(); ++it) {
      output_nlp_file << "'" << *it << "'";
      if (std::next(it) != stitch->nlpRow.wer_tags.end()) {
        output_nlp_file << ", ";
      }
    }
    output_nlp_file << "]|";
    // leaving old timings intact
    output_nlp_file << stitch->nlpRow.ts << "|" << stitch->nlpRow.endTs << "|";
    output_nlp_file << stitch->comment << endl;
  }
}

void HandleWer(FstLoader *refLoader, FstLoader *hypLoader, SynonymEngine *engine, string output_sbs, string output_nlp,
               int speaker_switch_context_size, int numBests, int pr_threshold, string symbols_filename,
               string composition_approach, bool record_case_stats) {
  auto logger = logger::GetOrCreateLogger("fstalign");
  logger->set_level(spdlog::level::info);

  spWERA topAlignment = Fstalign(refLoader, hypLoader, engine, numBests, symbols_filename, composition_approach);
  CalculatePrecisionRecall(topAlignment, pr_threshold);

  RecordWer(topAlignment);
  if (!output_sbs.empty()) {
    logger->info("output_sbs = {}", output_sbs);
    WriteSbs(topAlignment, output_sbs);
  }

  NlpFstLoader *nlp_ref_loader = dynamic_cast<NlpFstLoader *>(refLoader);
  if (nlp_ref_loader) {
    // We have an NLP reference, more metadata (e.g. speaker info) is available
    vector<shared_ptr<Stitching>> stitches;
    CtmFstLoader *ctm_hyp_loader = dynamic_cast<CtmFstLoader *>(hypLoader);
    NlpFstLoader *nlp_hyp_loader = dynamic_cast<NlpFstLoader *>(hypLoader);
    OneBestFstLoader *best_loader = dynamic_cast<OneBestFstLoader *>(hypLoader);
    if (ctm_hyp_loader) {
      stitches = make_stitches(topAlignment, ctm_hyp_loader->mCtmRows, {});
    } else if (nlp_hyp_loader) {
      stitches = make_stitches(topAlignment, {}, nlp_hyp_loader->mNlpRows);
    } else if (best_loader) {
      vector<string> tokens;
      tokens.reserve(best_loader->TokensSize());
      for (int i = 0; i < best_loader->TokensSize(); i++ ) {
        string token = best_loader->getToken(i);
        tokens.push_back(token);
      }
      stitches = make_stitches(topAlignment, {}, {}, tokens);
    } else {
      stitches = make_stitches(topAlignment);
    }

    // Align stitches to the NLP, so stitches can access metadata
    try {
      align_stitches_to_nlp(nlp_ref_loader, &stitches);
    } catch (const std::bad_alloc &) {
      logger->error("Speaker switch diagnostics failed from memory error, likely due to overlapping class labels.");
    }

    if (record_case_stats) {
      RecordCaseWer(stitches);
    }

    // Calculate and record speaker switch WER if context size is provided
    if (speaker_switch_context_size > 0) {
      logger->info("Calculating WER around speaker switches, using a window size of {0}", speaker_switch_context_size);
      // Count and record errors around speaker switches
      RecordSpeakerSwitchWer(stitches, speaker_switch_context_size);
    }

    // Calculate and record per-speaker WER
    RecordSpeakerWer(stitches);
    RecordTagWer(stitches);

    if (!output_nlp.empty()) {
      ofstream nlp_ostream(output_nlp);
      write_stitches_to_nlp(stitches, nlp_ostream, nlp_ref_loader->mJsonNorm);
    }
  }

  if (!output_nlp.empty() && !nlp_ref_loader) {
    logger->warn("Attempted to output an Aligned NLP file without NLP reference, skipping output.");
  }
}

void HandleAlign(NlpFstLoader *refLoader, CtmFstLoader *hypLoader, SynonymEngine *engine, ofstream &output_nlp_file,
                 int numBests, string symbols_filename, string composition_approach) {
  auto topAlignment = Fstalign(refLoader, hypLoader, engine, numBests, symbols_filename, composition_approach);
  // dump the WER details even when we're just considering alignment
  RecordWer(topAlignment);

  auto logger = logger::GetOrCreateLogger("fstalign");
  logger->set_level(spdlog::level::info);

  if (topAlignment == nullptr) {
    logger->warn("No alignment produced, can't compute WER statistics or alignments");
    return;
  }
  auto stitches = make_stitches(topAlignment, hypLoader->mCtmRows);
  align_stitches_to_nlp(refLoader, &stitches);
  write_stitches_to_nlp(stitches, output_nlp_file, refLoader->mJsonNorm);
}
