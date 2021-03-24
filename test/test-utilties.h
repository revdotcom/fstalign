#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <string>
#include "src/logging.h"
#ifndef __TEST_UTILITIES_H__
#define __TEST_UTILITIES_H__ 1
const auto TEST_BINARY = "./fstalign";
const auto TEST_DATA = "../sample_data/tests/";
const auto TEST_SYNONYMS = "../synonyms.rules.txt";
#ifdef WINDOWS
#include <direct.h>
#define GetCurrentDir _getcwd
#else
#include <unistd.h>
#define GetCurrentDir getcwd
#endif

void pclose_test(FILE *fp) {
  int status = pclose(fp);
  if (status != 0) {
    throw std::runtime_error("exit status non-zero!");
  }
}

std::string get_current_dir() {
  char buff[FILENAME_MAX];  // create string buffer to hold path
  GetCurrentDir(buff, FILENAME_MAX);
  std::string current_working_dir(buff);
  return current_working_dir;
}

// Executes a specific shell command, and returns a string containing the output
std::string exec(const std::string &cmd) {
  const size_t length = 256;
  std::array<char, length> buffer;

  std::shared_ptr<FILE> pipe{popen(cmd.c_str(), "r"), pclose_test};
  if (!pipe) {
    throw std::runtime_error("popen() failed!");
  }

  std::string result;
  while (!feof(pipe.get())) {
    if (fgets(buffer.data(), length, pipe.get()) != nullptr) result += buffer.data();
  }

  return result;
}

// Generates a specific fstalign command given certain flag values
std::string command(const char *subcommand, const char *approach, const char *reference, const char *hypothesis,
                    const std::string output_sbs = "", const std::string output_nlp = "",
                    const char *synonyms = nullptr, const char *refJson = nullptr, const bool disableCutoffs = false,
                    const int speakerSwitchContextSize = -1, const std::string extraFlags = "") {
  const auto ref = std::string{"--ref "} + TEST_DATA + reference;
  const auto hyp = std::string{"--hyp "} + TEST_DATA + hypothesis;

  auto cmd = std::string{TEST_BINARY} + " " + subcommand + " " + approach + " " + ref + " " + hyp;
  // useful for debugging test
  //   auto logger = logger::GetOrCreateLogger("main()");
  //   logger->info("final command is {}", cmd);

  if (synonyms != nullptr) {
    cmd = cmd + " --syn " + TEST_DATA + synonyms;
  }
  if (refJson != nullptr) {
    cmd = cmd + " --ref-json " + TEST_DATA + refJson;
  }

  if (disableCutoffs) {
    cmd += " --disable-cutoffs";
  }

  if (speakerSwitchContextSize > 0) {
    cmd += " --speaker-switch-context " + std::to_string(speakerSwitchContextSize);
  }

  if (!output_sbs.empty()) {
    cmd = cmd + " --output-sbs " + output_sbs;
  }

  if (!output_nlp.empty()) {
    cmd = cmd + " --output-nlp " + output_nlp;
  }

  if (!extraFlags.empty()) {
    cmd += " " + extraFlags;
  }

  return cmd;
}

// Compares two test files for exact equality
bool compareFiles(const std::string &p1, const std::string &p2) {
  std::ifstream f1(p1, std::ifstream::binary | std::ifstream::ate);
  std::ifstream f2(p2, std::ifstream::binary | std::ifstream::ate);

  // useful for debugging test
    auto logger = logger::GetOrCreateLogger("main()");
  //   logger->info("comparing {} with {}", p1, p2);

  if (f1.fail() || f2.fail()) {
    logger->info("comparing {} with {}", p1, p2);
    logger->info("some file can't be opened");
    return false;  // file problem
  }

  if (f1.tellg() != f2.tellg()) {
    logger->info("comparing {} with {}", p1, p2);
    logger->info("files sizes don't match {}, vs {}", f1.tellg(), f2.tellg());
    return false;  // size mismatch
  }

  // seek back to beginning and use std::equal to compare contents
  f1.seekg(0, std::ifstream::beg);
  f2.seekg(0, std::ifstream::beg);
  return std::equal(std::istreambuf_iterator<char>(f1.rdbuf()), std::istreambuf_iterator<char>(),
                    std::istreambuf_iterator<char>(f2.rdbuf()));
}

#endif
