#include <mram.h>
#include <stdint.h>
#include <sysdef.h>
#include <stddef.h>
#include <defs.h>
#include <barrier.h>
#include <alloc.h>
#include <vmutex.h>
#include <mutex.h>

#include "param.h"
#include "common.h"

__host uint32_t k_elepos_size;

//---------------------------------------
// FP Growth structures
typedef struct ElePosEntry elepos_entry_t;
typedef struct FPArrayEntry fp_array_entry_t;
typedef struct CandidateEntry candidate_entry_t;
//---------------------------------------

//---------------------------------------
// Cache Implementation
#define WAYS (2)
#define SET_BITS (10) // TODO: Test with different set sizes and associativity
#define SETS (1 << SET_BITS)

typedef struct {
    uint32_t index;
    uint16_t valid;
    uint16_t lru_counter;
    fp_array_entry_t data;
} cache_block_t;

typedef struct {
    cache_block_t blocks[WAYS];
} cache_set_t;

VMUTEX_INIT(cache_vmutex, SETS, 32);
MUTEX_INIT(mutex);
cache_set_t* cache_sets;

cache_set_t* init_cache() {
    cache_set_t* cache = (cache_set_t*) mem_alloc(SETS * sizeof(cache_set_t));
    for (int i = 0; i < SETS; i++) {
        for (int j = 0; j < WAYS; j++) {
            cache[i].blocks[j].valid = 0;
            cache[i].blocks[j].lru_counter = 0;
        }
    }

    return cache;
}

static inline uint32_t get_set_index(uint32_t index) {
    return index & (SETS - 1); // index % SETS
}

static inline int select_victim(cache_set_t* set) {
    int victim_index = 0;
    uint16_t min_lru = UINT16_MAX;

    for (int i = 0; i < WAYS; i++) {
        if (set->blocks[i].valid == 0) {
            return i;
        }

        if (set->blocks[i].lru_counter < min_lru) {
            min_lru = set->blocks[i].lru_counter;
            victim_index = i;
        }
    }

    return victim_index;
}

static inline void normalize_lru_counters(cache_set_t* set, uint32_t recent_index) {
    uint16_t max_lru = 0;

    for (int i = 0; i < WAYS; i++) {
        if (i == recent_index) continue;

        if (set->blocks[i].valid) {
            if (set->blocks[i].lru_counter > max_lru) {
                max_lru = set->blocks[i].lru_counter;
            }
            set->blocks[i].lru_counter >>= 2;
        }
    }

    set->blocks[recent_index].lru_counter = (max_lru >> 2) + 1;
}

fp_array_entry_t cache_fp_array_entry_read(uint32_t index) {
    uint32_t set_idx = get_set_index(index);

    vmutex_lock(&cache_vmutex, set_idx);

    cache_set_t* set = &cache_sets[set_idx];
    for (int i = 0; i < WAYS; i++) {
        if (set->blocks[i].valid && set->blocks[i].index == index) {
            if (set->blocks[i].lru_counter == UINT16_MAX) {
                normalize_lru_counters(set, i);
            } else {
                set->blocks[i].lru_counter++;
            }

            fp_array_entry_t result = set->blocks[i].data;
            vmutex_unlock(&cache_vmutex, set_idx);

            return result;
        }
    }

    int victim_index = select_victim(set);
    cache_block_t* victim = &set->blocks[victim_index];

    mram_read((__mram_ptr void const*) (DPU_MRAM_HEAP_POINTER + index * sizeof(fp_array_entry_t)), &victim->data, sizeof(fp_array_entry_t));
    victim->index = index;
    victim->valid = 1;
    normalize_lru_counters(set, victim_index);

    fp_array_entry_t result = victim->data;
    vmutex_unlock(&cache_vmutex, set_idx);

    return result;
}
//---------------------------------------

BARRIER_INIT(barrier, NR_TASKLETS);

static inline void get_fp_array_item(uint32_t idx, fp_array_entry_t* item) {
    // mutex_lock(mutex);
    // mram_read((__mram_ptr void const*) (DPU_MRAM_HEAP_POINTER + idx * sizeof(fp_array_entry_t)), item, sizeof(fp_array_entry_t));
    // mutex_unlock(mutex);
    *item = cache_fp_array_entry_read(idx);
}

static inline void get_k_elepos_item(uint32_t idx, elepos_entry_t* item) {
    mram_read((__mram_ptr void const*) (DPU_MRAM_HEAP_POINTER + MRAM_FP_ARRAY_SZ + idx * sizeof(elepos_entry_t)), item, sizeof(elepos_entry_t));
}

static inline void set_candidate_item(uint32_t idx, const candidate_entry_t* item) {
    mram_write(item, (__mram_ptr void*) (DPU_MRAM_HEAP_POINTER + MRAM_FP_ARRAY_SZ + MRAM_FP_ELEPOS_SZ + idx * sizeof(candidate_entry_t)), sizeof(candidate_entry_t));
}

int main() {
    const sysname_t id = me();

    if (id == 0) {
        mem_reset();
        cache_sets = init_cache();
    }
    barrier_wait(&barrier);
    
    for (int i = id; i < k_elepos_size; i += NR_TASKLETS) {
        candidate_entry_t candidate;
        elepos_entry_t entry;
        fp_array_entry_t fp_item;

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