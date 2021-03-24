/*
 * OneBestFstLoader.cpp
 * JP Robichaud (jp@rev.com)
 * 2018
 */

#include "OneBestFstLoader.h"

#include <fstream>
#include <stdexcept>

#include "utilities.h"

// empty constructor
OneBestFstLoader::OneBestFstLoader() : FstLoader() {}

void OneBestFstLoader::BuildFromString(const std::string content) {
  std::istringstream mystream(content);
  std::copy(std::istream_iterator<std::string>(mystream), std::istream_iterator<std::string>(),
            std::back_inserter(mToken));
}

void OneBestFstLoader::LoadTextFile(const std::string filename) {
  std::ifstream stream(filename);

  if (!stream.is_open()) throw std::runtime_error("Cannot open input file");

  std::copy(std::istream_iterator<std::string>(stream), std::istream_iterator<std::string>(),
            std::back_inserter(mToken));

  stream.close();
}

void OneBestFstLoader::addToSymbolTable(fst::SymbolTable &symbol) const {
  for (TokenType::const_iterator i = mToken.begin(); i != mToken.end(); ++i) {
    std::string token = *i;
    // putting everything to lowercase
    std::transform(token.begin(), token.end(), token.begin(), ::tolower);
    // fst::kNoSymbol
    if (symbol.Find(token) == -1) {
      symbol.AddSymbol(token);
    }
  }
}

fst::StdVectorFst OneBestFstLoader::convertToFst(const fst::SymbolTable &symbol) const {
  auto logger = logger::GetOrCreateLogger("OneBestFstLoader");

  FstAlignOption options;
  int eps_sym = symbol.Find(options.symEps);

  fst::StdVectorFst transducer;

  transducer.AddState();
  transducer.SetStart(0);

  int prevState = 0;
  int nextState = 1;
  int wc = 0;
  for (TokenType::const_iterator i = mToken.begin(); i != mToken.end(); ++i) {
    wc++;
    std::string token = *i;
    std::transform(token.begin(), token.end(), token.begin(), ::tolower);
    transducer.AddState();

    int tk_idx = symbol.Find(token);
    if (tk_idx < 0) {
      logger->trace("we found an invalid token [{}] at token position {} which gave a label id of {}", token, wc,
                    tk_idx);
    }

    transducer.AddArc(prevState, fst::StdArc(tk_idx, tk_idx, 0.0f, nextState));
    prevState = nextState;
    nextState++;
  }

  int realFinal = transducer.AddState();
  transducer.AddArc(prevState, fst::StdArc(eps_sym, eps_sym, 0.0f, realFinal));
  transducer.SetFinal(realFinal, StdFst::Weight::One());
  return transducer;
}

OneBestFstLoader::~OneBestFstLoader() {
  // TODO Auto-generated destructor stub
}
