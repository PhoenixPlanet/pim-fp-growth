#ifndef PARAM_H
#define PARAM_H

// PIM
#ifndef NR_DPUS
#define NR_DPUS 4
#endif
#ifndef NR_TASKLETS
#define NR_TASKLETS 16
#endif

#define DPU_DB_COUNT_ITEM "db_count_item"
#define DPU_DB_FILTER_ITEM "db_filter_item"
#define DPU_DB_FILTER "db_filter"
#define DPU_COND_FPTREE "cond_fptree"

#define BLOCK_SIZE 1024
#define MRAM_MAX 64ull << 20
#define MRAM_RESERVED 1ull  << 20

#define ALIGN_DOWN(BYTES, ALIGN) ((BYTES) - ((BYTES) % (ALIGN)))
#define MRAM_AVAILABLE ALIGN_DOWN(MRAM_MAX - MRAM_RESERVED, 8)

#ifndef NR_DB_ITEMS
#define NR_DB_ITEMS 1024
#endif

#define NR_THREADS 4

#endif