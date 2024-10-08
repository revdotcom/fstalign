/*
fstalign.h
 JP Robichaud (jp@rev.com)
 2018

*/

#ifndef __FSTALIGN_H__
#define __FSTALIGN_H__

#include "Ctm.h"
#include "Nlp.h"
#include "SynonymEngine.h"

using namespace std;
using namespace fst;

// Represent information associated with a reference or hypothesis token
struct Token {
  string token;
  float start_ts=-1.0;
  float end_ts=-1.0;
  float duration=0.0;
  float confidence=-1.0;
  string speaker;
};

// Stitchings will be used to represent fstalign output, combining reference,
// hypothesis, and error information into a record-like data structure.
struct Stitching {
  Token reftk;
  Token hyptk;
  string classLabel;
  RawNlpRecord nlpRow;
  string hyp_orig;
  string comment;
};

struct AlignerOptions {
  int speaker_switch_context_size;
  int numBests = 20;
  int heapPruningTarget = 20;
  int pr_threshold = 0;
  string symbols_filename = "";
  string composition_approach = "adapted";
  bool record_case_stats;
  bool levenstein_first_pass = false;
  int levenstein_maximum_error_streak = 100;
};

// original
// void HandleWer(FstLoader *refLoader, FstLoader *hypLoader, SynonymEngine *engine, string output_sbs, string
// output_nlp,
//                int speaker_switch_context_size, int numBests, int pr_threshold, string symbols_filename,
//                string composition_approach, bool record_case_stats);
// void HandleAlign(NlpFstLoader *refLoader, CtmFstLoader *hypLoader, SynonymEngine *engine, ofstream &output_nlp_file,
//                  int numBests, string symbols_filename, string composition_approach);

void HandleWer(FstLoader& refLoader, FstLoader& hypLoader, SynonymEngine &engine, const string& output_sbs, const string& output_nlp,
               AlignerOptions alignerOptions, bool add_inserts_nlp = false, bool use_case = false);
void HandleAlign(NlpFstLoader &refLoader, CtmFstLoader &hypLoader, SynonymEngine &engine, ofstream &output_nlp_file,
                 AlignerOptions alignerOptions);

string GetTokenPropertyAsString(Stitching stitch, bool refToken, string property);

#endif  // __FSTALIGN_H__
