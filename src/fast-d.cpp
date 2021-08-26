/*
fast-d.cpp
 JP Robichaud (jp@rev.com)
 2021
*/
#include "fast-d.h"
#include <algorithm>  // std::min
#include <iostream>

int min(int &a, int &b, int &c) {
  if (a < b) {
    if (a < c) {
      return a;
    } else {
      return c;
    }
  } else if (b < c) {
    return b;
  } else {
    return c;
  }
}

int GetEditDistance(std::vector<int> &seqA, std::vector<int> &seqB) {
  std::vector<int> mapA;
  std::vector<int> mapB;

  return GetEditDistance(seqA, mapA, seqB, mapB);
}

int GetEditDistance(std::vector<int> &seqA, std::vector<int> &mapA, std::vector<int> &seqB, std::vector<int> &mapB) {
  int lengthA = seqA.size();
  int lengthB = seqB.size();

  if (lengthA > lengthB) {
    // make sure seqA is always the shortest
    return GetEditDistance(seqB, mapB, seqA, mapA);
  }

  mapA.reserve(seqA.size());
  mapA.resize(seqA.size(), -1);
  mapB.reserve(seqB.size());
  mapB.resize(seqB.size(), -1);

  if (seqA.size() == 0) {
    mapA.resize(0);
    return seqB.size();
  } else if (seqB.size() == 0) {
    mapB.resize(0);
    for (int i = 0; i < seqA.size(); i++) {
      mapA[i] = -1;
    }
    return seqA.size();
  }

  int distance[lengthA + 1];
  for (int i = 0; i <= lengthA; ++i) {
    distance[i] = i;
  }
  for (int j = 1; j <= lengthB; ++j) {
    int prev_diag = distance[0], prev_diag_save;
    ++distance[0];

    for (int i = 1; i <= lengthA; ++i) {
      prev_diag_save = distance[i];
      if (seqA[i - 1] == seqB[j - 1]) {
        distance[i] = prev_diag;
        mapA[i - 1] = j - 1;
        mapB[j - 1] = i - 1;
      } else {
        distance[i] = min(distance[i - 1], distance[i], prev_diag) + 1;
      }
      prev_diag = prev_diag_save;
    }
  }

  return distance[lengthA];

  return 0;
}
