/*
 * wer.h
 *
 * Collection of functions specific to the WER subcommand.
 *
 * Quinn McNamara (quinn@rev.com)
 * 2021
 */
#include "fstalign.h"
#include "json_logging.h"

using namespace std;

struct WerResult {
  int insertions;
  int deletions;
  int substitutions;
  int numWordsInReference;
  int numWordsInHypothesis;
  int NumErrors() { return insertions + substitutions + deletions; }
  /* can return infinity if numWordsInReference == 0 and numWordsInHypothesis > 0 */
  float WER() {
    if (numWordsInReference > 0) {
      return (float)(insertions + deletions + substitutions) / (float)numWordsInReference;
    }

    if (numWordsInHypothesis > 0) {
      return numeric_limits<float>::infinity();
    }

    return -nanf("");
  }
};

vector<int> GetSpeakerSwitchIndices(const vector<Stitching>& stitches);

// These methods record different WER analyses to JSON
void RecordWerResult(Json::Value &json, WerResult wr);
void RecordWer(wer_alignment& topAlignment);
void RecordSpeakerWer(const vector<Stitching>& stitches);
void RecordSpeakerSwitchWer(const vector<Stitching>& stitches, int speaker_switch_context_size);
void RecordSentenceWer(const vector<Stitching>& stitches);
void RecordTagWer(const vector<Stitching>& stitches);
void RecordCaseWer(const vector<Stitching>& aligned_stitches);

// Adds PR metrics to topAlignment
void CalculatePrecisionRecall(wer_alignment &topAlignment, int threshold);

typedef vector<pair<size_t, string>> ErrorGroups;

void AddErrorGroup(ErrorGroups &groups, size_t &line, string &ref, string &hyp);
void WriteSbs(wer_alignment &topAlignment, const vector<Stitching>& stitches, string sbs_filename, const vector<string> extra_nlp_columns);
void JsonLogUnigramBigramStats(wer_alignment &topAlignment);
