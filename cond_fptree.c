#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fpgrowth_defs.h"
#include "fpgrowth.h"

void build_conditional_tree(PatternBaseList* pattern_base, int min_support) {

    int item_count[1024] = {0};

    for (PatternBase* current = pattern_base->head; current != NULL; current = current->next) {
        if (current->transaction == NULL) continue; // Skip empty transactions
        for (int i = 0; current->transaction[i] != -1; i++) {
            int item = current->transaction[i];
            item_count[item] += current->count;
        }
    }

    HeaderTable* header_table = create_header_table();

    for (int item_id = 0; item_id < 1024; item_id++) {
        if (item_count[item_id] >= min_support) {
            HeaderTableEntry* entry = (HeaderTableEntry*)malloc(sizeof(HeaderTableEntry));
            entry->item = (Item*)malloc(sizeof(Item));
            entry->item->item_id = item_id;
            entry->item->count = item_count[item_id];
            entry->node_link = NULL; // Initialize node link to NULL
            entry->next = NULL;
            push_header_table(header_table, entry);
        }
    }

    SortedItemList* filtered_items = create_sorted_item_list();
    for (PatternBase* current = pattern_base->head; current != NULL; current = current->next) {
        if (current->transaction == NULL) continue; // Skip empty transactions

        int* transaction = current->transaction;
        int count = current->count;

        for (int i = 0; transaction[i] != -1; i++) {
            int item = transaction[i];
            if (item_count[item] >= min_support) {
                push_sorted_item_list(filtered_items, item);
            }
        }
    

        Node* current_node = root;

        for (SortedItemEntry* entry = filtered_items->head; entry != NULL; entry = entry->next) {
            int item = entry->item;
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
                Node* new_node = (Node*)malloc(sizeof(Node));
                new_node->item.item_id = item;
                new_node->item.count = count;
                new_node->parent = current_node;
                new_node->first_child = NULL;
                new_node->next_sibling = NULL;

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
                    push_header_table(htb_entry, new_node);
                } else {
                    fprintf(stderr, "Error: Item not found in header table: %d\n", item);
                }
            }
        }
    }
    
    free_sorted_item_list(filtered_items);
    free_header_table(header_table);
}

void mine_pattern(HeaderTable* header_table, int* prefix_path, int prefix_length,FrequentItemSet* frequent_itemsets, int min_support) {
    for (HeaderTableEntry* entry = header_table->head; entry != NULL; entry = entry->next) {
        int item_id = entry->item->item_id;
        int item_count = entry->item->count;

        // Create a new prefix path
        int* new_prefix_path = (int*)malloc((prefix_length + 1) * sizeof(int));
        memcpy(new_prefix_path, prefix_path, prefix_length * sizeof(int));
        new_prefix_path[prefix_length] = item_id;

        // Add the new prefix path to the frequent itemsets
        FrequentItem* new_item = (FrequentItem*)malloc(sizeof(FrequentItem));
        new_item->item = entry->item;
        new_item->next = NULL;
        push_frequent_item_set(frequent_itemsets, new_item);

        // Build conditional pattern base
        PatternBaseList* pattern_base = create_pattern_base_list();
        for (Node** node_link = entry->node_link; node_link != NULL; node_link++) {
            Node* node = *node_link;
            PatternBase* base = (PatternBase*)malloc(sizeof(PatternBase));
            base->transaction = (int*)malloc((prefix_length + 2) * sizeof(int));
            memcpy(base->transaction, new_prefix_path, (prefix_length + 1) * sizeof(int));
            base->transaction[prefix_length + 1] = -1; // Null-terminate the transaction
            base->count = node->item.count;
            base->next = NULL;
            push_pattern_base_list(pattern_base, base);
        }

        // Recursively mine patterns from the conditional pattern base
        build_conditional_tree(pattern_base, min_support);
        
        free(new_prefix_path);
        free_pattern_base_list(pattern_base);
    }
}