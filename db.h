#ifndef DB_H
#define DB_H

#include <stdio.h>
#include "fpgrowth_defs.h"

typedef struct SortedItemEntry {
    int item;
    SortedItemEntry* next;
} SortedItemEntry;

typedef struct SortedItemList {
    SortedItemEntry* head;
    SortedItemEntry* tail;
    int size;
} SortedItemList;

int item_count[1024];


int find_item_count(int item) {
    if (item < 0 || item >= 1024) {
        return 0; // Invalid item ID
    }
    return item_count[item];
}


SortedItemList* create_sorted_item_list() {
    SortedItemList* list = (SortedItemList*)malloc(sizeof(SortedItemList));
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    return list;
}

void push_sorted_item_list(SortedItemList* list, int item) {
    SortedItemEntry* entry = (SortedItemEntry*)malloc(sizeof(SortedItemEntry));
    entry->item = item;
    SortedItemEntry* current = list->head;
    while(current != NULL && current->item < item) {
        current = current->next;
    }
    if (current == NULL) {
        if (list->tail == NULL) {
            list->head = entry;
            list->tail = entry;
        } else {
            list->tail->next = entry;
            list->tail = entry;
        }
    } else {
        if (current == list->head) {
            entry->next = list->head;
            list->head = entry;
        } else {
            SortedItemEntry* prev = list->head;
            while(prev->next != current) {
                prev = prev->next;
            }
            prev->next = entry;
            entry->next = current;
        }
    }
    list->size++;
}

void free_sorted_item_list(SortedItemList* list) {
    SortedItemEntry* current = list->head;
    while (current != NULL) {
        SortedItemEntry* next = current->next;
        free(current);
        current = next;
    }
    free(list);
}

void seek_to_start();
FrequentItemSet* scan_for_frequent_items(FILE* stream, int min_support);
int* filtered_items(FILE* stream, int min_support);

#endif