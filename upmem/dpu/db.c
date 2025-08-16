#include <stdio.h>
#include <string.h>


#include "fpgrowth_defs.h"
#include "db.h"

FrequentItemSet* scan_for_frequent_items(ItemCountList* item_count, int min_support) {
    char line[1024];
    item_count = itemCountList();
    FrequentItemSet* set = frequentItemSet();

    while(fscanf(stream, "%[^\n]\n", line) == 1) {
        char* token = strtok(line, " ");
        while(token != NULL) {
            int item = atoi(token);
            incr_item_count(item_count, item, 1);
            token = strtok(NULL, " ");
        }
    }

    for (int i = 0; i < 1024; i++) {
        int c = find_item_count(item_count, i);
        if (c >= min_support) {
            FrequentItem* fi = frequentItem(i, c);
            INSERT(set, fi);
        }
    }

    return set;
}

Vector* filtered_items(ItemCountList* item_count, int min_support) {
    char line[1024];
    Vector* items = vector();
    while (fscanf(stream, "%[^\n]\n", line) == 1) {
        char* token = strtok(line, " ");
        int item = atoi(token);
        if (find_item_count(item_count, item) >= min_support) {
            Scalar* s = scalar(item);
            INSERT(items, s);
        }
    }
    return items;
}