/*
fast-d.cpp
 JP Robichaud (jp@rev.com)
 2021
*/
#include "fast-d.h"
#include <algorithm>  // std::min
#include <iomanip>
#include <iostream>
#include <utility>

#define debug_map false

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

void print_vect(std::string lbl, int *v, int l) {
  std::cout << lbl;
  for (int x = 0; x < l; x++) {
    std::cout << std::setw(2);
    std::cout << v[x] << " ";
  }
  std::cout << std::endl;
}

/* This is memory hungry.  We need seqA.size()  * seqB.size() * sizeof(int) because we need to be able to do the
 * backtracking.
 */
int GetEditDistance(std::vector<int> &seqA, std::vector<int> &mapA, std::vector<int> &seqB, std::vector<int> &mapB) {
  int lengthA = seqA.size();
  int lengthB = seqB.size();

  if (lengthA > lengthB) {
    // make sure seqA is always the shortest
    return GetEditDistance(seqB, mapB, seqA, mapA);
  }

  mapA.reserve(seqA.size());
  mapA.resize(seqA.size(), -1);  // resize() sets all position to the given value, -1
  mapB.reserve(seqB.size());
  mapB.resize(seqB.size(), -1);

  if (seqA.size() == 0) {
    return seqB.size();
  } else if (seqB.size() == 0) {
    return seqA.size();
  }

  // let's keep two rows, dig all the way to the end to get the distance, then
  // let's try to backtrack and get the edits from there, recomputing the rows again

  int distance[lengthA + 1];
  int distancePrev[lengthA + 1];
  for (int i = 0; i <= lengthA; ++i) {
    distance[i] = i;
  }

  // TODO: we should maybe optimize this to be a 2D uint array?
  std::vector<std::vector<int>> all_distances;

#if debug_map
  print_vect(std::string("seqA: "), seqA.data(), lengthA);
  print_vect(std::string("seqB: "), seqB.data(), lengthB);
#endif
  for (int j = 1; j <= lengthB; ++j) {
    all_distances.push_back(std::vector<int>(distance, distance + lengthA + 1));
    // for (int x = 0; x <= lengthA; ++x) {
    //   distancePrev[x] = distance[x];
    // }

    std::copy(distance, distance + lengthA + 1, distancePrev);

#if debug_map
    print_vect(std::string("d: "), distance, lengthA + 1);
#endif

    int prev_diag = distance[0], prev_diag_save;
    ++distance[0];

    for (int i = 1; i <= lengthA; ++i) {
      prev_diag_save = distance[i];
      if (seqA[i - 1] == seqB[j - 1]) {
        distance[i] = prev_diag;
      } else {
        distance[i] = min(distance[i - 1], distance[i], prev_diag) + 1;
      }
      prev_diag = prev_diag_save;
    }
  }
  all_distances.push_back(std::vector<int>(distance, distance + lengthA + 1));
#if debug_map
  print_vect(std::string("d: "), distance, lengthA + 1);
#endif

  int edit_distance = distance[lengthA];

  // now, we want to backtrack the computation and trace, row, by row,
  // the path

  int current_pos = lengthA;
  int seqB_track = lengthB;

  while (current_pos > 0 && seqB_track >= 0) {
    int current_pos_score = distance[current_pos];
#if debug_map
    std::cout << "starting iter" << std::endl;
    std::cout << "current_pos = " << current_pos << std::endl;
    std::cout << "current_pos_score = " << current_pos_score << std::endl;
    std::cout << "seqB_track  = " << seqB_track << std::endl;
    print_vect(std::string("mapA:"), mapA.data(), lengthA);
    print_vect(std::string("mapB:"), mapB.data(), lengthB);
    print_vect(std::string("dP: "), distancePrev, lengthA + 1);
    print_vect(std::string("d : "), distance, lengthA + 1);
#endif

    int token_a = seqA[current_pos - 1];
    int token_b = seqB[seqB_track - 1];

    int nw = distancePrev[current_pos - 1];
    int n = distancePrev[current_pos];
    int w = distance[current_pos - 1];

    bool is_sub = false;
    bool is_match = false;
    bool is_del = false;
    bool is_ins = false;

#if debug_map
    std::cout << "checking " << token_a << " vs " << token_b << std::endl;
    std::cout << "nw = " << nw << std::endl;
    std::cout << "n  = " << n << std::endl;
    std::cout << "w  = " << w << std::endl;
#endif

    int min_path_score = min(nw, n, w);
    if (min_path_score == nw) {
      // the upper-left diagonal is the best path
      is_sub = true;
      if (min_path_score == current_pos_score) {
        // we have a caracter match
        mapA[current_pos - 1] = 1;
        mapB[seqB_track - 1] = 1;
        is_match = true;
        is_sub = false;
      }
#if debug_map
      std::cout << "S(" << token_a << "|" << token_b << ")" << std::endl;
#endif
      // going up-left, next time we need to be one position on the left
      // to read the distance
      current_pos--;
      seqB_track--;
    } else if (min_path_score == w) {
      // this is a deletion, going left
      current_pos--;
      is_del = true;
#if debug_map
      std::cout << "D(" << token_a << ")" << std::endl;
#endif
    } else {
      // this is an insertion, going north doesn't
      // change the current_position we read in the distance vector
      seqB_track--;
      is_ins = true;
#if debug_map
      std::cout << "I(" << token_b << ")" << std::endl;
#endif
    }

    if (current_pos < 0 || seqB_track == 0) {
      // We reach to a point where any position left
      // are errors (either insertions or deletions).
      // There's no point in analyzing these values.
      break;
    }

    // ok, we stop reflecting on the best path to take, now we need to update
    // the distance vectors.  Step 1, distancePrev becomes the new distance
    // now, we only have to go up to current_pos, because everything on the
    // right will get ignored.  Actually, we only have to compute the two values
    // above

    for (int x = 0; x <= lengthA; x++) {
      distance[x] = distancePrev[x];
    }

    // Step 2, let's look at the row above the one we are now.
    if (false) {
      token_b = seqB[seqB_track - 1];
      int token_b_prime = seqB[seqB_track - 2];
      for (int x = current_pos; x > 0; --x) {
        token_a = seqA[x - 1];

        if (token_a == token_b) {
          distancePrev[x - 1] = distance[x];
          distancePrev[x] = distance[x] + 1;
        } else {
          distancePrev[x - 1] = distance[x - 1] - 1;
          distancePrev[x] = distance[x] - 1;
        }
      }

    } else {
      auto v = all_distances[seqB_track - 1];
      for (int j = 0; j <= lengthA; ++j) {
        distancePrev[j] = v[j];
      }
    }
  }

  return edit_distance;
}

/* This version doesn't handle a map and uses much less ram
   because it doesn't keep information required for backtracking.
   With this method, you only get the final edit distance.
 */
int GetEditDistanceOnly(std::vector<int> &seqA, std::vector<int> &seqB) {
  int lengthA = seqA.size();
  int lengthB = seqB.size();

  if (lengthA > lengthB) {
    // make sure seqA is always the shortest
    return GetEditDistanceOnly(seqB, seqA);
  }

  if (seqA.size() == 0) {
    return seqB.size();
  } else if (seqB.size() == 0) {
    return seqA.size();
  }

  int distance[lengthA + 1];
  for (int i = 0; i <= lengthA; ++i) {
    distance[i] = i;
  }

  for (int j = 1; j <= lengthB; ++j) {
    int prev_diag = distance[0], prev_diag_save;
    int dist_lenA = distance[lengthA];
    ++distance[0];

    for (int i = 1; i <= lengthA; ++i) {
      prev_diag_save = distance[i];
      if (seqA[i - 1] == seqB[j - 1]) {
        distance[i] = prev_diag;
      } else {
        distance[i] = min(distance[i - 1], distance[i], prev_diag) + 1;
      }
      prev_diag = prev_diag_save;
    }
  }

  return distance[lengthA];
}
