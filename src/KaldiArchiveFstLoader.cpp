/*
 * KaldiArchiveFstLoader.cpp
 */
#include "KaldiArchiveFstLoader.h"

// empty constructor
KaldiArchiveFstLoader::KaldiArchiveFstLoader(std::string filename) : FstLoader(), filename_(filename) {}

void KaldiArchiveFstLoader::addToSymbolTable(fst::SymbolTable& symbol) const { return; }

struct utterance_id_compare {
  bool operator()(const std::string& s1, const std::string& s2) const {
    std::regex re("chunk(\\d+)");
    std::cmatch m1, m2;
    if (std::regex_search(s1.c_str(), m1, re) && std::regex_search(s2.c_str(), m2, re)) {
      int s1_chunk = std::stoi(m1.str(1));
      int s2_chunk = std::stoi(m2.str(1));
      if (s1_chunk != s2_chunk) {
        return (s1_chunk < s2_chunk);
      }
    }
    return s1 <= s2;
  }
};

fst::StdVectorFst KaldiArchiveFstLoader::convertToFst(const fst::SymbolTable& symbol) const {
  auto logger = logger::GetOrCreateLogger("KaldiArchiveFstLoader");

  fst::StdVectorFst transducer;

  transducer.AddState();
  transducer.SetStart(0);

  std::map<std::string, fst::StdVectorFst, utterance_id_compare> fsts_in_archive;
  kaldi::SequentialTableReader<fst::VectorFstHolder> fst_reader(filename_);
  for (; !fst_reader.Done(); fst_reader.Next()) {
    fsts_in_archive[fst_reader.Key()] = fst_reader.Value();
  }

  logger->info("Reading in {} FSTs from archive.", fsts_in_archive.size());

  // Loop from (last-1) to first, as 'prepending' the fsts is faster,
  // see: http://www.openfst.org/twiki/bin/view/FST/ConcatDoc

  for (auto it = fsts_in_archive.rbegin(); it != fsts_in_archive.rend(); ++it) {
    fst::Minimize(&it->second);
    fst::Concat(it->second, &transducer);
  }

  // Since we concatenated in reverse, set the final state
  transducer.SetFinal(0, 0.0f);

  logger->info("Total lattice FST has {} states.", transducer.NumStates());
  return transducer;
}

KaldiArchiveFstLoader::~KaldiArchiveFstLoader() {}
