#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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