#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <dpu.h>
#include <dpu_log.h>
#include <time.h>

#include "db.h"
#include "fpgrowth.h"
#include "fpgrowth_defs.h"

#define DB_PATH "data.txt"
#define MIN_SUPPORT 3

// #define COPY_TO
// #define BROADCAST
// #define XFER

int main(int argc, char *argv[]) {
    struct dpu_set_t dpu_set, dpu;
    uint64_t elapsed;
    char config[128];
    int i;
    uint32_t nr_dpus;

    open_db(DB_PATH);
    init_FPTree(MIN_SUPPORT);

    HeaderTable* header_table = headerTable();
    build_tree(MIN_SUPPORT);

    FrequentItemSet* frequent_itemsets = frequentItemSet();
    Vector* prefix_path = vector();

    // Init DPUs
    DPU_ASSERT(dpu_alloc(NR_DPUS, config, &dpu_set));
    DPU_ASSERT(dpu_load(dpu_set, DPU_COND_FPTREE, NULL));
    DPU_ASSERT(dpu_get_nr_dpus(dpu_set, &nr_dpus));

    DPU_FOREACH(dpu_set, dpu, i) {
        DPU_ASSERT(dpu_copy_to(dpu, "input_data", 0, src_buffer[i], DATA_SIZE));
    }

    mine_pattern(header_table, prefix_path, frequent_itemsets, MIN_SUPPORT);

    DPU_ASSERT(dpu_free(dpu_set));

    return 0;
}