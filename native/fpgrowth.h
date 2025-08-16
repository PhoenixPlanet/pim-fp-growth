#ifndef FPGROWTH_H
#define FPGROWTH_H

#include "fpgrowth_defs.h"
#include "db.h"

Node* root;
HeaderTableEntry* _header_table;

void init_FPTree(int min_support) {
    root = node(-1, 0); // Create root node with item_id -1 and count 0
    _header_table = headerTable(); // Initialize header table
    mem_count = 0; // Initialize memory count
}

void build_tree(int min_support);
void build_conditional_tree(PatternBaseList* pattern_base, int min_support);
void mine_pattern(HeaderTable* header_table, Vector* prefix_path,
    FrequentItemSet* frequent_itemsets, int min_support);
void delete_tree();

int mem_count;

#endif