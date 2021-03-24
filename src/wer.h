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

vector<int> GetSpeakerSwitchIndices(vector<shared_ptr<Stitching>> stitches);

// These methods record different WER analyses to JSON
void RecordWerResult(Json::Value &json, WerResult wr);
void RecordWer(spWERA topAlignment);
void RecordSpeakerWer(vector<shared_ptr<Stitching>> stitches);
void RecordSpeakerSwitchWer(vector<shared_ptr<Stitching>> stitches, int speaker_switch_context_size);
void RecordTagWer(vector<shared_ptr<Stitching>> stitches);

// Adds PR metrics to topAlignment
void CalculatePrecisionRecall(spWERA &topAlignment, int threshold);

typedef vector<pair<size_t, string>> ErrorGroups;

void AddErrorGroup(ErrorGroups &groups, size_t &line, string &ref, string &hyp);
void WriteSbs(spWERA topAlignment, string sbs_filename);
