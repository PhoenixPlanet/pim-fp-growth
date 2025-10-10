#ifndef MINE_CANDIDATES_CPU_H
#define MINE_CANDIDATES_CPU_H

#include <vector>
#include "param.h"
#include "common.h"

void mine_candidates_worker(int thread_id, int num_threads, 
                           const std::vector<ElePosEntry>& ele_pos,
                           const std::vector<FPArrayEntry>& fp_array,
                           std::vector<CandidateEntry>& candidates);

#endif
