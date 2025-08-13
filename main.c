#include "fpgrowth_defs.h"
#include "fpgrowth.h"
#include "db.h"

#include <stdio.h>

#define MIN_SUPPORT 3
#define DB_PATH "data.txt"

int main() {

    open_db(DB_PATH);

    build_tree(DB_PATH, MIN_SUPPORT);

    FrequentItemSet* frequent_itemsets = create_frequent_item_set();
    int* prefix_path = (int*)malloc(sizeof(int) * 1024);
    int prefix_length = 0;
    mine_pattern(prefix_path, frequent_itemsets);

    return 0;
}