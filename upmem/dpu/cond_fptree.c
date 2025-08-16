#include <mram.h>
#include <alloc.h>
#include <barrier.h>
#include <defs.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <perfcounter.h>

#include "fpgrowth_defs.h"
#include "fpgrowth.h"

void build_conditional_tree(PatternBaseList* pattern_base, int min_support) {

    ItemCountList *item_count = itemCountList();

    for (PatternBase* current = pattern_base->head; current != NULL; current = current->next) {
        if (current->transaction->size == 0) continue; // Skip empty transactions
        for (Scalar* item = current->transaction->head; item != NULL; item = item->next) {
            incr_item_count(item_count, item->value, current->count);
        }
    }

    HeaderTable* header_table = headerTable();

    for (ItemCount* ic = item_count->head; ic != NULL; ic = ic->next) {
        if (ic->count >= min_support) {
            HeaderTableEntry* entry = headerTableEntry(ic->item_id, ic->count);
            SORTED_INSERT(HeaderTableEntry, header_table, item_id, entry);
        }
    }

    Vector* filtered_items = vector();
    for (PatternBase* current = pattern_base->head; current != NULL; current = current->next) {
        if (current->transaction == NULL) continue; // Skip empty transactions

        Vector* transaction = current->transaction;
        int count = current->count;

        for (Scalar* item = transaction->head; item != NULL; item = item->next) {
            if (find_item_count(item_count, item->value) >= min_support) {
                Scalar* new_scalar = scalar(item->value);
                INSERT(filtered_items, new_scalar);
            }
        }
    

        Node* current_node = root;

        for (Scalar* entry = filtered_items->head; entry != NULL; entry = entry->next) {
            int item = entry->value;
            // Check if the item already exists in the current node's children
            int found = 1;
            for (Node* child = current_node->first_child; child != NULL; child = child->next_sibling) {
                if (child->item.item_id == item) {
                    child->item.count += count;
                    current_node = child;
                    found = 0;
                    break;
                }
            }
            if (!found) {
                Node* new_node = node(item, count);
                new_node->parent = current_node;

                if (current_node->first_child == NULL) {
                    current_node->first_child = new_node;
                } else {
                    Node* sibling = current_node->first_child;
                    while (sibling->next_sibling != NULL) {
                        sibling = sibling->next_sibling;
                    }
                    sibling->next_sibling = new_node;
                }

                // Add to header table
                HeaderTableEntry* htb_entry = find_header_table_entry(header_table, item);
                if (htb_entry != NULL) {
                    INSERT(htb_entry->node_link, nodeLink(new_node));
                    htb_entry->frequency += count;
                    current_node = new_node;
                } else {
                    fprintf(stderr, "Error: Item not found in header table: %d\n", item);
                }
            }
        }
    }
}

void mine_pattern(HeaderTable* header_table, Vector* prefix_path,
    FrequentItemSet* frequent_itemsets, int min_support) {
    for (HeaderTableEntry* entry = header_table->head; entry != NULL; entry = entry->next) {
        int item_id = entry->item_id;

        // Create a new prefix path
        Vector* new_prefix_path = vector();
        for (Scalar* current = prefix_path->head; current != NULL; current = current->next) {
            Scalar* s = scalar(current->value);
            INSERT(new_prefix_path, s);
        }
        INSERT(new_prefix_path, scalar(item_id));

        // Add the new prefix path to the frequent itemsets
        FrequentItem* new_item = frequentItem(item_id, entry->frequency);
        INSERT(frequent_itemsets, new_item);

        // Build conditional pattern base
        PatternBaseList* pattern_base = patternBaseList();
        for (Node** node_link = entry->node_link; node_link != NULL; node_link++) {
            Node* node = *node_link;
            Vector* transaction = vector();
            for (Scalar* s = new_prefix_path->head; s != NULL; s = s->next) {
                Scalar* sca = scalar(s->value);
                INSERT(transaction, sca);
            }
            PatternBase* base = patternBase(transaction, node->item.count);
            base->next = NULL;
            INSERT(pattern_base, base);
        }

        // Recursively mine patterns from the conditional pattern base
        build_conditional_tree(pattern_base, min_support);
    }
}



#ifndef DATA_SIZE
#define DATA_SIZE 1024
#endif

#ifndef WRAM_SIZE
#define WRAM_SIZE 1024
#endif

#define data_t int32_t

BARRIER_INIT(barrier, NR_TASKLETS);

__host uint32_t sum_arr[NR_TASKLETS];
__host uint64_t elapsed_arr[NR_TASKLETS];

int main() {
    const sysname_t id = me();

    uint32_t total_bytes = DATA_SIZE * NR_TASKLETS;
    uint32_t buffer_size_per_dpu = DATA_SIZE;
    uint32_t data_count_per_dpu = buffer_size_per_dpu / sizeof(data_t);
    uint32_t data_count_per_tasklet = WRAM_SIZE / sizeof(data_t);
    uint32_t wram_indices_size = data_count_per_tasklet * sizeof(uint32_t);

    uint32_t indices_address = (uint32_t)(DPU_MRAM_HEAP_POINTER) + (id * wram_indices_size);
    uint32_t from_buffer_address = (uint32_t)(DPU_MRAM_HEAP_POINTER) + data_count_per_dpu * sizeof(uint32_t) + (id * WRAM_SIZE);

    uint32_t sum = 0;
    uint64_t* elapsed = &elapsed_arr[id];
    *elapsed = 0;

    if (id == 0) {
        mem_reset();
        perfcounter_config(COUNT_CYCLES, true);
    }

    barrier_wait(&barrier);
    
    uint32_t* indices = (uint32_t*)mem_alloc(wram_indices_size);
    uint32_t* from_buffer = (uint32_t*)mem_alloc(WRAM_SIZE);
    uint32_t* to_buffer = (uint32_t*)mem_alloc(WRAM_SIZE);

    uint32_t buf_offset = 0, indices_offset = 0;
    for (; buf_offset < buffer_size_per_dpu; buf_offset += WRAM_SIZE * NR_TASKLETS, indices_offset += wram_indices_size * NR_TASKLETS) {
        mram_read((__mram_ptr void const*)(indices_address + indices_offset), indices, wram_indices_size);
        mram_read((__mram_ptr void const*)(from_buffer_address + buf_offset), from_buffer, WRAM_SIZE);\

        barrier_wait(&barrier);

        cyclecount_start(&cc);
        #pragma unroll
        for (uint32_t i = 0; i < data_count_per_tasklet; i++) {
            uint32_t index = indices[i];
            sum += from_buffer[index];
        }
        *elapsed += cyclecount_end(&cc);

        barrier_wait(&barrier);
    }
    sum_arr[id] = sum;

    return 0;
}