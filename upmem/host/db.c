#include "db.h"

#include <stdio.h>
#include <string.h>
#include <dpu.h>

#include "dpu_defs.h"
#include "fpgrowth_defs.h"
#include "param.h"

#define MAX_ELEMS (MRAM_AVAILABLE / sizeof(int32_t))

int32_t** alloc_buffers(int n_buffer, size_t buffer_size) {
    int32_t** buffers = (int32_t**) malloc(n_buffer * sizeof(int32_t*));

    for (int i = 0; i < n_buffer; i++) {
        buffers[i] = (int32_t*) malloc(buffer_size);
    }

    return buffers;
}

void free_buffers(int32_t** buffers, int n_buffer) {
    for (int i = 0; i < n_buffer; i++) {
        free(buffers[i]);
    }
    free(buffers);
}

void dpu_count_items(uint32_t* histogram, dpu_set_ptr set, int32_t** buffers, size_t size_per_buffer, uint32_t* counts, uint32_t nr_of_dpus) {
    struct dpu_set_t dpu;
    uint32_t each_dpu;
    size_t sz_result_buf = NR_DB_ITEMS * NR_TASKLETS * sizeof(uint32_t);
    uint32_t** result_buffers = alloc_buffers(nr_of_dpus, sz_result_buf);

    // Populate MRAM
    DPU_FOREACH(*set, dpu, each_dpu) {
        DPU_ASSERT(dpu_prepare_xfer(dpu, buffers[each_dpu]));
    }
    DPU_ASSERT(dpu_push_xfer(*set, DPU_XFER_TO_DPU, "buffer", 0, size_per_buffer, DPU_XFER_DEFAULT));
    
    // Send count value to WRAM var
    DPU_FOREACH(*set, dpu, each_dpu) {
        dpu_prepare_xfer(dpu, counts[each_dpu]);
    }
    DPU_ASSERT(dpu_push_xfer(*set, DPU_XFER_TO_DPU, "count", 0, sizeof(uint32_t), DPU_XFER_DEFAULT));

    // Launch DPU Binary
    DPU_ASSERT(dpu_launch(*set, DPU_SYNCHRONOUS));

    // Retrieve result data
    DPU_FOREACH(*set, dpu, each_dpu) {
        dpu_prepare_xfer(dpu, result_buffers[each_dpu]);
    }
    DPU_ASSERT(dpu_push_xfer(*set, DPU_XFER_FROM_DPU, "histogram_mram", 0, sz_result_buf, DPU_XFER_DEFAULT));

    // Reduce results
    for (int i = 0; i < NR_DB_ITEMS; i++) {
        uint32_t acc = 0;
        for (int d = 0; d < nr_of_dpus; d++) {
            for (int t = 0; t < NR_TASKLETS; t++) {
                acc += result_buffers[d][NR_TASKLETS * t + i];
            }
        }
        histogram[i] += acc;
    }

    free_buffers(result_buffers, nr_of_dpus);
}

FrequentItemSet* scan_for_frequent_items(int min_support) {
    struct dpu_set_t dpu_set;
    uint32_t nr_of_dpus;

    FrequentItemSet* set = frequentItemSet();

    DPU_ASSERT(dpu_alloc(NR_DPUS, NULL, &dpu_set));
    DPU_ASSERT(dpu_load(dpu_set, DPU_DB_COUNT_ITEM, NULL));
    DPU_ASSERT(dpu_get_nr_dpus(dpu_set, &nr_of_dpus));

    char line[1024];
    int32_t** buffers = alloc_buffers(nr_of_dpus, MAX_ELEMS * sizeof(int32_t));
    uint32_t* buffer_item_counts = (uint32_t*) calloc(nr_of_dpus, sizeof(uint32_t));
    uint32_t* histogram = (uint32_t*) calloc(NR_DB_ITEMS, sizeof(uint32_t));

    int buffer_index = 0;
    while(fscanf(stream, "%[^\n]\n", line) == 1) {
        char* token = strtok(line, " ");
        while(token != NULL) {
            if (buffer_item_counts[buffer_index] >= MAX_ELEMS) {
                dpu_count_items(
                    histogram,
                    &dpu_set, 
                    buffers, 
                    buffer_item_counts[0] * sizeof(int32_t), 
                    buffer_item_counts, 
                    nr_of_dpus
                );

                buffer_index = 0;
                memset(buffer_item_counts, 0, nr_of_dpus * sizeof(uint32_t));
            }

            int32_t item = (int32_t) atoi(token);
            int buffer_index = (buffer_index + 1) % nr_of_dpus;
            buffers[buffer_index][buffer_item_counts[buffer_index]++]; // fill buffer

            token = strtok(NULL, " ");
        }
    }

    for (int i = 0; i < NR_DB_ITEMS; i++) {
        if (histogram[i] >= min_support) {
            FrequentItem* fi = frequentItem(i, histogram[i]);
            INSERT(set, fi);
        }
    }

    return set;
}

