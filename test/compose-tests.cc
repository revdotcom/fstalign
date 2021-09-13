#define CATCH_CONFIG_MAIN
#include <queue>
#include <set>
#include "../third-party/catch2/single_include/catch2/catch.hpp"
#include "compose-tests-utils.h"
#include "test-utilties.h"

using Catch::Matchers::Contains;

#include "src/AdaptedComposition.h"
#include "src/logging.h"

// there just to setup the loggers
TEST_CASE("STATIC_REQUIRE showcase", "[traits]") {
  logger::InitLoggers("");
  STATIC_REQUIRE(std::is_void<void>::value);
  STATIC_REQUIRE_FALSE(std::is_void<int>::value);
}

// TODO: add degenerated case, where all words in CTM are <unk> or no words at all are available
TEST_CASE("CheckEntity") {
  SECTION("synonyms") {
    REQUIRE(isSynonymLabel("___100000_SYN_1-1___"));
    REQUIRE(isEntityLabel("___100000_SYN_1-1___"));

    REQUIRE(isSynonymLabel("___90_CARDINAL___") == false);
    REQUIRE(isEntityLabel("___90_CARDINAL___"));
    REQUIRE(isEntityLabel("___90___"));
    REQUIRE(isEntityLabel("__90__") == false);

    REQUIRE(isSynonymLabel("___100000_syn_1-1___") == false);
    REQUIRE(isSynonymLabel("___100000SYN_1-1___") == false);
  }
}
TEST_CASE("composition()") {
  SECTION("simple1") {
    auto logger = logger::GetOrCreateLogger("simple1");
    logger->info("starting");

    fst::StdVectorFst a = GetStdFstA();
    fst::StdVectorFst b = GetStdFstB();
    AdaptedCompositionFst composer(a, b);

    REQUIRE(composer.Start() == 0);

    auto s = composer.Start();
    vector<StdArc> arcs;
    bool ret_status = composer.TryGetArcsAtState(s, &arcs);

    REQUIRE(ret_status);
    REQUIRE(arcs.size() == 3);

    REQUIRE(true);
  }

  SECTION("perfect match") {
    auto logger = logger::GetOrCreateLogger("perfect match");
    logger->info("starting");

    SymbolTable symbols;
    symbols.AddSymbol("<eps>");
    symbols.AddSymbol("<del>");
    symbols.AddSymbol("<ins>");
    symbols.AddSymbol("<sub>");

    auto a = GetFstFromString(&symbols, "this is a test");
    auto b = GetFstFromString(&symbols, "this is a test");

    logger->info("symbols has {} entries, fst has {} states", symbols.NumSymbols(), a.NumStates());

    AdaptedCompositionFst composer(a, b);
    auto s = composer.Start();
    REQUIRE(s == 0);

    // given that we have a match for each words, we should always have 1 arc per state and one composed state per pair
    // of input arcs (0,0) -> 0 (1,1) -> 1 (2,2) -> 2 (3,3) -> 3
    int current_state = s;
    for (int i = 0; i < 7; i++) {
      vector<StdArc> arcs_leaving_state;
      bool ret_status = composer.TryGetArcsAtState(current_state, &arcs_leaving_state);
      logger->info("({}) from state {}, we have {} arcs leaving with a ret-status {}", i, current_state,
                   arcs_leaving_state.size(), ret_status);
      REQUIRE(ret_status);

      if (i == 6) {
        // final state
        REQUIRE(arcs_leaving_state.size() == 0);
        logger->info("({}) we expect composed state id {} to have a weight one One()", i, current_state);
        REQUIRE(composer.Final(current_state) == StdFst::Weight::One());
      } else {
        if (i >= 4) {
          REQUIRE(arcs_leaving_state.size() == 1);
        } else {
          REQUIRE(arcs_leaving_state.size() == 3);
        }
        for (vector<StdArc>::iterator iter = arcs_leaving_state.begin(); iter != arcs_leaving_state.end(); ++iter) {
          const fst::StdArc arc = *iter;
          logger->info("({}) arc leaving state {} to {} with label {}/{} ({}/{})", i, current_state, arc.nextstate,
                       arc.ilabel, arc.olabel, symbols.Find(arc.ilabel), symbols.Find(arc.olabel));

          logger->info("({}) we expect composed state id {} to have a weight one Zero()", i, current_state);
          REQUIRE(composer.Final(current_state) == StdFst::Weight::Zero());

          current_state = arc.nextstate;
        }
      }
    }
  }

  SECTION("deletion at the end") {
    auto logger = logger::GetOrCreateLogger("deletions");
    logger->info("starting");

    SymbolTable symbols;
    symbols.AddSymbol("<eps>");
    symbols.AddSymbol("<del>");
    symbols.AddSymbol("<ins>");
    symbols.AddSymbol("<sub>");

    auto a = GetFstFromString(&symbols, "this is a test with some extra words at the end");
    auto b = GetFstFromString(&symbols, "this is a test");

    logger->info("symbols has {} entries, fst has {} states", symbols.NumSymbols(), a.NumStates());

    AdaptedCompositionFst composer(a, b);
    auto s = composer.Start();
    REQUIRE(s == 0);

    // given that we have a match for each words, we should always have 1 arc per state and one composed state per pair
    // of input arcs (0,0) -> 0 (1,1) -> 1 (2,2) -> 2 (3,3) -> 3
    int current_state = s;

    // The test here is to check that we can reach a final node where the word "end" is deleted.
    std::queue<int> states_to_process;
    std::set<int> states_explored;

    states_to_process.push(s);

    vector<StdArc> arcs_leaving_state;
    bool found_deleted_end = false;
    while (states_to_process.size() > 0) {
      current_state = states_to_process.front();
      states_to_process.pop();

      if (states_explored.find(current_state) != states_explored.end()) {
        continue;
      }
      states_explored.insert(current_state);

      bool ret_status = composer.TryGetArcsAtState(current_state, &arcs_leaving_state);
      logger->info("from state {}, we have {} arcs leaving with a ret-status {}", current_state,
                   arcs_leaving_state.size(), ret_status);
      REQUIRE(ret_status);

      for (vector<StdArc>::iterator iter = arcs_leaving_state.begin(); iter != arcs_leaving_state.end(); ++iter) {
        const fst::StdArc arc = *iter;
        logger->info("arc leaving state {} to {} with label {}/{} ({}/{})", current_state, arc.nextstate, arc.ilabel,
                     arc.olabel, symbols.Find(arc.ilabel), symbols.Find(arc.olabel));

        if (arc.nextstate != current_state && states_explored.find(arc.nextstate) == states_explored.end()) {
          states_to_process.push(arc.nextstate);
        }

        if (symbols.Find(arc.ilabel) == "end" && arc.olabel == 0) {
          found_deleted_end = true;
        }
      }
    }

    REQUIRE(found_deleted_end);
  }
}
