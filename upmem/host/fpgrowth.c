#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fpgrowth.h"
#include "db.h"
#include "fpgrowth_defs.h"


void build_tree(int min_support) {
    ItemCountList* item_count = itemCountList();
    FrequentItemSet* frequent_items = scan_for_frequent_items(item_count, min_support);

    HeaderTable* header_table = headerTable();
    for (FrequentItem* item = frequent_items->head; item != NULL; item) {
        HeaderTableEntry* entry = headerTableEntry(item->item.item_id, item->item.count);
        SORTED_INSERT(HeaderTableEntry, header_table, item_id, entry);
    }
    while (1) {
        Vector* items = filtered_items(item_count, min_support);
        if (items == NULL) break;
        if (items->size == 0) {
            free_vector(items);
            continue; // Skip empty transactions
        }
        Node* current_node = root;

        for(Scalar* item = items->head; item != NULL; item = item->next) {
            int found = 0;
            for (Node* child = current_node->first_child; child != NULL; child = child->next_sibling) {
                if (child->item.item_id == item->value) {
                    child->item.count++;
                    current_node = child;
                    found = 1;
                    break;
                }
            }
            if (!found) {
                Node* n = node(item->value, 1);
                n->parent = current_node;

                if (current_node->first_child == NULL) {
                    current_node->first_child = n;
                } else {
                    Node* sibling = current_node->first_child;
                    while (sibling->next_sibling != NULL) {
                        sibling = sibling->next_sibling;
                    }
                    sibling->next_sibling = n;
                }

                // Add to header table
                HeaderTableEntry* htb_entry = find_header_table_entry(header_table, item->value);
                if (htb_entry != NULL) {
                    INSERT(htb_entry->node_link, nodeLink(n));
                    htb_entry->frequency++;
                    current_node = n;
                } else {
                    fprintf(stderr, "Error: Item not found in header table: %d\n", item->value);
                }
            }
        }
    }
}