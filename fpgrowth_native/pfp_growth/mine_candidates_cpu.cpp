#include <vector>
#include <thread>
#include <cstdint>
#include <mutex>
#include <iostream>
#include "include/param.h"
#include "include/common.h"

// Threaded candidate mining for CPU
// CPU version of DPU mine_candidates.c
void cpu_mine_candidates(const std::vector<ElePosEntry>& ele_pos,
                        const std::vector<FPArrayEntry>& fp_array,
                        std::vector<CandidateEntry>& candidates,
                        int num_threads) {
    uint32_t k_elepos_size = ele_pos.size();
    candidates.resize(k_elepos_size * 8); // Over-allocate for safety
    std::vector<std::vector<CandidateEntry>> local_candidates(num_threads);

    auto worker = [&](int tid) {
        for (uint32_t i = tid; i < k_elepos_size; i += num_threads) {
            const ElePosEntry& entry = ele_pos[i];
            if (entry.item == 0) continue;
            
            #ifdef DEBUG
            if (tid == 0) {
                std::cout << "Processing item " << entry.item << " at pos " << entry.pos << std::endl;
            }
            #endif
            
            // Find ALL occurrences of this item in the FP-Array, not just the first one
            for (uint32_t pos = 0; pos < fp_array.size(); ++pos) {
                if (fp_array[pos].item != entry.item) continue;
                
                FPArrayEntry fp_item = fp_array[pos];
                uint32_t suffix_idx = fp_item.parent_pos;
                if (suffix_idx >= fp_array.size()) continue;
                
                fp_item = fp_array[suffix_idx];
                int candidate_idx = 0;
                while (fp_item.item != 0 && suffix_idx < fp_array.size()) {
                    CandidateEntry candidate;
                    candidate.prefix_item = entry.item;
                    candidate.suffix_item = fp_item.item;
                    candidate.suffix_item_pos = suffix_idx;
                    candidate.support = fp_array[pos].support; // Use the support from current node
                    local_candidates[tid].push_back(candidate);
                    
                    #ifdef DEBUG
                    if (tid == 0) {
                        std::cout << "  Found candidate: " << candidate.prefix_item 
                                  << " -> " << candidate.suffix_item 
                                  << " (support: " << candidate.support << ")" << std::endl;
                    }
                    #endif
                    
                    suffix_idx = fp_item.parent_pos;
                    if (suffix_idx >= fp_array.size()) break;
                    fp_item = fp_array[suffix_idx];
                }
            }
        }
    };
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back(worker, t);
    }
    for (auto& th : threads) th.join();
    // Flatten local candidates
    candidates.clear();
    for (int t = 0; t < num_threads; ++t) {
        candidates.insert(candidates.end(), local_candidates[t].begin(), local_candidates[t].end());
    }
}
