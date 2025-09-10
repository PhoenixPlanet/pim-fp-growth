#ifndef MINE_CANDIDATES_CPU_H
#define MINE_CANDIDATES_CPU_H

#include <vector>
#include "param.h"
#include "common.h"

void cpu_mine_candidates(const std::vector<ElePosEntry>& ele_pos,
                        const std::vector<FPArrayEntry>& fp_array,
                        std::vector<CandidateEntry>& candidates,
                        int num_threads);

#endif
