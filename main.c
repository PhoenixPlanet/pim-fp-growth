#include "fpgrowth_defs.h"
#include "fpgrowth.h"
#include "db.h"

#include <stdio.h>

#define MIN_SUPPORT 3
#define DB_PATH "data.txt"

int main() {

    open_db(DB_PATH);
    init_FPTree(MIN_SUPPORT);

    HeaderTable* header_table = headerTable();

    build_tree(MIN_SUPPORT);

    FrequentItemSet* frequent_itemsets = frequentItemSet();
    Vector* prefix_path = vector();
    mine_pattern(header_table, prefix_path, frequent_itemsets, MIN_SUPPORT);

    return 0;
}