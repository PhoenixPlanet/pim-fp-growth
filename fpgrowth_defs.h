#ifndef FPGROWTH_DEFS_H
#define FPGROWTH_DEFS_H

/**
 * @file fpgrowth_defs.h
 * @brief Definitions and structures for the FP-Growth algorithm.
 */

#define MAX_ITEMS 1024

#define NULL 0

typedef struct Item {
    int item_id;
    int count;
} Item;

typedef struct Node {
    Item item;
    Node* parent;
    Node* first_child;
    Node* next_sibling;
} Node;

typedef struct HeaderTableEntry {
    Item* item;
    Node** node_link;
    HeaderTableEntry* next;
} HeaderTableEntry;

typedef struct HeaderTable {
    HeaderTableEntry* head;
    HeaderTableEntry* tail;
    int size;
} HeaderTable;

HeaderTable* create_header_table() {
    HeaderTable* table = (HeaderTable*)malloc(sizeof(HeaderTable));
    table->head = NULL;
    table->tail = NULL;
    table->size = 0;
    return table;
}

void push_header_table(HeaderTable* table, HeaderTableEntry* entry) {
    if (table->head == NULL) {
        table->head = entry;
        table->tail = entry;
    } else {
        table->tail->next = entry;
        table->tail = entry;
    }
    entry->next = NULL;
    table->size++;
}

void free_header_table(HeaderTable* table) {
    HeaderTableEntry* current = table->head;
    while (current != NULL) {
        HeaderTableEntry* next = current->next;
        free(current);
        current = next;
    }
    free(table);
}

typedef struct PatternBase {
    int* transaction;
    int count;
    PatternBase* next;
} PatternBase;

typedef struct PatternBaseList {
    PatternBase* head;
    PatternBase* tail;
    int size;
} PatternBaseList;

PatternBaseList* create_pattern_base_list() {
    PatternBaseList* list = (PatternBaseList*)malloc(sizeof(PatternBaseList));
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    return list;
}

void push_pattern_base_list(PatternBaseList* list, PatternBase* base) {
    if (list->head == NULL) {
        list->head = base;
        list->tail = base;
    } else {
        list->tail->next = base;
        list->tail = base;
    }
    base->next = NULL;
    list->size++;
}

void free_pattern_base_list(PatternBaseList* list) {
    PatternBase* current = list->head;
    while (current != NULL) {
        PatternBase* next = current->next;
        free(current->transaction);
        free(current);
        current = next;
    }
    free(list);
}

typedef struct FrequentItem {
    Item* item;
    FrequentItem* next;
} FrequentItem;

typedef struct FrequentItemSet {
    FrequentItem* head;
    FrequentItem* tail;
    int size;
} FrequentItemSet;

FrequentItemSet* create_frequent_item_set() {
    FrequentItemSet* set = (FrequentItemSet*)malloc(sizeof(FrequentItemSet));
    set->head = NULL;
    set->tail = NULL;
    set->size = 0;
    return set;
}

void push_frequent_item_set(FrequentItemSet* set, FrequentItem* new_item) {
    FrequentItem* current = set->head;
    while (current != NULL && current->item->count < new_item->item->count) {
        current = current->next;
    }
    if (current == NULL) {
        if (set->tail == NULL) {
            set->head = new_item;
            set->tail = new_item;
        } else {
            set->tail->next = new_item;
            set->tail = new_item;
        }
    } else {
        if (current == set->head) {
            new_item->next = set->head;
            set->head = new_item;
        } else {
            FrequentItem* prev = set->head;
            while (prev->next != current) {
                prev = prev->next;
            }
            prev->next = new_item;
            new_item->next = current;
        }
    }
    set->size++;
}

void free_frequent_item_set(FrequentItemSet* set) {
    FrequentItem* current = set->head;
    while (current != NULL) {
        FrequentItem* next = current->next;
        free(current->item);
        free(current);
        current = next;
    }
    free(set);
}

#endif // FPGROWTH_DEFS_H