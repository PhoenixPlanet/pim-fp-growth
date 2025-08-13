#ifndef FPGROWTH_H
#define FPGROWTH_H

#include "fpgrowth_defs.h"
#include "db.h"

typedef struct HeaderTableEntry {
    int item;
    int frequency;
    Node** node_link;
} HeaderTableEntry;

typedef struct PatternBase {
    int* transaction;
    int count;
} PatternBase;

Node* _root;
int _min_support;
HeaderTableEntry* _header_table;

void init_FPTree(int min_support, Database* db);
void init_FPTree(int min_support);

void build_tree();
void build_conditional_tree(struct PatternBase* pattern_base, int min_support);
void mine_pattern(std::vector<int>& prefix_path, std::vector<std::vector<int>>& frequent_itemsets);
void delete_tree();

int mem_count;

#endif