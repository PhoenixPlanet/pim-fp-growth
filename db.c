#include <stdio.h>
#include <string.h>


#include "fpgrowth_defs.h"
#include "db.h"

FrequentItemSet* scan_for_frequent_items(FILE* stream, int min_support) {
    char line[1024];
    FrequentItemSet* set = create_frequent_item_set();

    while(fscanf(stream, "%[^\n]\n", line) == 1) {
        char* token = strtok(line, " ");
        while(token != NULL) {
            int item = atoi(token);
            item_count[item]++;
            token = strtok(NULL, " ");
        }
    }

    for (int i = 0; i < 1024; i++) {
        if (item_count[i] >= min_support) {
            FrequentItem* item = (FrequentItem*)malloc(sizeof(FrequentItem));
            item->item->item_id = i;
            item->item->count = item_count[i];
            push_frequent_item_set(set, item);
        }
    }

    return set;
}

int* filtered_items(FILE* stream, int min_support) {
    char line[1024];
    SortedItemList* items = create_sorted_item_list();
    while (fscanf(stream, "%[^\n]\n", line) == 1) {
        char* token = strtok(line, " ");
        int item = atoi(token);
        if (find_item_count(item) && item_count[item] >= min_support) {
            push_sorted_item_list(items, item);
        }
    }

    int* result = (int*)malloc(items->size * sizeof(int));
    SortedItemEntry* current = items->head;
    for (int i = 0; (i < items->size) && (current); i++, current = current->next) {
        result[i] = current->item;
    }
    free_sorted_item_list(items);
    return result;
}