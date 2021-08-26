#define CATCH_CONFIG_MAIN
#include "../third-party/catch2/single_include/catch2/catch.hpp"
#include "src/fast-d.h"

typedef std::vector<int> vint;

TEST_CASE("simple-edits-counts") {
  vint a = {1, 2, 3, 4, 5};
  vint b = {1, 2, 8, 4, 5};

  REQUIRE(GetEditDistance(a, b) == 1);
  // edit distance should be symetric
  REQUIRE(GetEditDistance(b, a) == 1);

  // distance of oneself with oneself should be 0
  REQUIRE(GetEditDistance(a, a) == 0);
  REQUIRE(GetEditDistance(b, b) == 0);
}

TEST_CASE("boundaries-edits-counts") {
  vint a = {1, 2, 3, 4, 5};
  vint b = {};

  REQUIRE(GetEditDistance(a, b) == 5);
  // edit distance should be symetric
  REQUIRE(GetEditDistance(b, a) == 5);

  // distance of oneself with oneself should be 0
  REQUIRE(GetEditDistance(a, a) == 0);
  REQUIRE(GetEditDistance(b, b) == 0);
}

TEST_CASE("just-one-target-edits-counts") {
  vint a = {1, 2, 3, 4, 5};
  vint b1 = {1};
  vint b2 = {2};
  vint b3 = {3};
  vint b4 = {4};
  vint b5 = {5};

  REQUIRE(GetEditDistance(a, b1) == 4);
  REQUIRE(GetEditDistance(a, b2) == 4);
  REQUIRE(GetEditDistance(a, b3) == 4);
  REQUIRE(GetEditDistance(a, b4) == 4);
  REQUIRE(GetEditDistance(a, b5) == 4);
}

TEST_CASE("single-edits-counts") {
  vint a = {1, 2, 3, 4, 5};
  vint b = {1, 1, 2, 3, 4, 5};

  REQUIRE(GetEditDistance(b, a) == 1);
  REQUIRE(GetEditDistance(a, b) == 1);
}

TEST_CASE("left-insert") {
  vint va = {1, 2, 3, 4, 5};
  vint vb = {8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 1, 2, 3, 4, 5};
  vint mapA;
  vint mapB;

  REQUIRE(GetEditDistance(va, mapA, vb, mapB) == 10);

  vint vb1 = {8, 8, 8, 8, 8, 1, 8, 8, 8, 8, 1, 2, 3, 4, 5};
  REQUIRE(GetEditDistance(va, mapA, vb1, mapB) == 10);
}

TEST_CASE("map-test-A") {
  vint a = {1, 2, 3, 4, 5};
  vint b = {1, 2, 3, 4, 5};
  vint mapA = {-1, -1, -1, -1, -1};
  vint mapB = {-1, -1, -1, -1, -1};

  REQUIRE(GetEditDistance(a, mapA, b, mapB) == 0);
  for (int i = 0; i < 5; i++) {
    REQUIRE(mapA[i] == i);
    REQUIRE(mapB[i] == i);
  }
}

TEST_CASE("map-test-B") {
  vint a = {1, 2, 3, 4, 5};
  vint b = {1, 4, 5};
  vint mapA = {-1, -1, -1, -1, -1};
  vint mapB = {-1, 1, -1};

  REQUIRE(GetEditDistance(a, mapA, b, mapB) == 2);
  REQUIRE(mapA[0] == 0);
  REQUIRE(mapA[1] == -1);
  REQUIRE(mapA[2] == -1);
  REQUIRE(mapA[3] == 1);
  REQUIRE(mapA[4] == 2);

  REQUIRE(mapB[0] == 0);
  REQUIRE(mapB[1] == 3);
  REQUIRE(mapB[2] == 4);
}

TEST_CASE("map-test-C") {
  vint a = {1, 2, 3, 4, 5};
  vint b = {10, 20, 30, 40, 50};
  vint mapA = {-1, -1, -1, -1, -1};
  vint mapB = {-1, -1, -1, -1, -1};

  REQUIRE(GetEditDistance(a, mapA, b, mapB) == 5);
  for (int i = 0; i < 5; i++) {
    REQUIRE(mapA[i] == -1);
    REQUIRE(mapB[i] == -1);
  }
}
