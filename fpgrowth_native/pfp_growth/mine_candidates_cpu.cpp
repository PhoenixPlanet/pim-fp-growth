#include <vector>
#include <thread>
#include <cstdint>
#include <mutex>
#include <iostream>
#include "include/param.h"
#include "include/common.h"

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
cache_set_t* cache_sets;

static std::vector<fp_array_entry_t> fp_array_global;
static std::vector<elepos_entry_t> elepos_global;
static std::vector<candidate_entry_t> candidates_global;
static std::mutex cache_mutex;

cache_set_t* init_cache() {
    cache_set_t* cache = (cache_set_t*)malloc(SETS * sizeof(cache_set_t));
    for (int i = 0; i < SETS; i++) {
        for (int j = 0; j < WAYS; j++) {
            cache[i].blocks[j].valid = 0;
            cache[i].blocks[j].lru_counter = 0;
        }
    }
    return cache;
}

void free_cache(cache_set_t* cache) {
    free(cache);
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
    // Check bounds to prevent segfault
    if (index >= fp_array_global.size()) {
        // Return a default/empty entry or handle error appropriately
        fp_array_entry_t empty_entry = {};
        return empty_entry;
    }
    
    // Initialize cache if not already done
    if (cache_sets == nullptr) {
        cache_sets = init_cache();
    }
    
    uint32_t set_idx = get_set_index(index);
    cache_mutex.lock();

    cache_set_t* set = &cache_sets[set_idx];
    for (int i = 0; i < WAYS; i++) {
        if (set->blocks[i].valid && set->blocks[i].index == index) {
            if (set->blocks[i].lru_counter == UINT16_MAX) {
                normalize_lru_counters(set, i);
            } else {
                set->blocks[i].lru_counter++;
            }

            fp_array_entry_t result = set->blocks[i].data;
            cache_mutex.unlock();

            return result;
        }
    }

    int victim_index = select_victim(set);
    cache_block_t* victim = &set->blocks[victim_index];
    victim->data = fp_array_global[index];
    victim->index = index;
    victim->valid = 1;
    normalize_lru_counters(set, victim_index);

    fp_array_entry_t result = victim->data;
    cache_mutex.unlock();

    return result;
}
//---------------------------------------

static inline void get_fp_array_item(uint32_t idx, fp_array_entry_t* item) {
    // mutex_lock(mutex);
    // mram_read((__mram_ptr void const*) (DPU_MRAM_HEAP_POINTER + idx * sizeof(fp_array_entry_t)), item, sizeof(fp_array_entry_t));
    // mutex_unlock(mutex);
    *item = cache_fp_array_entry_read(idx);
}

static inline void get_k_elepos_item(uint32_t idx, elepos_entry_t* item) {
    *item = elepos_global[idx];
}

static inline void set_candidate_item(uint32_t idx, const candidate_entry_t* item) {
    candidates_global[idx] = *item;
}

// Worker function for threaded candidate mining
void mine_candidates_worker(int thread_id, int num_threads, 
                           const std::vector<ElePosEntry>& ele_pos,
                           const std::vector<FPArrayEntry>& fp_array,
                           std::vector<CandidateEntry>& candidates) {
    // Initialize global vectors if not already done
    if (fp_array_global.empty()) {
        fp_array_global = fp_array;
    }
    if (elepos_global.empty()) {
        elepos_global = ele_pos;
    }
    if (candidates_global.empty()) {
        candidates_global.resize(candidates.size());
    }
    // Each thread processes every num_threads-th element
    for (int i = thread_id; i < (int)ele_pos.size(); i += num_threads) {
        candidate_entry_t candidate;
        elepos_entry_t entry;
        fp_array_entry_t fp_item;
        
        // Read ElePos Entry
        get_k_elepos_item(i, &entry);
        if (entry.item == 0) {
            continue; // Skip if item is 0 (root)
        }

        // Get the corresponding FPArrayEntry
        get_fp_array_item(entry.pos, &fp_item);
        uint32_t suffix_idx = fp_item.parent_pos;
        get_fp_array_item(fp_item.parent_pos, &fp_item); // Get the parent FPArrayEntry
        int candidate_idx = 0;
        while (fp_item.item != 0) {
            // Create candidate itemsets
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
