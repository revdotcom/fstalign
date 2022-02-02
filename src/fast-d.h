/*
 *
 * fast-d.h
 *
 * JP Robichaud (jp@rev.com)
 * (C) 2021
 *
 */

#ifndef __FASTD_H__
#define __FASTD_H__

#include <vector>

// Simply call GetEditDistance with dummy mapA/mapB
int GetEditDistance(std::vector<int> &seqA, std::vector<int> &seqB);

// returns the edit distance, resize mapA and mapB to be the same length as seqA and seqB
// the map vectors will have either -1 or 1 values, 1 indicating that the token in this position
// matched its counterpart in the other sequence vector, -1 otherwise.
// Unfortunately, for now, we'll need in the order of seqA.size()*seqB.size()*sizeof(int) memory because
// we need to get the backtracking info available to construct the map objects
int GetEditDistance(std::vector<int> &seqA, std::vector<int> &mapA, std::vector<int> &seqB, std::vector<int> &mapB);

// returns only the edit distance.
// This is a memory optimized version and is quite fast.
int GetEditDistanceOnly(std::vector<int> &seqA, std::vector<int> &seqB);

// Returns whether map contains long error streaks.
bool MapContainsErrorStreaks(std::vector<int> map, int streak_cutoff);

#endif