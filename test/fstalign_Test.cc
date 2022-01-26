#define CATCH_CONFIG_MAIN
#include "../third-party/catch2/single_include/catch2/catch.hpp"

#include "test-utilties.h"

using Catch::Matchers::Contains;

class UniqueTestsFixture {
 protected:
  std::string getOutputSbsPath() { return std::to_string(++uniqueId) + "_output.sbs"; }

  std::string getOutputNlpPath() { return std::to_string(++uniqueId) + "_output.nlp"; }

 private:
  static size_t uniqueId;
};

size_t UniqueTestsFixture::uniqueId = 0;

// there just to setup the loggers
TEST_CASE("STATIC_REQUIRE showcase", "[traits]") {
  logger::InitLoggers("");
  STATIC_REQUIRE(std::is_void<void>::value);
  STATIC_REQUIRE_FALSE(std::is_void<int>::value);
}

TEST_CASE_METHOD(UniqueTestsFixture, "main-standard-composition()") {
  // setup (before each test) -- done sequentially before each test gets spawned off in parallel
  const auto sbs_output = getOutputSbsPath();
  const auto nlp_output = getOutputNlpPath();

  auto logger = logger::GetOrCreateLogger("main()");
  const char *approach = "--composition-approach standard";

  SECTION("empty_hyp_ctm") {
    const auto result = exec(command("wer", approach, "empty.ref.txt", "empty.hyp.ctm", sbs_output));

    REQUIRE_THAT(result, Contains("WER: 3/3 = 1.0000"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:3 SUB:0"));
  }

  SECTION("empty_hyp_nlp") {
    const auto result = exec(command("wer", approach, "empty.ref.txt", "empty.hyp.nlp", sbs_output));

    REQUIRE_THAT(result, Contains("WER: 3/3 = 1.0000"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:3 SUB:0"));
  }

  SECTION("empty_hyp_txt") {
    const auto result = exec(command("wer", approach, "empty.ref.txt", "empty.hyp.txt", sbs_output));

    REQUIRE_THAT(result, Contains("WER: 3/3 = 1.0000"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:3 SUB:0"));
  }

  SECTION("empty_ref_ctm") {
    const auto result = exec(command("wer", approach, "empty.hyp.ctm", "empty.ref.txt", sbs_output));

    REQUIRE_THAT(result, Contains("WER: 3/0 ="));
    REQUIRE_THAT(result, Contains("WER: INS:3 DEL:0 SUB:0"));
  }

  SECTION("empty_ref_nlp") {
    const auto result = exec(command("wer", approach, "empty.hyp.nlp", "empty.ref.txt", sbs_output));

    REQUIRE_THAT(result, Contains("WER: 3/0 ="));
    REQUIRE_THAT(result, Contains("WER: INS:3 DEL:0 SUB:0"));
  }
  SECTION("empty_ref_txt") {
    const auto result = exec(command("wer", approach, "empty.hyp.txt", "empty.ref.txt", sbs_output));

    REQUIRE_THAT(result, Contains("WER: 3/0 ="));
    REQUIRE_THAT(result, Contains("WER: INS:3 DEL:0 SUB:0"));
  }

  SECTION("syn_1") {
    const auto result = exec(command("wer", approach, "syn_1.ref.txt", "syn_1.hyp.txt", sbs_output));
    const auto testFile = std::string{TEST_DATA} + "syn_1.hyp.sbs";

    REQUIRE_THAT(result, Contains("WER: 8/21 = 0.3810"));
    REQUIRE_THAT(result, Contains("WER: INS:3 DEL:2 SUB:3"));

    REQUIRE(compareFiles(sbs_output.c_str(), testFile.c_str()));
  }

  SECTION("syn_1 (with synonyms)") {
    const auto result = exec(command("wer", approach, "syn_1.ref.txt", "syn_1.hyp.txt", sbs_output, "", TEST_SYNONYMS));

    REQUIRE_THAT(result, Contains("WER: 5/21 = 0.2381"));
    REQUIRE_THAT(result, Contains("WER: INS:3 DEL:2 SUB:0"));
  }

  SECTION("syn_2") {
    const auto result = exec(command("wer", approach, "syn_2.ref.txt", "syn_2.hyp.txt", sbs_output, "", TEST_SYNONYMS));

    REQUIRE_THAT(result, Contains("WER: 0/1 = 0.0000"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:0 SUB:0"));
  }

  SECTION("syn_3") {
    const auto result = exec(command("wer", approach, "syn_3.ref.txt", "syn_3.hyp.txt", sbs_output, "", TEST_SYNONYMS));

    REQUIRE_THAT(result, Contains("WER: 0/2 = 0.0000"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:0 SUB:0"));
  }

  SECTION("syn_4") {
    const auto result = exec(command("wer", approach, "syn_4.ref.txt", "syn_4.hyp.txt", sbs_output, "", TEST_SYNONYMS));

    REQUIRE_THAT(result, Contains("WER: 2/2 = 1.0000"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:1 SUB:1"));
  }

  SECTION("syn_5") {
    const auto result = exec(command("wer", approach, "syn_5.ref.txt", "syn_5.hyp.txt", sbs_output, "", TEST_SYNONYMS));

    REQUIRE_THAT(result, Contains("WER: 1/2 = 0.5"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:1 SUB:0"));
  }

  SECTION("syn_6") {
    const auto result = exec(command("wer", approach, "syn_6.ref.txt", "syn_6.hyp.txt", sbs_output, "", TEST_SYNONYMS));

    REQUIRE_THAT(result, Contains("WER: 1/5 = 0.2"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:1 SUB:0"));
  }

  SECTION("syn_6 (with cutoffs disabled)") {
    const auto result =
        exec(command("wer", approach, "syn_6.ref.txt", "syn_6.hyp.txt", sbs_output, "", TEST_SYNONYMS, nullptr, true));

    REQUIRE_THAT(result, Contains("WER: 2/5 = 0.4"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:1 SUB:1"));
  }

  // synonyms inside a label

  SECTION("syn_7 (hyp1)") {
    const auto result = exec(command("wer", approach, "syn_7.ref.nlp", "syn_7.hyp.txt", sbs_output, "",
                                     TEST_DATA + "syn_7.synonym.rules.txt", "syn_7.norm.json"));

    REQUIRE_THAT(result, Contains("WER: 1/20 = 0.0500"));
    REQUIRE_THAT(result, Contains("WER: INS:1 DEL:0 SUB:0"));
  }

  SECTION("syn_7 (hyp2)") {
    const auto result = exec(command("wer", approach, "syn_7.ref.nlp", "syn_7.hyp2.txt", sbs_output, "",
                                     TEST_DATA + "syn_7.synonym.rules.txt", "syn_7.norm.json"));

    REQUIRE_THAT(result, Contains("WER: 2/20 = 0.1000"));
    REQUIRE_THAT(result, Contains("WER: INS:1 DEL:1 SUB:0"));
  }

  SECTION("syn_7 (hyp3)") {
    const auto result = exec(command("wer", approach, "syn_7.ref.nlp", "syn_7.hyp3.txt", sbs_output, "",
                                     TEST_DATA + "syn_7.synonym.rules.txt", "syn_7.norm.json"));

    REQUIRE_THAT(result, Contains("WER: 1/19 = 0.0526"));
    REQUIRE_THAT(result, Contains("WER: INS:1 DEL:0 SUB:0"));
  }

  SECTION("syn_7 (hyp4)") {
    const auto result = exec(command("wer", approach, "syn_7_ref4.nlp", "syn_7.hyp4.txt", sbs_output, "",
                                     TEST_DATA + "syn_7.synonym.rules.txt", "syn_7.norm.json"));

    REQUIRE_THAT(result, Contains("WER: 0/2 = 0.0000"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:0 SUB:0"));
    REQUIRE_THAT(result, Contains("MONEY WER: 0/0 = -nan"));
  }

  SECTION("syn_9") {
    const auto result = exec(command("wer", approach, "syn_9.ref.txt", "syn_9.hyp.txt", sbs_output, "",
                                     TEST_DATA + "syn_9.synonym.rules.txt"));

    REQUIRE_THAT(result, Contains("WER: 0/6 = 0.0"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:0 SUB:0"));
  }

  SECTION("syn_10") {
    const auto result = exec(command("wer", approach, "syn_10.ref.txt", "syn_10.hyp.txt", sbs_output, "",
                                     TEST_DATA + "syn_9.synonym.rules.txt"));

    REQUIRE_THAT(result, Contains("WER: 1/6 = 0.1667"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:0 SUB:1"));
  }

  // synonyms for noise codes

  SECTION("noise_1 (wer -- hyp1)") {
    const auto result =
        exec(command("wer", approach, "noise_1.ref.nlp", "noise.hyp1.ctm", sbs_output, "", TEST_SYNONYMS));

    REQUIRE_THAT(result, Contains("WER: 2/12 = 0.1667"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:2 SUB:0"));
  }

  SECTION("noise_1 (wer -- hyp2)") {
    const auto result =
        exec(command("wer", approach, "noise_1.ref.nlp", "noise.hyp2.ctm", sbs_output, "", TEST_SYNONYMS));

    REQUIRE_THAT(result, Contains("WER: 0/12 = 0.0"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:0 SUB:0"));
  }

  SECTION("noise_1 (align - hyp1)") {
    const auto result =
        exec(command("align", approach, "noise_1.ref.nlp", "noise.hyp1.ctm", "", nlp_output, TEST_SYNONYMS));
    const auto testFile = std::string{TEST_DATA} + "noise_1.hyp1.aligned";

    REQUIRE(compareFiles(nlp_output.c_str(), testFile.c_str()));
  }

  SECTION("noise_1 (align - hyp2)") {
    const auto result =
        exec(command("align", approach, "noise_1.ref.nlp", "noise.hyp2.ctm", "", nlp_output, TEST_SYNONYMS));
    const auto testFile = std::string{TEST_DATA} + "noise_1.hyp2.aligned";

    REQUIRE(compareFiles(nlp_output.c_str(), testFile.c_str()));
  }

  // wer around speaker information

  SECTION("speaker_1") {
    const auto result = exec(command("wer", approach, "speaker_1.ref.nlp", "speaker_1.hyp.txt", sbs_output, "",
                                     TEST_SYNONYMS, nullptr, false, 3));

    REQUIRE_THAT(result, Contains("Speaker switch WER: 1/6 = 0.1667"));
    REQUIRE_THAT(result, Contains("speaker 1 WER: 1/10 = 0.1000"));
    REQUIRE_THAT(result, Contains("speaker 2 WER: 4/11 = 0.3636"));
  }

  SECTION("speaker_2") {
    const auto result = exec(command("wer", approach, "speaker_2.ref.nlp", "speaker_2.hyp.txt", sbs_output, "",
                                     TEST_SYNONYMS, nullptr, false, 2));

    REQUIRE_THAT(result, Contains("WER: 6/19 = 0.3158"));
    REQUIRE_THAT(result, Contains("Speaker switch WER: 4/13 = 0.3077"));
    REQUIRE_THAT(result, Contains("speaker 1 WER: 1/7 = 0.1429"));
    REQUIRE_THAT(result, Contains("speaker 2 WER: 2/6 = 0.3333"));
    REQUIRE_THAT(result, Contains("speaker 3 WER: 3/6 = 0.5000"));
  }

  SECTION("speaker_3") {
    const auto result = exec(command("wer", approach, "noise_1.ref.nlp", "noise.hyp2.ctm", sbs_output, "",
                                     TEST_SYNONYMS, nullptr, false, 25));

    // Single speaker and super high context, should be nan
    REQUIRE_THAT(result, Contains("Speaker switch WER: 0/0"));
    REQUIRE_THAT(result, Contains("speaker 1 WER: 0/12 = 0.0000"));
  }

  SECTION("short file") {
    const auto result = exec(command("wer", approach, "short.ref.nlp", "short.hyp.nlp", sbs_output, "", TEST_SYNONYMS));

    REQUIRE_THAT(result, Contains("WER: 5/31 = 0.1613"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:2 SUB:2"));
  }

  SECTION("wer (nlp output)") {
    const auto result = exec(command("wer", approach, "short.ref.nlp", "short.hyp.nlp", sbs_output, nlp_output,
                                     TEST_SYNONYMS, nullptr, false, -1, "--disable-approx-alignment"));
    const auto testFile = std::string{TEST_DATA} + "short.aligned.nlp";

    REQUIRE(compareFiles(nlp_output.c_str(), testFile.c_str()));
    REQUIRE_THAT(result, Contains("WER: 5/31 = 0.1613"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:2 SUB:2"));
  }

  SECTION("Case Metrics") {
    const auto result = exec(command("wer", approach, "short.ref.nlp", "short.hyp.txt", sbs_output, nlp_output,
                                     TEST_SYNONYMS, nullptr, false, -1, "--record-case-stats"));
    const auto testFile = std::string{TEST_DATA} + "short.aligned.nlp";

    REQUIRE(compareFiles(nlp_output.c_str(), testFile.c_str()));
    REQUIRE_THAT(result, Contains("case WER, (matching words only): Precision:1.0"));
    REQUIRE_THAT(result, Contains("case WER, (all including substitutions): Precision:0.77"));
  }

  // alignment tests

  SECTION("align_1") {
    const auto result = exec(command("align", approach, "align_1.ref.nlp", "align_1.hyp.ctm", "", nlp_output,
                                     TEST_SYNONYMS, "align_1.norm.json"));
    const auto testFile = std::string{TEST_DATA} + "align_1.ref.aligned.nlp";

    REQUIRE(compareFiles(nlp_output.c_str(), testFile.c_str()));
    REQUIRE_THAT(result, Contains("WER: 1/12 = 0.0833"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:1 SUB:0"));
  }

  SECTION("align_2") {
    const auto result = exec(command("align", approach, "align_2.ref.nlp", "align_2.hyp.ctm", "", nlp_output,
                                     TEST_SYNONYMS, "align_2.norm.json"));
    const auto testFile = std::string{TEST_DATA} + "align_2.ref.aligned.nlp";

    REQUIRE(compareFiles(nlp_output.c_str(), testFile.c_str()));
    REQUIRE_THAT(result, Contains("WER: 2/10 = 0.2000"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:1 SUB:1"));
  }

  SECTION("align_3") {
    const auto result = exec(command("align", approach, "align_3.ref.nlp", "align_3.hyp.ctm", "", nlp_output,
                                     TEST_SYNONYMS, "align_3.norm.json"));
    const auto testFile = std::string{TEST_DATA} + "align_3.ref.aligned.nlp";

    REQUIRE(compareFiles(nlp_output.c_str(), testFile.c_str()));
    REQUIRE_THAT(result, Contains("WER: 1/13 = 0.0769"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:1 SUB:0"));
  }

  // insertions and deletions within classes
  SECTION("align_4 (insertion -- hyp1)") {
    const auto result = exec(command("align", approach, "align_4.ref.nlp", "align_4.hyp1.ctm", "", nlp_output,
                                     TEST_SYNONYMS, "align_4.norm.json"));
    const auto testFile = std::string{TEST_DATA} + "align_4.ref.aligned1.nlp";

    REQUIRE(compareFiles(nlp_output.c_str(), testFile.c_str()));
    REQUIRE_THAT(result, Contains("WER: 1/13 = 0.0769"));
    REQUIRE_THAT(result, Contains("WER: INS:1 DEL:0 SUB:0"));
    REQUIRE_THAT(result, Contains("class CARDINAL WER: 1/3 = 0.3333"));
  }

  SECTION("align_4 (deletion -- hyp2)") {
    const auto result = exec(command("align", approach, "align_4.ref.nlp", "align_4.hyp2.ctm", "", nlp_output,
                                     TEST_SYNONYMS, "align_4.norm.json"));
    const auto testFile = std::string{TEST_DATA} + "align_4.ref.aligned2.nlp";

    REQUIRE(compareFiles(nlp_output.c_str(), testFile.c_str()));
    REQUIRE_THAT(result, Contains("WER: 1/13 = 0.0769"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:1 SUB:0"));
    REQUIRE_THAT(result, Contains("class CARDINAL WER: 1/3 = 0.3333"));
  }

  // insertions and deletions within synonyms
  SECTION("align_5 (insertion -- hyp1)") {
    const auto result =
        exec(command("align", approach, "align_5.ref.nlp", "align_5.hyp1.ctm", "", nlp_output, TEST_SYNONYMS));
    const auto testFile = std::string{TEST_DATA} + "align_5.ref.aligned1.nlp";

    REQUIRE(compareFiles(nlp_output.c_str(), testFile.c_str()));
    REQUIRE_THAT(result, Contains("WER: 1/12 = 0.0833"));
    REQUIRE_THAT(result, Contains("WER: INS:1 DEL:0 SUB:0"));
  }

  SECTION("align_5 (deletion -- hyp2)") {
    const auto result =
        exec(command("align", approach, "align_5.ref.nlp", "align_5.hyp2.ctm", "", nlp_output, TEST_SYNONYMS));
    const auto testFile = std::string{TEST_DATA} + "align_5.ref.aligned2.nlp";

    REQUIRE_THAT(result, Contains("WER: 1/12 = 0.0833"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:1 SUB:0"));
    REQUIRE(compareFiles(nlp_output.c_str(), testFile.c_str()));
  }

  SECTION("wer_tag wer") {
    const auto testFile = std::string{TEST_DATA} + "twenty.hyp.sbs";
    const auto result = exec(command("wer", approach, "twenty.ref.testing.nlp", "twenty.hyp.txt", sbs_output, "",
                                     TEST_SYNONYMS, "twenty.ref.testing.norm.json"));
    REQUIRE_THAT(result, Contains("Wer Entity ID 1 WER: 1/1 = 1.0000"));
    REQUIRE_THAT(result, Contains("Wer Entity ID 0 WER: 1/2 = 0.5000"));
    REQUIRE_THAT(result, Contains("Wer Entity ID 2 WER: 1/2 = 0.5000"));
    REQUIRE_THAT(result, Contains("Wer Entity ID 3 WER: 1/3 = 0.3333"));
  }

  // Additional WER tests
  SECTION("entity precision recall") {
    const auto testFile = std::string{TEST_DATA} + "twenty.hyp.sbs";
    logger->info("Entity precision recall 2020.  sbs = {}", sbs_output);
    const auto result = exec(command("wer", approach, "twenty.ref.nlp", "twenty.hyp.txt", sbs_output, "", TEST_SYNONYMS,
                                     "twenty.norm.json", false, -1, "--pr_threshold 1"));
    REQUIRE(compareFiles(sbs_output.c_str(), testFile.c_str()));
  }

  SECTION("bigram_1") {
    const auto result = exec(command("wer", approach, "test1.ref.txt", "test1.hyp.txt", sbs_output));

    REQUIRE_THAT(result, Contains("WER: 10/76 = 0.1316"));
    REQUIRE_THAT(result, Contains("WER: INS:1 DEL:2 SUB:7"));
    REQUIRE_THAT(result, Contains("WER: Precision:0.893333 Recall:0.881579"));
  }

  // test oracle WER calculation with lattice FST archive as hypothesis input
  SECTION("oracle_1") {
    const auto result = exec(
        "./fstalign wer --ref ../test/data/oracle_1.ref.txt "
        "--hyp ../test/data/oracle_1.hyp.fst "
        "--symbols ../test/data/oracle_1.symbols.txt "
        "--output-sbs " +
        sbs_output);

    REQUIRE_THAT(result, Contains("WER: 1/9 = 0.1111"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:1 SUB:0"));
  }

  // cleanup (after each test)
  remove(sbs_output.c_str());
  remove(nlp_output.c_str());
}

/*

  Approach 2


*/

TEST_CASE_METHOD(UniqueTestsFixture, "main-adapted-composition()") {
  // setup (before each test) -- done sequentially before each test gets spawned off in parallel
  const auto sbs_output = getOutputSbsPath();
  const auto nlp_output = getOutputNlpPath();

  auto logger = logger::GetOrCreateLogger("main()");
  const char *approach = "--composition-approach adapted";

  SECTION("empty_hyp_ctm") {
    const auto result = exec(command("wer", approach, "empty.ref.txt", "empty.hyp.ctm", sbs_output));

    REQUIRE_THAT(result, Contains("WER: 3/3 = 1.0000"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:3 SUB:0"));
  }

  SECTION("empty_hyp_nlp") {
    const auto result = exec(command("wer", approach, "empty.ref.txt", "empty.hyp.nlp", sbs_output));

    REQUIRE_THAT(result, Contains("WER: 3/3 = 1.0000"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:3 SUB:0"));
  }

  SECTION("empty_hyp_txt") {
    const auto result = exec(command("wer", approach, "empty.ref.txt", "empty.hyp.txt", sbs_output));

    REQUIRE_THAT(result, Contains("WER: 3/3 = 1.0000"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:3 SUB:0"));
  }

  SECTION("empty_ref_ctm") {
    const auto result = exec(command("wer", approach, "empty.hyp.ctm", "empty.ref.txt", sbs_output));

    REQUIRE_THAT(result, Contains("WER: 3/0 ="));
    REQUIRE_THAT(result, Contains("WER: INS:3 DEL:0 SUB:0"));
  }

  SECTION("empty_ref_nlp") {
    const auto result = exec(command("wer", approach, "empty.hyp.nlp", "empty.ref.txt", sbs_output));

    REQUIRE_THAT(result, Contains("WER: 3/0 ="));
    REQUIRE_THAT(result, Contains("WER: INS:3 DEL:0 SUB:0"));
  }
  SECTION("empty_ref_txt") {
    const auto result = exec(command("wer", approach, "empty.hyp.txt", "empty.ref.txt", sbs_output));

    REQUIRE_THAT(result, Contains("WER: 3/0 ="));
    REQUIRE_THAT(result, Contains("WER: INS:3 DEL:0 SUB:0"));
  }

  SECTION("syn_1") {
    const auto result = exec(command("wer", approach, "syn_1.ref.txt", "syn_1.hyp.txt", sbs_output));
    const auto testFile = std::string{TEST_DATA} + "syn_1.hyp.sbs";

    REQUIRE_THAT(result, Contains("WER: 8/21 = 0.3810"));
    REQUIRE_THAT(result, Contains("WER: INS:3 DEL:2 SUB:3"));

    REQUIRE(compareFiles(sbs_output.c_str(), testFile.c_str()));
  }

  SECTION("syn_1 (with synonyms)") {
    const auto result = exec(command("wer", approach, "syn_1.ref.txt", "syn_1.hyp.txt", sbs_output, "", TEST_SYNONYMS));

    REQUIRE_THAT(result, Contains("WER: 5/21 = 0.2381"));
    REQUIRE_THAT(result, Contains("WER: INS:3 DEL:2 SUB:0"));
  }

  SECTION("syn_2") {
    const auto result = exec(command("wer", approach, "syn_2.ref.txt", "syn_2.hyp.txt", sbs_output, "", TEST_SYNONYMS));

    REQUIRE_THAT(result, Contains("WER: 0/1 = 0.0000"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:0 SUB:0"));
  }

  SECTION("syn_3") {
    const auto result = exec(command("wer", approach, "syn_3.ref.txt", "syn_3.hyp.txt", sbs_output, "", TEST_SYNONYMS));

    REQUIRE_THAT(result, Contains("WER: 0/2 = 0.0000"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:0 SUB:0"));
  }

  SECTION("syn_4") {
    const auto result = exec(command("wer", approach, "syn_4.ref.txt", "syn_4.hyp.txt", sbs_output, "", TEST_SYNONYMS));

    REQUIRE_THAT(result, Contains("WER: 2/2 = 1.0000"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:1 SUB:1"));
  }

  SECTION("syn_5") {
    const auto result = exec(command("wer", approach, "syn_5.ref.txt", "syn_5.hyp.txt", sbs_output, "", TEST_SYNONYMS));

    REQUIRE_THAT(result, Contains("WER: 2/1 = 2.0"));
  }

  SECTION("syn_6") {
    const auto result = exec(command("wer", approach, "syn_6.ref.txt", "syn_6.hyp.txt", sbs_output, "", TEST_SYNONYMS));

    REQUIRE_THAT(result, Contains("WER: 1/5 = 0.2"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:1 SUB:0"));
  }

  SECTION("syn_6 (with cutoffs disabled)") {
    const auto result =
        exec(command("wer", approach, "syn_6.ref.txt", "syn_6.hyp.txt", sbs_output, "", TEST_SYNONYMS, nullptr, true));

    REQUIRE_THAT(result, Contains("WER: 2/5 = 0.4"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:1 SUB:1"));
  }

  // synonyms inside a label

  SECTION("syn_7 (hyp1)") {
    const auto result = exec(command("wer", approach, "syn_7.ref.nlp", "syn_7.hyp.txt", sbs_output, "",
                                     TEST_DATA + "syn_7.synonym.rules.txt", "syn_7.norm.json"));

    REQUIRE_THAT(result, Contains("WER: 1/20 = 0.0500"));
    REQUIRE_THAT(result, Contains("WER: INS:1 DEL:0 SUB:0"));
  }

  SECTION("syn_7 (hyp2)") {
    const auto result = exec(command("wer", approach, "syn_7.ref.nlp", "syn_7.hyp2.txt", sbs_output, "",
                                     TEST_DATA + "syn_7.synonym.rules.txt", "syn_7.norm.json"));

    REQUIRE_THAT(result, Contains("WER: 4/18 = 0.2222"));
    REQUIRE_THAT(result, Contains("WER: INS:2 DEL:0 SUB:2"));
  }

  SECTION("syn_7 (hyp3)") {
    const auto result = exec(command("wer", approach, "syn_7.ref.nlp", "syn_7.hyp3.txt", sbs_output, "",
                                     TEST_DATA + "syn_7.synonym.rules.txt", "syn_7.norm.json"));

    REQUIRE_THAT(result, Contains("WER: 1/19 = 0.0526"));
    REQUIRE_THAT(result, Contains("WER: INS:1 DEL:0 SUB:0"));
  }

  SECTION("syn_7 (hyp4)") {
    const auto result = exec(command("wer", approach, "syn_7_ref4.nlp", "syn_7.hyp4.txt", sbs_output, "",
                                     TEST_DATA + "syn_7.synonym.rules.txt", "syn_7.norm.json"));

    REQUIRE_THAT(result, Contains("WER: 0/2 = 0.0000"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:0 SUB:0"));
    REQUIRE_THAT(result, Contains("MONEY WER: 0/0 = -nan"));
  }

  SECTION("syn_9") {
    const auto result = exec(command("wer", approach, "syn_9.ref.txt", "syn_9.hyp.txt", sbs_output, "",
                                     TEST_DATA + "syn_9.synonym.rules.txt"));

    REQUIRE_THAT(result, Contains("WER: 0/6 = 0.0"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:0 SUB:0"));
  }

  SECTION("syn_10") {
    const auto result = exec(command("wer", approach, "syn_10.ref.txt", "syn_10.hyp.txt", sbs_output, "",
                                     TEST_DATA + "syn_9.synonym.rules.txt"));

    REQUIRE_THAT(result, Contains("WER: 1/6 = 0.1667"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:0 SUB:1"));
  }

  // synonyms for noise codes

  SECTION("noise_1 (wer -- hyp1)") {
    const auto result =
        exec(command("wer", approach, "noise_1.ref.nlp", "noise.hyp1.ctm", sbs_output, "", TEST_SYNONYMS));

    REQUIRE_THAT(result, Contains("WER: 2/12 = 0.1667"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:2 SUB:0"));
  }

  SECTION("noise_1 (wer -- hyp2)") {
    const auto result =
        exec(command("wer", approach, "noise_1.ref.nlp", "noise.hyp2.ctm", sbs_output, "", TEST_SYNONYMS));

    REQUIRE_THAT(result, Contains("WER: 0/12 = 0.0"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:0 SUB:0"));
  }

  SECTION("noise_1 (align - hyp1)") {
    const auto result =
        exec(command("align", approach, "noise_1.ref.nlp", "noise.hyp1.ctm", "", nlp_output, TEST_SYNONYMS));
    const auto testFile = std::string{TEST_DATA} + "noise_1.hyp1.aligned";

    REQUIRE(compareFiles(nlp_output.c_str(), testFile.c_str()));
  }

  SECTION("noise_1 (align - hyp2)") {
    const auto result =
        exec(command("align", approach, "noise_1.ref.nlp", "noise.hyp2.ctm", "", nlp_output, TEST_SYNONYMS));
    const auto testFile = std::string{TEST_DATA} + "noise_1.hyp2.aligned";

    REQUIRE(compareFiles(nlp_output.c_str(), testFile.c_str()));
  }

  // wer around speaker information

  SECTION("speaker_1") {
    const auto result = exec(command("wer", approach, "speaker_1.ref.nlp", "speaker_1.hyp.txt", sbs_output, "",
                                     TEST_SYNONYMS, nullptr, false, 3));

    REQUIRE_THAT(result, Contains("Speaker switch WER: 1/6 = 0.1667"));
    REQUIRE_THAT(result, Contains("speaker 1 WER: 1/10 = 0.1000"));
    REQUIRE_THAT(result, Contains("speaker 2 WER: 4/11 = 0.3636"));
  }

  SECTION("speaker_2") {
    const auto result = exec(command("wer", approach, "speaker_2.ref.nlp", "speaker_2.hyp.txt", sbs_output, "",
                                     TEST_SYNONYMS, nullptr, false, 2));

    REQUIRE_THAT(result, Contains("WER: 6/19 = 0.3158"));
    REQUIRE_THAT(result, Contains("Speaker switch WER: 4/13 = 0.3077"));
    REQUIRE_THAT(result, Contains("speaker 1 WER: 1/7 = 0.1429"));
    REQUIRE_THAT(result, Contains("speaker 2 WER: 2/6 = 0.3333"));
    REQUIRE_THAT(result, Contains("speaker 3 WER: 3/6 = 0.5000"));
  }

  SECTION("speaker_3") {
    const auto result = exec(command("wer", approach, "noise_1.ref.nlp", "noise.hyp2.ctm", sbs_output, "",
                                     TEST_SYNONYMS, nullptr, false, 25));

    // Single speaker and super high context, should be nan
    REQUIRE_THAT(result, Contains("Speaker switch WER: 0/0"));
    REQUIRE_THAT(result, Contains("speaker 1 WER: 0/12 = 0.0000"));
  }

  SECTION("short file") {
    const auto result = exec(command("wer", approach, "short.ref.nlp", "short.hyp.nlp", sbs_output, "", TEST_SYNONYMS));

    REQUIRE_THAT(result, Contains("WER: 6/32 = 0.1875"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:3 SUB:3"));
  }

  SECTION("wer (nlp output)") {
    const auto result =
        exec(command("wer", approach, "short.ref.nlp", "short.hyp.nlp", sbs_output, nlp_output, TEST_SYNONYMS));
    const auto testFile = std::string{TEST_DATA} + "short.aligned.strict.nlp";

    REQUIRE(compareFiles(nlp_output.c_str(), testFile.c_str()));
    REQUIRE_THAT(result, Contains("WER: 6/32 = 0.1875"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:3 SUB:3"));
  }

  // alignment tests

  SECTION("align_1") {
    const auto result = exec(command("align", approach, "align_1.ref.nlp", "align_1.hyp.ctm", "", nlp_output,
                                     TEST_SYNONYMS, "align_1.norm.json"));
    const auto testFile = std::string{TEST_DATA} + "align_1.ref.aligned.nlp";

    REQUIRE(compareFiles(nlp_output.c_str(), testFile.c_str()));
    REQUIRE_THAT(result, Contains("WER: 1/12 = 0.0833"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:1 SUB:0"));
  }

  SECTION("align_2") {
    const auto result = exec(command("align", approach, "align_2.ref.nlp", "align_2.hyp.ctm", "", nlp_output,
                                     TEST_SYNONYMS, "align_2.norm.json"));
    const auto testFile = std::string{TEST_DATA} + "align_2.ref.aligned.nlp";

    REQUIRE(compareFiles(nlp_output.c_str(), testFile.c_str()));
    REQUIRE_THAT(result, Contains("WER: 2/10 = 0.2000"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:1 SUB:1"));
  }

  SECTION("align_3") {
    const auto result = exec(command("align", approach, "align_3.ref.nlp", "align_3.hyp.ctm", "", nlp_output,
                                     TEST_SYNONYMS, "align_3.norm.json"));
    const auto testFile = std::string{TEST_DATA} + "align_3.ref.aligned.nlp";

    REQUIRE(compareFiles(nlp_output.c_str(), testFile.c_str()));
    REQUIRE_THAT(result, Contains("WER: 1/13 = 0.0769"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:1 SUB:0"));
  }

  // insertions and deletions within classes
  SECTION("align_4 (insertion -- hyp1)") {
    const auto result = exec(command("align", approach, "align_4.ref.nlp", "align_4.hyp1.ctm", "", nlp_output,
                                     TEST_SYNONYMS, "align_4.norm.json"));
    const auto testFile = std::string{TEST_DATA} + "align_4.ref.aligned1.nlp";

    REQUIRE(compareFiles(nlp_output.c_str(), testFile.c_str()));
    REQUIRE_THAT(result, Contains("WER: 1/13 = 0.0769"));
    REQUIRE_THAT(result, Contains("WER: INS:1 DEL:0 SUB:0"));
    REQUIRE_THAT(result, Contains("class CARDINAL WER: 1/3 = 0.3333"));
  }

  SECTION("align_4 (deletion -- hyp2)") {
    const auto result = exec(command("align", approach, "align_4.ref.nlp", "align_4.hyp2.ctm", "", nlp_output,
                                     TEST_SYNONYMS, "align_4.norm.json"));
    const auto testFile = std::string{TEST_DATA} + "align_4.ref.aligned2.nlp";

    REQUIRE(compareFiles(nlp_output.c_str(), testFile.c_str()));
    REQUIRE_THAT(result, Contains("WER: 1/13 = 0.0769"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:1 SUB:0"));
    REQUIRE_THAT(result, Contains("class CARDINAL WER: 1/3 = 0.3333"));
  }

  // insertions and deletions within synonyms
  SECTION("align_5 (insertion -- hyp1)") {
    const auto result =
        exec(command("align", approach, "align_5.ref.nlp", "align_5.hyp1.ctm", "", nlp_output, TEST_SYNONYMS));
    const auto testFile = std::string{TEST_DATA} + "align_5.ref.aligned1-2.nlp";
    REQUIRE(compareFiles(nlp_output.c_str(), testFile.c_str()));
    REQUIRE_THAT(result, Contains("WER: 3/11 = 0.2727"));
    REQUIRE_THAT(result, Contains("WER: INS:2 DEL:0 SUB:1"));
  }

  SECTION("align_5 (deletion -- hyp2)") {
    const auto result =
        exec(command("align", approach, "align_5.ref.nlp", "align_5.hyp2.ctm", "", nlp_output, TEST_SYNONYMS));
    const auto testFile = std::string{TEST_DATA} + "align_5.ref.aligned2-a2.nlp";

    REQUIRE_THAT(result, Contains("WER: 1/11 = 0.0909"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:0 SUB:1"));
    REQUIRE(compareFiles(nlp_output.c_str(), testFile.c_str()));
  }

  SECTION("wer_tag wer") {
    const auto testFile = std::string{TEST_DATA} + "twenty.hyp.sbs";
    auto cmd = command("wer", approach, "twenty.ref.testing.nlp", "twenty.hyp.txt", sbs_output, "", TEST_SYNONYMS,
                       "twenty.ref.testing.norm.json");

    std::cout << "cmd = " << cmd << std::endl;
    const auto result = exec(cmd);
    REQUIRE_THAT(result, Contains("Wer Entity ID 1 WER: 1/1 = 1.0000"));
    REQUIRE_THAT(result, Contains("Wer Entity ID 0 WER: 1/2 = 0.5000"));
    REQUIRE_THAT(result, Contains("Wer Entity ID 2 WER: 1/2 = 0.5000"));
    REQUIRE_THAT(result, Contains("Wer Entity ID 3 WER: 1/3 = 0.3333"));
  }

  // Additional WER tests
  SECTION("entity precision recall") {
    const auto testFile = std::string{TEST_DATA} + "twenty.hyp-a2.sbs";
    logger->info("Entity precision recall 2020.  sbs = {}", sbs_output);
    const auto result = exec(command("wer", approach, "twenty.ref.nlp", "twenty.hyp.txt", sbs_output, "", TEST_SYNONYMS,
                                     "twenty.norm.json", false, -1, "--pr_threshold 1"));
    REQUIRE(compareFiles(sbs_output.c_str(), testFile.c_str()));
  }

  SECTION("bigram_1") {
    const auto result = exec(command("wer", approach, "test1.ref.txt", "test1.hyp.txt", sbs_output));

    REQUIRE_THAT(result, Contains("WER: 10/76 = 0.1316"));
    REQUIRE_THAT(result, Contains("WER: INS:1 DEL:2 SUB:7"));
    REQUIRE_THAT(result, Contains("WER: Precision:0.893333 Recall:0.881579"));
  }

  // test oracle WER calculation with lattice FST archive as hypothesis input
  SECTION("oracle_1") {
    const auto result = exec(
        "./fstalign wer --ref ../test/data/oracle_1.ref.txt "
        "--hyp ../test/data/oracle_1.hyp.fst "
        "--symbols ../test/data/oracle_1.symbols.txt "
        "--output-sbs " +
        sbs_output);

    REQUIRE_THAT(result, Contains("WER: 1/9 = 0.1111"));
    REQUIRE_THAT(result, Contains("WER: INS:0 DEL:1 SUB:0"));
  }

  // cleanup (after each test)
  remove(sbs_output.c_str());
  remove(nlp_output.c_str());
}
