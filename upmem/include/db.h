#ifndef DB_H
#define DB_H

#include <stdio.h>
#include "fpgrowth_defs.h"

FILE* stream;

void open_db(const char* filename) {
    stream = fopen(filename, "r");
    if (stream == NULL) {
        perror("Failed to open database file");
        exit(EXIT_FAILURE);
    }
}
FrequentItemSet* scan_for_frequent_items(int min_support);
Vector* filtered_items(ItemCountList* item_count, int min_support);

#endif // DB_H