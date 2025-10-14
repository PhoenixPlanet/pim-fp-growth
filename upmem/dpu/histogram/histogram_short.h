#ifndef HISTOGRAM_SHORT_H
#define HISTOGRAM_SHORT_H

#include <mram.h>
#include <alloc.h>
#include <barrier.h>
#include <sysdef.h>
#include <defs.h>
#include <param.h>

#define CACHE_ELEM  (BLOCK_SIZE >> 2)
#define BUFFER_ADDR  ((uint32_t) DPU_MRAM_HEAP_POINTER)

BARRIER_INIT(barrier, NR_TASKLETS);

uint32_t* histograms[NR_TASKLETS];

// This function should be used when NR_DB_ITEMS is small (e.g., <= 512)
static void histogram_short(uint32_t count) {
    const sysname_t id = me();
    uint32_t elem_stride = NR_TASKLETS * CACHE_ELEM;

    if (id == 0) {
        mem_reset();
    }

    barrier_wait(&barrier);

    int32_t* cache = (int32_t*) mem_alloc(BLOCK_SIZE);
    uint32_t* local_hist = (uint32_t*) mem_alloc(NR_DB_ITEMS * sizeof(uint32_t));
    for (int i = 0; i < NR_DB_ITEMS; i++) {
        local_hist[i] = 0;
    }
    histograms[id] = local_hist;
    
    for (uint32_t i = id * CACHE_ELEM; i < count; i += elem_stride) {
        uint32_t remaining = count - i;
        uint32_t take_elems = (remaining > CACHE_ELEM) ? CACHE_ELEM : remaining;
        uint32_t bytes = ((take_elems + 1U) & ~1U) << 2;

        mram_read((__mram_ptr void const*) (BUFFER_ADDR + (i * sizeof(int32_t))), cache, bytes);
        for (uint32_t k = 0; k < take_elems; k++) {
            local_hist[cache[k]] += 1u;
        }
    }

    barrier_wait(&barrier);

    for (uint32_t i = id; i < NR_DB_ITEMS; i += NR_TASKLETS) {
        uint32_t acc = 0;
        for (uint32_t t = 0; t < NR_TASKLETS; t++) {
            acc += histograms[t][i];
        }
        histograms[0][i] = acc;
    }

    barrier_wait(&barrier);

    if (id == 0) {
        mram_write(histograms[0], (__mram_ptr void*) (DPU_MRAM_HEAP_POINTER + MRAM_TRX_ARRAY_SZ), NR_DB_ITEMS * sizeof(int32_t));
    }
}

#endif