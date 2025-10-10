#ifndef PARAM_H
#define PARAM_H

// PIM
#ifndef NR_DPUS
#define NR_DPUS 64
#endif
#ifndef NR_TASKLETS
#define NR_TASKLETS 16
#endif

// DPU binary paths - will be set via environment variables
#ifndef DPU_DB_COUNT_ITEM
#define DPU_DB_COUNT_ITEM (getenv("DPU_DB_COUNT_ITEM_PATH") ? getenv("DPU_DB_COUNT_ITEM_PATH") : "db_count_item")
#endif
#ifndef DPU_MINE_CANDIDATES
#define DPU_MINE_CANDIDATES (getenv("DPU_MINE_CANDIDATES_PATH") ? getenv("DPU_MINE_CANDIDATES_PATH") : "mine_candidates")
#endif

#define BLOCK_SIZE (1024)
#define MRAM_MAX (64ull << 20)
#define MRAM_TRX_ARRAY_RESERVED (1ull << 20)
#define MRAM_FP_ARRAY_SZ (16ull << 20)
#define MRAM_FP_ELEPOS_SZ (1ull << 20)

#define ALIGN_DOWN(BYTES, ALIGN) ((BYTES) - ((BYTES) % (ALIGN)))
#define MRAM_TRX_ARRAY_SZ ALIGN_DOWN(MRAM_MAX - MRAM_TRX_ARRAY_RESERVED, 8)

#ifndef NR_DB_ITEMS
#define NR_DB_ITEMS (512) // Should be a power of 2
#endif

#define DPU_CONFIG "backend=simulator"

#define NR_THREADS 4

#endif