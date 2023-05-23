#include <CLI/CLI.hpp>
#include <fstream>

#include "FstFileLoader.h"
#include "OneBestFstLoader.h"
#include "fast-d.h"
#include "fstalign.h"
#include "json_logging.h"
#include "utilities.h"
#include "version.h"

using namespace std;
using namespace fst;

int main(int argc, char **argv) {
  setlocale(LC_ALL, "en_US.UTF-8");
  string ref_filename;
  string json_norm_filename;
  string wer_sidecar_filename;
  string hyp_filename;
  string log_filename = "";
  string output_nlp = "";
  string output_sbs = "";
  string synonyms_filename;
  string hyp_json_norm_filename = "";
  string output_json_log;
  string symbols_filename = "";
  int pr_threshold = 0;
  bool version;
  string composition_approach = "adapted";
  int speaker_switch_context_size = 5;
  int numBests = 100;
  int levenstein_maximum_error_streak = 100;
  bool record_case_stats = false;
  bool use_punctuation = false;
  bool disable_approximate_alignment = false;

  bool disable_cutoffs = false;
  bool disable_hyphen_ignore = false;

  CLI::App app("Rev FST Align");
  app.set_help_all_flag("--help-all", "Expand all help");
  app.add_flag("--version", version, "Show fstalign version.");

  CLI::App *get_wer = app.add_subcommand("wer", "Get the WER between a reference and an hypothesis.");
  CLI::App *get_alignment =
      app.add_subcommand("align", "Produce an alignment between an NLP file and a CTM-like input.");

  // adding common options.  It's fine to reuse the ref_filename since we
  // require exactly one subcommand to be defined
  for (auto &c : {get_wer, get_alignment}) {
    c->add_option("-r,--ref", ref_filename,
                  "Reference filename (.nlp & .ctm have special handling, "
                  "everything else is handled as a plain text)");
    c->add_option("--ref-json", json_norm_filename,
                  "JSon normalization sidecar file, used in conjunction with "
                  ".nlp input.)");
    c->add_option("-s,--syn", synonyms_filename,
                  "Synonyms definition filename.  |-delimited list of "
                  "expressions to allow alternatives to one or many words");
    c->add_flag("--disable-cutoffs", disable_cutoffs,
                "Prevents the synonym engine from adding synonyms of cutoff "
                "words (e.g. the-)");
    c->add_flag("--disable-hyphen-ignore", disable_hyphen_ignore,
                "Prevents the synonym engine from adding synonyms of hyphenated "
                "compound words (e.g. best-ever <-> best ever)");
    c->add_flag("--disable-approx-alignment", disable_approximate_alignment,
                "Disable getting a first approximate alignment/WER before the more exhaustive search happens");

    // NOTE: we can't have -h as a synonym for --hyp as it collides with --help
    c->add_option("--hyp", hyp_filename, "Hypothesis filename (same rules as for --ref handling.)");

    // TODO: add support for hypothesis-side normalization
    // c->add_option("--hyp-json", hyp_json_norm_filename,
    //               "JSon normalization sidecar file, used in conjunction with "
    //               ".nlp input.)");

    c->add_option("--output-nlp", output_nlp, "The output path to store the aligned nlp file");
    c->add_option("--output-sbs", output_sbs, "The output path to store the side-by-side alignment");

    c->add_option("--log", log_filename, "Save logging output to this file as well as to the console.)");

    c->add_option("--numbests", numBests,
                  "The maximum number of minimum error paths through the alignment graph. Defaults to 100.");

    c->add_option("--levenstein-max-error-streak", levenstein_maximum_error_streak,
                  "The maximum number of consecutive errors supported by levenstein approximation. Defaults to 100.");

    c->add_option("--pr_threshold", pr_threshold,
                  "Threshold of occurrences that will be output in"
                  "Precision and Recall listings");

    c->add_option("--symbols", symbols_filename,
                  "Symbols table to use as a common starting point. Required for FST inputs.");

    c->add_option("--composition-approach", composition_approach,
                  "Desired composition logic. Choices are 'standard' or 'adapted'");
  }
  get_wer->add_option("--wer-sidecar", wer_sidecar_filename,
                "WER sidecar json file.");

  get_wer->add_option("--speaker-switch-context", speaker_switch_context_size,
                      "Amount of context (in each direction) around "
                      "a speaker switch to investigate for WER. This "
                      "feature can be disabled by setting a value <= 0.");

  // Add JSON log output to all sub commands
  get_wer->add_option(
      "--json-log", output_json_log,
      "Filename for JSON log output, containing structured output from the subcommand you run. Current schema looks like\n\
                        {\n\
                            wer:\n\
                            {\n\
                                bestWER:{deletions:INT, insertions:INT, substitutions:INT, numErrors:INT, numWordsInReference:INT, wer:FLOAT, meta:{}},\n\
                                classWER:{CARDINAL:{deletions:INT, insertions:INT, substitutions:INT, numErrors:INT, numWordsInReference:INT, wer:FLOAT, meta:{}}},\n\
                                speakerSwitchWER:{deletions:INT, insertions:INT, substitutions:INT, numErrors:INT, numWordsInReference:INT, wer:FLOAT, meta:{windowSize:INT}}\n\
                                speakerWER:{1:{deletions:INT, insertions:INT, substitutions:INT, numErrors:INT, numWordsInReference:INT, wer:FLOAT, meta:{}}},\n\
                            }\n\
                        }");

  get_wer->add_flag("--record-case-stats", record_case_stats,
                    "Record precision/recall for how well the hypothesis"
                    "casing matches the reference.");
  get_wer->add_flag("--use-punctuation", use_punctuation, "Treat punctuation from nlp rows as separate tokens");

  // CLI11_PARSE(app, argc, argv);
  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
    return app.exit(e);
  }

  if (version) {
    std::cout << FSTALIGNER_VERSION_MAJOR << "." << FSTALIGNER_VERSION_MINOR << "." << FSTALIGNER_VERSION_PATCH
              << std::endl;
    return 0;
  }

  if (app.get_subcommands().size() == 0) {
    // If no subcommands were provided, show help text
    std::cout << app.help();
    return 1;
  }

  logger::InitLoggers(log_filename);
  auto console = logger::GetLogger("console");
  console->info("fstalign version is {}.{}.{}", FSTALIGNER_VERSION_MAJOR, FSTALIGNER_VERSION_MINOR,
                FSTALIGNER_VERSION_PATCH);

  auto subcommand = app.get_subcommands()[0];
  auto command = subcommand->get_name(); 
 

  // loading "reference" inputs
  std::unique_ptr<FstLoader> hyp = FstLoader::MakeHypothesisLoader(hyp_filename, hyp_json_norm_filename, use_punctuation, !symbols_filename.empty());
  std::unique_ptr<FstLoader> ref = FstLoader::MakeReferenceLoader(ref_filename, wer_sidecar_filename, json_norm_filename, use_punctuation, !symbols_filename.empty()); 

  AlignerOptions alignerOptions;
  alignerOptions.speaker_switch_context_size = speaker_switch_context_size;
  alignerOptions.levenstein_first_pass = !disable_approximate_alignment;
  alignerOptions.numBests = numBests;
  alignerOptions.levenstein_maximum_error_streak = levenstein_maximum_error_streak;
  alignerOptions.pr_threshold = pr_threshold;
  alignerOptions.record_case_stats = record_case_stats;
  alignerOptions.symbols_filename = symbols_filename;
  alignerOptions.composition_approach = composition_approach;


  SynonymOptions syn_opts;
  syn_opts.disable_cutoffs = disable_cutoffs;
  syn_opts.disable_hyphen_ignore = disable_hyphen_ignore;

  SynonymEngine engine(syn_opts);
  if (synonyms_filename.size() > 0) {
    engine.LoadFile(synonyms_filename);
  }

  if (command == "wer") {
    HandleWer(*ref, *hyp, engine, output_sbs, output_nlp, alignerOptions);
  } else if (command == "align") {
    if (output_nlp.empty()) {
      console->error("the output nlp file must be specified");
    }

    console->info("we'll be writing to {}", output_nlp);
    ofstream output_nlp_file(output_nlp);

    // TODO: We should instrument FstLoader base class
    // to have nlp rows and ctm rows and have a getCtmRows/getNlpRows
    // empty methods that throw exceptions if not implemented.
    NlpFstLoader *nlpRef = dynamic_cast<NlpFstLoader *>(ref.get());
    CtmFstLoader *ctmHyp = dynamic_cast<CtmFstLoader *>(hyp.get());

    HandleAlign(*nlpRef, *ctmHyp, engine, output_nlp_file, alignerOptions);

    output_nlp_file.flush();
    output_nlp_file.close();

  } else {
    console->error("The command {} isn't implemented yet", command);
  }

  logger::CloseLoggers();

  if (!output_json_log.empty()) {
    console->info("Writing JSON log output to {}", output_json_log);
    ofstream jsonFile;
    jsonFile.open(output_json_log);
    jsonFile << jsonLogger::JsonLogger::getLogger().root << std::endl;
    jsonFile.close();
  }

  console->info("done");
}

std::unique_ptr<FstLoader> FstLoader::MakeReferenceLoader(const std::string& ref_filename,
                                                          const std::string& wer_sidecar_filename,
                                                          const std::string& json_norm_filename,
                                                          bool use_punctuation,
                                                          bool symbols_file_included) {
  auto console = logger::GetLogger("console");
  Json::Value obj;
  if (!json_norm_filename.empty()) {
    console->info("reading json norm info from {}", json_norm_filename);
    ifstream ifs(json_norm_filename);

    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;

    JSONCPP_STRING errs;
    Json::parseFromStream(builder, ifs, &obj, &errs);

    console->info("The json we just read [{}] has {} elements from its root", json_norm_filename, obj.size());
  } else {
    stringstream ss;
    ss << "{}";

    Json::CharReaderBuilder builder;
    JSONCPP_STRING errs;
    Json::parseFromStream(builder, ss, &obj, &errs);
  }
  Json::Value wer_sidecar_obj;
  if (!wer_sidecar_filename.empty()) {
    console->info("reading wer sidecar info from {}", wer_sidecar_filename);
    ifstream ifs(wer_sidecar_filename);

    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;

    JSONCPP_STRING errs;
    Json::parseFromStream(builder, ifs, &wer_sidecar_obj, &errs);

    console->info("The json we just read [{}] has {} elements from its root", wer_sidecar_filename, wer_sidecar_obj.size());
  } else {
    stringstream ss;
    ss << "{}";

    Json::CharReaderBuilder builder;
    JSONCPP_STRING errs;
    Json::parseFromStream(builder, ss, &wer_sidecar_obj, &errs);
  }
  if (EndsWithCaseInsensitive(ref_filename, string(".nlp"))) {
    NlpReader nlpReader = NlpReader();
    console->info("reading reference nlp from {}", ref_filename);
    auto vec = nlpReader.read_from_disk(ref_filename);
    return std::make_unique<NlpFstLoader>(vec, obj, wer_sidecar_obj, true, use_punctuation);
  } else if (EndsWithCaseInsensitive(ref_filename, string(".ctm"))) {
    console->info("reading reference ctm from {}", ref_filename);
    CtmReader ctmReader = CtmReader();
    auto vect = ctmReader.read_from_disk(ref_filename);
    return std::make_unique<CtmFstLoader>(vect);
  } else if (EndsWithCaseInsensitive(ref_filename, string(".fst"))) {
    if (!symbols_file_included) {
      console->error("a symbols file must be specified if reading an FST.");
    }
    console->info("reading reference fst from {}", ref_filename);
    return std::make_unique<FstFileLoader>(ref_filename);
  } else {
    console->info("reading reference plain text from {}", ref_filename);
    auto oneBestFst = std::make_unique<OneBestFstLoader>();
    oneBestFst->LoadTextFile(ref_filename);
    return oneBestFst;
  }
}

std::unique_ptr<FstLoader> FstLoader::MakeHypothesisLoader(const std::string& hyp_filename,
                                                           const std::string& hyp_json_norm_filename,
                                                           bool use_punctuation,
                                                           bool symbols_file_included) {
  auto console = logger::GetLogger("console");



  Json::Value hyp_json_obj;
  if (!hyp_json_norm_filename.empty()) {
    console->info("reading hypothesis json norm info from {}", hyp_json_norm_filename);
    ifstream ifs(hyp_json_norm_filename);

    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;

    JSONCPP_STRING errs;
    Json::parseFromStream(builder, ifs, &hyp_json_obj, &errs);

    console->info("The json we just read [{}] has {} elements from its root", hyp_json_norm_filename, hyp_json_obj.size());
  } else {
    stringstream ss;
    ss << "{}";

    Json::CharReaderBuilder builder;
    JSONCPP_STRING errs;
    Json::parseFromStream(builder, ss, &hyp_json_obj, &errs);
  }

  // loading "hypothesis" inputymb
  if (EndsWithCaseInsensitive(hyp_filename, string(".nlp"))) {
    console->info("reading hypothesis nlp from {}", hyp_filename);
    // Make empty json for wer sidecar
    Json::Value hyp_empty_json;
    stringstream ss;
    ss << "{}";

    Json::CharReaderBuilder builder;
    JSONCPP_STRING errs;
    Json::parseFromStream(builder, ss, &hyp_empty_json, &errs);
    NlpReader nlpReader = NlpReader();
    auto vec = nlpReader.read_from_disk(hyp_filename);
    // for now, nlp files passed as hypothesis won't have their labels handled as such
    // this also mean that json normalization will be ignored
    return std::make_unique<NlpFstLoader>(vec, hyp_json_obj, hyp_empty_json, false, use_punctuation);
  } else if (EndsWithCaseInsensitive(hyp_filename, string(".ctm"))) {
    console->info("reading hypothesis ctm from {}", hyp_filename);
    CtmReader ctmReader = CtmReader();
    auto vect = ctmReader.read_from_disk(hyp_filename);
    return std::make_unique<CtmFstLoader>(vect);
  } else if (EndsWithCaseInsensitive(hyp_filename, string(".fst"))) {
    if (!symbols_file_included) {
      console->error("a symbols file must be specified if reading an FST.");
    }
    console->info("reading hypothesis fst from {}", hyp_filename);
    return std::make_unique<FstFileLoader>(hyp_filename);
  } else {
    console->info("reading hypothesis plain text from {}", hyp_filename);
    auto hypOneBest = std::make_unique<OneBestFstLoader>();
    hypOneBest->LoadTextFile(hyp_filename);
    return hypOneBest;
  }
} 
