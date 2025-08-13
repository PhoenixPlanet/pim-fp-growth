#ifndef FPGROWTH_H
#define FPGROWTH_H

#include "fpgrowth_defs.h"
#include "db.h"

Node* root;
HeaderTableEntry* _header_table;

void init_FPTree(int min_support);

void build_tree(FILE* stream, int min_support);
void build_conditional_tree(PatternBase* pattern_base, int min_support);
void mine_pattern(int* prefix_path, FrequentItemSet frequent_itemsets);
void delete_tree();

int mem_count;

#endif