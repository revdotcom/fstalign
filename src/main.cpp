#include <CLI/CLI.hpp>
#include <fstream>

#include "FstFileLoader.h"
#include "OneBestFstLoader.h"
#include "fstalign.h"
#include "json_logging.h"
#include "version.h"

using namespace std;
using namespace fst;

int main(int argc, char **argv) {
  setlocale(LC_ALL, "en_US.UTF-8");
  string ref_filename;
  string json_norm_filename;
  string hyp_filename;
  string log_filename = "";
  string output_nlp = "";
  string output_sbs = "";
  string synonyms_filename;
  string hyp_json_norm_filename = "";
  string output_json_log;
  string symbols_filename = "";
  int pr_threshold = 0;
  bool disable_cutoffs;
  bool version;
  bool do_adapted_composition = false;
  int speaker_switch_context_size = 5;
  int numBests = 100;
  bool keep_case = false;

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

    c->add_option("--pr_threshold", pr_threshold,
                  "Threshold of occurrences that will be output in"
                  "Precision and Recall listings");

    c->add_option("--symbols", symbols_filename,
                  "Symbols table to use as a common starting point. Required for FST inputs.");

    c->add_flag("--do-adapted-composition", do_adapted_composition, "Use new alternative composition logic");
  }

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

  get_wer->add_flag("--keep-case", keep_case, "Keep casing information, default is to only compare lowercase tokens");

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

  Json::Value hyp_json_obj;
  if (!hyp_json_norm_filename.empty()) {
    console->info("reading hypothesis json norm info from {}", hyp_json_norm_filename);
    ifstream ifs(hyp_json_norm_filename);

    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;

    JSONCPP_STRING errs;
    Json::parseFromStream(builder, ifs, &hyp_json_obj, &errs);

    console->info("The json we just read [{}] has {} elements from its root", json_norm_filename, hyp_json_obj.size());
  } else {
    stringstream ss;
    ss << "{}";

    Json::CharReaderBuilder builder;
    JSONCPP_STRING errs;
    Json::parseFromStream(builder, ss, &hyp_json_obj, &errs);
  }

  // loading "reference" inputs
  FstLoader *hyp;
  FstLoader *ref;
  if (EndsWithCaseInsensitive(ref_filename, string(".nlp"))) {
    NlpReader nlpReader = NlpReader();
    console->info("reading reference nlp from {}", ref_filename);
    auto vec = nlpReader.read_from_disk(ref_filename);
    NlpFstLoader *nlpFst = new NlpFstLoader(vec, obj, true, keep_case);
    ref = nlpFst;
  } else if (EndsWithCaseInsensitive(ref_filename, string(".ctm"))) {
    console->info("reading reference ctm from {}", ref_filename);
    CtmReader ctmReader = CtmReader();
    auto vect = ctmReader.read_from_disk(ref_filename);
    CtmFstLoader *ctmFst = new CtmFstLoader(vect, keep_case);
    ref = ctmFst;
  } else {
    console->info("reading reference plain text from {}", ref_filename);
    auto *oneBestFst = new OneBestFstLoader();
    oneBestFst->LoadTextFile(ref_filename, keep_case);
    ref = oneBestFst;
  }

  // loading "hypothesis" inputs
  if (EndsWithCaseInsensitive(hyp_filename, string(".nlp"))) {
    console->info("reading hypothesis nlp from {}", hyp_filename);
    NlpReader nlpReader = NlpReader();
    auto vec = nlpReader.read_from_disk(hyp_filename);
    // for now, nlp files passed as hypothesis won't have their labels handled as such
    // this also mean that json normalization will be ignored
    NlpFstLoader *nlpFst = new NlpFstLoader(vec, hyp_json_obj, false, keep_case);
    hyp = nlpFst;
  } else if (EndsWithCaseInsensitive(hyp_filename, string(".ctm"))) {
    console->info("reading hypothesis ctm from {}", hyp_filename);
    CtmReader ctmReader = CtmReader();
    auto vect = ctmReader.read_from_disk(hyp_filename);
    CtmFstLoader *ctmFst = new CtmFstLoader(vect, keep_case);
    hyp = ctmFst;
  } else if (EndsWithCaseInsensitive(hyp_filename, string(".fst"))) {
    if (symbols_filename.empty()) {
      console->error("a symbols file must be specified if reading an FST.");
    }
    console->info("reading hypothesis fst from {}", hyp_filename);
    FstFileLoader *archive_fst = new FstFileLoader(hyp_filename);
    hyp = archive_fst;
  } else {
    console->info("reading hypothesis plain text from {}", hyp_filename);
    auto *hypOneBest = new OneBestFstLoader();
    hypOneBest->LoadTextFile(hyp_filename, keep_case);
    hyp = hypOneBest;
  }

  SynonymEngine *engine = nullptr;
  if (synonyms_filename.size() > 0) {
    engine = new SynonymEngine(disable_cutoffs);
    engine->load_file(synonyms_filename);
  }

  if (command == "wer") {
    if (do_adapted_composition) {
      HandleWer(ref, hyp, engine, output_sbs, output_nlp, speaker_switch_context_size, numBests, pr_threshold,
                symbols_filename, "adapted");
    } else {
      HandleWer(ref, hyp, engine, output_sbs, output_nlp, speaker_switch_context_size, numBests, pr_threshold,
                symbols_filename, "standard");
    }
  } else if (command == "align") {
    if (output_nlp.empty()) {
      console->error("the output nlp file must be specified");
    }

    console->info("we'll be writing to {}", output_nlp);
    ofstream output_nlp_file(output_nlp);

    // TODO: We should instrument FstLoader base class
    // to have nlp rows and ctm rows and have a getCtmRows/getNlpRows
    // empty methods that throw exceptions if not implemented.
    NlpFstLoader *nlpRef = dynamic_cast<NlpFstLoader *>(ref);
    CtmFstLoader *ctmHyp = dynamic_cast<CtmFstLoader *>(hyp);

    if (do_adapted_composition) {
      HandleAlign(nlpRef, ctmHyp, engine, output_nlp_file, numBests, symbols_filename, "adapted");
    } else {
      HandleAlign(nlpRef, ctmHyp, engine, output_nlp_file, numBests, symbols_filename, "standard");
    }

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
