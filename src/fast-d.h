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

int GetEditDistance(std::vector<int> &seqA, std::vector<int> &seqB);
int GetEditDistance(std::vector<int> &seqA, std::vector<int> &mapA, std::vector<int> &seqB, std::vector<int> &mapB);
#endif