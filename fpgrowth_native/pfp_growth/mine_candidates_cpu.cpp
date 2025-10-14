#include <vector>
#include <thread>
#include <cstdint>
#include "include/param.h"
#include "include/common.h"

//---------------------------------------
// FP Growth structures
typedef struct ElePosEntry elepos_entry_t;
typedef struct FPArrayEntry fp_array_entry_t;
typedef struct CandidateEntry candidate_entry_t;
//---------------------------------------

// Worker function for threaded candidate mining
// Simplified version matching DPU logic
void mine_candidates_worker(int thread_id, int num_threads, 
                           const std::vector<ElePosEntry>& ele_pos,
                           const std::vector<FPArrayEntry>& fp_array,
                           std::vector<CandidateEntry>& candidates) {
    // Process ALL elements in ele_pos (which is already partitioned per-thread)
    // Don't skip by num_threads - that's already done in the partitioning
    for (int i = 0; i < (int)ele_pos.size(); i++) {
        candidate_entry_t candidate;
        elepos_entry_t entry;
        fp_array_entry_t fp_item;
        
        // Read ElePos Entry
        entry = ele_pos[i];
        if (entry.item == 0) {
            continue; // Skip if item is 0 (root)
        }

        // Get the corresponding FPArrayEntry
        if (entry.pos >= fp_array.size()) continue; // Bounds check
        fp_item = fp_array[entry.pos];
        
        uint32_t suffix_idx = fp_item.parent_pos;
        
        // Get the parent FPArrayEntry
        if (fp_item.parent_pos >= fp_array.size()) continue; // Bounds check
        fp_item = fp_array[fp_item.parent_pos];
        
        int candidate_idx = 0;
        while (fp_item.item != 0) {
            // Create candidate itemsets
            candidate.prefix_item = entry.item;
            candidate.suffix_item = fp_item.item;
            candidate.suffix_item_pos = suffix_idx;
            candidate.support = entry.support;

            // Write candidate to output
            uint32_t out_idx = entry.candidate_start_idx + candidate_idx++;
            if (out_idx < candidates.size()) {
                candidates[out_idx] = candidate;
            }

            // Get the next parent
            suffix_idx = fp_item.parent_pos;
            if (fp_item.parent_pos >= fp_array.size()) break; // Bounds check
            fp_item = fp_array[fp_item.parent_pos];
        }
    }
}
