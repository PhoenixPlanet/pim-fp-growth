#include <mram.h>
#include <stdint.h>
#include <sysdef.h>
#include <stddef.h>
#include <defs.h>

#include "param.h"
#include "common.h"

__host uint32_t k_elepos_size;

typedef struct ElePosEntry ElePosEntry_t;
typedef struct FPArrayEntry FPArrayEntry_t;
typedef struct CandidateEntry CandidateEntry_t;

static inline void get_fp_array_item(uint32_t idx, FPArrayEntry_t* item) {
    mram_read((__mram_ptr void const*) (DPU_MRAM_HEAP_POINTER + idx * sizeof(FPArrayEntry_t)), item, sizeof(FPArrayEntry_t));
}

static inline void get_k_elepos_item(uint32_t idx, ElePosEntry_t* item) {
    mram_read((__mram_ptr void const*) (DPU_MRAM_HEAP_POINTER + MRAM_FP_ARRAY_SZ + idx * sizeof(ElePosEntry_t)), item, sizeof(ElePosEntry_t));
}

static inline void set_candidate_item(uint32_t idx, const CandidateEntry_t* item) {
    mram_write(item, (__mram_ptr void*) (DPU_MRAM_HEAP_POINTER + MRAM_FP_ARRAY_SZ + MRAM_FP_ELEPOS_SZ + idx * sizeof(CandidateEntry_t)), sizeof(CandidateEntry_t));
}

int main() {
    const sysname_t id = me();
    
    for (int i = id; i < k_elepos_size; i += NR_TASKLETS) {
        CandidateEntry_t candidate;
        ElePosEntry_t entry;
        FPArrayEntry_t fp_item;

        get_k_elepos_item(i, &entry);
        if (entry.item == 0) {
            continue; // Skip if item is 0 (root)
        }
        get_fp_array_item(entry.pos, &fp_item); // Get the corresponding FPArrayEntry
        uint32_t suffix_idx = fp_item.parent_pos;
        get_fp_array_item(fp_item.parent_pos, &fp_item); // Get the parent FPArrayEntry
        
        int candidate_idx = 0;
        while (fp_item.item != 0) {
            candidate.prefix_item = entry.item;
            candidate.suffix_item = fp_item.item;
            candidate.suffix_item_pos = suffix_idx;
            candidate.support = entry.support;

            set_candidate_item(entry.candidate_start_idx + candidate_idx++, &candidate);

            suffix_idx = fp_item.parent_pos;
            get_fp_array_item(fp_item.parent_pos, &fp_item); // Get the next parent
        }
    }
}