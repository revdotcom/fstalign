#define CATCH_CONFIG_MAIN
#include <stdlib.h> /* srand, rand */
#include <time.h>   /* time */
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
  //   vint mapB = {-1, -1, -1, -1, -1};
  vint mapB;

  REQUIRE(GetEditDistance(a, mapA, b, mapB) == 5);
  for (int i = 0; i < 5; i++) {
    REQUIRE(mapA[i] == -1);
    REQUIRE(mapB[i] == -1);
  }
}

TEST_CASE("test-long-seq") {
  srand(time(NULL));
  int ins_rate = 20;  // over 1k, so 2%
  int del_rate = 20;  // over 1k, so 2%
  int sub_rate = 50;  // over 1k, so 5%

  int retries_left = 5;
  int number_of_edits = 0;
  int edit_distance = 0;

  // for stochastic reasons, it is possible that the naive
  // ins + sub + del count gets off a little.  We give ourselves
  // few attempts to validate that this test passes.
  while (retries_left-- > 0) {
    vint a;
    vint b;
    vint mapA;
    vint mapB;

    number_of_edits = 0;
    int num_ins = 0;
    int num_del = 0;
    int num_sub = 0;

    for (int i = 0; i < 1000; i++) {
      int ai = rand() % 32000 + rand() % 32000 + rand() % 32000 + rand() % 32000 + 1;
      a.push_back(ai);

      // if you want to debug the test
      // std::cout << "a[" << i << "] = " << a[i] << std::endl;
      int extra_char = a[i] + rand() % 32000 + 40000;

      int f = rand() % 1000;
      if (f < ins_rate) {
        b.push_back(extra_char);
        b.push_back(a[i]);
        number_of_edits++;
        num_ins++;
      } else if (f < ins_rate + del_rate) {
        // let's skip this one
        number_of_edits++;
        num_del++;
      } else if (f < ins_rate + del_rate + sub_rate) {
        b.push_back(extra_char);
        number_of_edits++;
        num_sub++;
      } else {
        b.push_back(a[i]);
      }
    }

    // if you want to debug the test
    //   for (int j = 0; j < b.size(); j++) {
    //     std::cout << "b[" << j << "] = " << b[j] << std::endl;
    //   }

    std::cout << " We have " << num_ins << " insertions, " << num_del << " deletions and " << num_sub
              << " substitution for a total of " << number_of_edits << " edits" << std::endl;

    edit_distance = GetEditDistance(a, mapA, b, mapB);
    if (edit_distance != number_of_edits) {
      std::cout << "a= " << a[0];
      for (int i = 1; i < a.size(); i++) {
        std::cout << " " << a[i];
      }
      std::cout << std::endl;
      std::cout << "b= " << b[0];
      for (int i = 1; i < b.size(); i++) {
        std::cout << " " << b[i];
      }
      std::cout << std::endl;
      continue;
    } else {
      break;
    }
  }

  REQUIRE(edit_distance == number_of_edits);
}