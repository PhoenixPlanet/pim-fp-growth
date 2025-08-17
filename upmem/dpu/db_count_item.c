#include <mram.h>
#include <alloc.h>
#include <barrier.h>
#include <sysdef.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "param.h"

#define MAX_BUF_ELEM    (MRAM_AVAILABLE >> 2)   // MRAM_AVAILABLE / sizeof(int32_t)
#define CACHE_ELEM      (BLOCK_SIZE >> 2)       // BLOCK_SIZE / sizeof(int32_t)

BARRIER_INIT(barrier, NR_TASKLETS);

__mram_noinit int32_t buffer[MAX_BUF_ELEM];
__mram_noinit uint32_t histogram_mram[NR_DB_ITEMS * NR_TASKLETS];
__host uint32_t count; // TODO: Consider copy this value before use (tasklet-safe?)

int main() {
    const sysname_t id = me();
    uint32_t rounded_count = (count + 1U) & ~1U; // (count + (count % 2))
    uint32_t elem_stride = NR_TASKLETS * CACHE_ELEM;

    if (id == 0) {
        mem_reset();
        for (uint32_t i = 0; i < NR_DB_ITEMS; i++) {
            mram_write((void *)&(uint32_t){0}, (__mram_ptr void *)&histogram_mram[i], sizeof(uint32_t));
        }
    }
    barrier_wait(&barrier);

    int32_t* cache = (int32_t*) mem_alloc(BLOCK_SIZE);
    uint32_t* local_hist = (uint32_t*) mem_alloc(NR_DB_ITEMS * sizeof(uint32_t));
    
    for (uint32_t i = id * CACHE_ELEM; i < count; i += elem_stride) {
        uint32_t remaining = (rounded_count > i) ? (rounded_count - i) : 0;
        uint32_t take_elems = (remaining > CACHE_ELEM) ? CACHE_ELEM : remaining;
        uint32_t bytes = take_elems << 2;

        mram_read((__mram_ptr void const*) &buffer[i], cache, bytes);
        
        uint32_t valid_elems = (i < count) ? ((count - i < take_elems) ? (count - i) : take_elems) : 0; // min(count - i, take_elems)
        for (uint32_t k = 0; k < valid_elems; k++) {
            local_hist[cache[k]] += 1u;
        }
    }

    mram_write(local_hist, &histogram_mram[NR_DB_ITEMS * id], NR_DB_ITEMS * sizeof(int32_t));
    barrier_wait(&barrier);

    return 0;
}