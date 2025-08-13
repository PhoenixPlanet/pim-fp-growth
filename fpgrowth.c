#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fpgrowth.h"
#include "db.h"
#include "fpgrowth_defs.h"


void build_tree(FILE* stream, int min_support) {
    FrequentItemSet* frequent_items = scan_for_frequent_items(stream, min_support);

    HeaderTable* header_table = create_header_table();
    for (FrequentItem* item = frequent_items->head; item != NULL; item) {
        HeaderTableEntry* entry = (HeaderTableEntry*)malloc(sizeof(HeaderTableEntry));
        entry->item = item->item;
        entry->node_link = NULL; // Initialize node link to NULL
        entry->next = NULL;
        push_header_table(header_table, entry);
    }
    while (1) {
        int* items = filtered_items(stream, min_support);
        if (items == NULL) break;
        if (items[0] == -1) {
            free(items);
            continue; // Skip empty transactions
        }
        Node* current_node = root;

        for(int item = 0; items[item] != -1; item++) {
            int found = 0;
            for (Node* child = current_node->first_child; child != NULL; child = child->next_sibling) {
                if (child->item.item_id == items[item]) {
                    child->item.count++;
                    current_node = child;
                    found = 1;
                    break;
                }
            }
            if (!found) {
                Node* new_node = (Node*)malloc(sizeof(Node));
                new_node->item.item_id = items[item];
                new_node->item.count = 1;
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
                HeaderTableEntry* htb_entry = find_header_table_entry(header_table, items[item]);
                if (htb_entry != NULL) {
                    push_header_table(htb_entry, new_node);
                } else {
                    fprintf(stderr, "Error: Item not found in header table: %d\n", items[item]);
                }
            }
        }
        free(items);
    }
}