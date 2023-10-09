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
OneBestFstLoader::OneBestFstLoader(bool use_case) : FstLoader() {
  mUseCase = use_case;
}

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
    if (!mUseCase) {
      token = UnicodeLowercase(token);
    }
    // fst::kNoSymbol
    if (symbol.Find(token) == -1) {
      symbol.AddSymbol(token);
    }
  }
}

fst::StdVectorFst OneBestFstLoader::convertToFst(const fst::SymbolTable &symbol, std::vector<int> map) const {
  auto logger = logger::GetOrCreateLogger("OneBestFstLoader");

  FstAlignOption options;
  int eps_sym = symbol.Find(options.symEps);

  fst::StdVectorFst transducer;

  transducer.AddState();
  transducer.SetStart(0);

  int prevState = 0;
  int nextState = 1;
  int map_sz = map.size();
  int wc = 0;
  for (TokenType::const_iterator i = mToken.begin(); i != mToken.end(); ++i) {
    std::string token = *i;
    if (!mUseCase) {
      token = UnicodeLowercase(token);
    }
    transducer.AddState();

    int tk_idx = symbol.Find(token);
    if (tk_idx < 0) {
      logger->trace("we found an invalid token [{}] at token position {} which gave a label id of {}", token, (wc + 1),
                    tk_idx);
    }
    if (map_sz > wc && map[wc] > 0) {
      transducer.AddArc(prevState, fst::StdArc(tk_idx, tk_idx, 1.0f, nextState));
    } else {
      transducer.AddArc(prevState, fst::StdArc(tk_idx, tk_idx, 0.0f, nextState));
    }

    prevState = nextState;
    nextState++;
    wc++;
  }

  int realFinal = transducer.AddState();
  transducer.AddArc(prevState, fst::StdArc(eps_sym, eps_sym, 0.0f, realFinal));
  transducer.SetFinal(realFinal, StdFst::Weight::One());
  return transducer;
}

std::vector<int> OneBestFstLoader::convertToIntVector(fst::SymbolTable &symbol) const {
  auto logger = logger::GetOrCreateLogger("OneBestFstLoader");
  std::vector<int> vect;
  addToSymbolTable(symbol);
  int sz = mToken.size();
  logger->info("creating std::vector<int> for OneBestFstLoader for {} tokens", sz);
  vect.reserve(sz);

  FstAlignOption options;
  for (TokenType::const_iterator i = mToken.begin(); i != mToken.end(); ++i) {
    std::string token = *i;
    if (!mUseCase) {
      token = UnicodeLowercase(token);
    }
    int token_sym = symbol.Find(token);
    if (token_sym == -1) {
      token_sym = symbol.Find(options.symUnk);
    }
    vect.emplace_back(token_sym);
  }

  return vect;
}

OneBestFstLoader::~OneBestFstLoader() {
  // TODO Auto-generated destructor stub
}
