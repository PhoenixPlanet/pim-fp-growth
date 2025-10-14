#ifndef HISTOGRAM_LONG_H
#define HISTOGRAM_LONG_H

#include <mram.h>
#include <alloc.h>
#include <barrier.h>
#include <atomic_bit.h>
#include <mutex.h>
#include <mutex_pool.h>
#include <sysdef.h>
#include <defs.h>
#include <param.h>

#define CACHE_ELEM  (BLOCK_SIZE >> 2)
#define BUFFER_ADDR ((uint32_t) DPU_MRAM_HEAP_POINTER)
#define CHUNK_SIZE  (2048)
#define NR_HISTO    (4)

BARRIER_INIT(global_barrier, NR_TASKLETS);
ATOMIC_BIT_INIT(barriers_bits)[NR_HISTO];
MUTEX_POOL_INIT(local_mutexes, NR_HISTO);
barrier_t local_barriers[NR_HISTO];

uint32_t* histograms[NR_HISTO];

static void histogram_long(uint32_t count) {
    const sysname_t id = me();
    const sysname_t local_id = id / NR_HISTO;
    uint32_t nr_tasklet_per_histo = NR_TASKLETS / NR_HISTO;
    uint32_t histo_id = id & (NR_HISTO - 1);
    uint32_t elem_stride = NR_TASKLETS * CACHE_ELEM;

    if (id == 0) {
        mem_reset();
        for (uint32_t i = 0; i < NR_HISTO; i++) {
            local_barriers[i].wait_queue = 0xff;
            local_barriers[i].count = nr_tasklet_per_histo;
            local_barriers[i].initial_count = nr_tasklet_per_histo;
            local_barriers[i].lock = (uint8_t) &ATOMIC_BIT_GET(barriers_bits)[i];
        }
    }

    barrier_wait(&global_barrier);

    int32_t* cache = (int32_t*) mem_alloc(BLOCK_SIZE);
    if (local_id == 0) {
        histograms[histo_id] = (uint32_t*) mem_alloc(NR_DB_ITEMS * sizeof(uint32_t));
    }

    barrier_wait(&local_barriers[histo_id]);

    uint32_t* local_hist = histograms[histo_id];
    for (int i = local_id; i < NR_DB_ITEMS; i += nr_tasklet_per_histo) {
        local_hist[i] = 0;
    }

    barrier_wait(&local_barriers[histo_id]);

    for (uint32_t i = id * CACHE_ELEM; i < count; i += elem_stride) {
        uint32_t remaining = count - i;
        uint32_t take_elems = (remaining > CACHE_ELEM) ? CACHE_ELEM : remaining;
        uint32_t bytes = ((take_elems + 1U) & ~1U) << 2;

        mram_read((__mram_ptr void const*) (BUFFER_ADDR + (i * sizeof(int32_t))), cache, bytes);
        for (uint32_t k = 0; k < take_elems; k++) {
            mutex_pool_lock(&local_mutexes, histo_id);
            local_hist[cache[k]] += 1u;
            mutex_pool_unlock(&local_mutexes, histo_id);
        }
    }

    barrier_wait(&global_barrier);

    for (uint32_t i = id; i < NR_DB_ITEMS; i += NR_TASKLETS) {
        uint32_t acc = 0;
        for (uint32_t t = 0; t < NR_HISTO; t++) {
            acc += histograms[t][i];
        }
        histograms[0][i] = acc;
    }

    barrier_wait(&global_barrier);

    uint32_t nr_chunks = (NR_DB_ITEMS * sizeof(uint32_t)) / CHUNK_SIZE;
    for (uint32_t chunk = id; chunk < nr_chunks; chunk += NR_TASKLETS) {
        uint32_t offset = chunk * CHUNK_SIZE;
        mram_write(
            (uint8_t*)histograms[0] + offset,
            (__mram_ptr void*) (DPU_MRAM_HEAP_POINTER + MRAM_TRX_ARRAY_SZ + offset),
            CHUNK_SIZE
        );
    }
}

#endif