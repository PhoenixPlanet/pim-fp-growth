#ifndef FPGROWTH_DEFS_H
#define FPGROWTH_DEFS_H


#include <stdio.h>
#include <stdlib.h>

/**
 * @file fpgrowth_defs.h
 * @brief Definitions and structures for the FP-Growth algorithm.
 */

#define MAX_ITEMS 1024

#define NULL 0

#define INSERT(list, element) ({\
    if (list->head == NULL) {\
        list->head = element;\
        list->tail = element;\
    } else {\
        list->tail->next = element;\
        list->tail = element;\
    }\
    element->next = NULL;\
    list->size++;\
})

#define SORTED_INSERT(type, list, value_name, element) ({\
    type* current = list->head;\
    while (current != NULL && current->value_name < element->value_name) {\
        current = current->next;\
    }\
    if (current == NULL) {\
        if (list->tail == NULL) {\
            list->head = element;\
            list->tail = element;\
        } else {\
            list->tail->next = element;\
            list->tail = element;\
        }\
    } else {\
        if (current == list->head) {\
            element->next = list->head;\
            list->head = element;\
        } else {\
            type* prev = list->head;\
            while (prev->next != current) {\
                prev = prev->next;\
            }\
            prev->next = element;\
            element->next = current;\
        }\
    }\
    list->size++;}\
)

/* Basic Data structure */

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

Node* node(int item_id, int count) {
    Node* node = (Node*)malloc(sizeof(Node));
    node->item.item_id = item_id;
    node->item.count = count;
    node->parent = NULL;
    node->first_child = NULL;
    node->next_sibling = NULL;
    return node;
}

/* Data structures for dynamic list */

typedef struct Scalar {
    int value;
    Scalar* next;
} Scalar;

typedef struct Vector {
    Scalar* head;
    Scalar* tail;
    int size;
} Vector;

Vector* vector() {
    Vector* v = (Vector*)malloc(sizeof(Vector));
    v->head = NULL;
    v->tail = NULL;
    v->size = 0;
    return v;
}

Scalar* scalar(int value) {
    Scalar* s = (Scalar*)malloc(sizeof(Scalar));
    s->value = value;
    s->next = NULL;
    return s;
}

int find_scalar(Vector* vector, int value) {
    Scalar* current = vector->head;
    while (current != NULL) {
        if (current->value == value) {
            return 1;
        }
        current = current->next;
    }
    return 0; // Not found
}

void free_vector(Vector* vector) {
    Scalar* current = vector->head;
    Scalar* next;
    while(current) {
        next = current->next;
        free(current);
        current = next;
    }
    free(vector);
}

typedef struct ItemCount {
    int item_id;
    int count;
    ItemCount* next;
} ItemCount;

typedef struct ItemCountList {
    ItemCount* head;
    ItemCount* tail;
    int size;
} ItemCountList;

ItemCountList* itemCountList() {
    ItemCountList* list = (ItemCountList*)malloc(sizeof(ItemCountList));
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    return list;
}

ItemCount* itemCount(int item_id, int count) {
    ItemCount* ic = (ItemCount*)malloc(sizeof(ItemCount));
    ic->item_id = item_id;
    ic->count = count;
    ic->next = NULL;
    return ic;
}

void incr_item_count(ItemCountList* list, int item_id, int amount) {
    ItemCount* current = list->head;
    while (current != NULL) {
        if (current->item_id == item_id) {
            current->count += amount;;
            return;
        }
        current = current->next;
    }
    // If not found, create a new item count
    ItemCount* new_ic = itemCount(item_id, amount);
    INSERT(list, new_ic);
}

int find_item_count(ItemCountList* list, int item_id) {
    ItemCount* current = list->head;
    while (current != NULL) {
        if (current->item_id == item_id) {
            return current->count; // Found
        }
        current = current->next;
    }
    return 0; // Not found
}

void free_item_count_list(ItemCountList* list) {
    ItemCount* current = list->head;
    ItemCount* next;
    while(current) {
        next = current->next;
        free(current);
        current = next;
    }
    free(list);
}

typedef struct NodeLink {
    Node* node_link;
    NodeLink* next;
} NodeLink;

typedef struct NodeLinkList {
    NodeLink* head;
    NodeLink* tail;
    int size;
} NodeLinkList;

NodeLinkList* nodeLinkList() {
    NodeLinkList* n = (NodeLinkList*)malloc(sizeof(NodeLinkList));
    n->head = NULL;
    n->tail = NULL;
    n->size = 0;
    return n;
}

NodeLink* nodeLink(Node* node_link) {
    NodeLink* n = (NodeLink*)malloc(sizeof(NodeLink));
    n->node_link = node_link;
    n->next = NULL;
    return n;
}

NodeLink* free_node_link_list(NodeLinkList* list) {
    NodeLink* current = list->head;
    NodeLink* next;
    while(current) {
        next = current->next;
        free(current);
        current = next;
    }
    free(list);
    return NULL;
}

typedef struct HeaderTableEntry {
    int item_id;
    int frequency;
    NodeLinkList* node_link;
    HeaderTableEntry* next;
} HeaderTableEntry;

typedef struct HeaderTable {
    HeaderTableEntry* head;
    HeaderTableEntry* tail;
    int size;
} HeaderTable;

HeaderTable* headerTable() {
    HeaderTable* ht = (HeaderTable*)malloc(sizeof(HeaderTable));
    ht->head = NULL;
    ht->tail = NULL;
    ht->size = 0;
    return ht;
}

HeaderTableEntry* headerTableEntry(int item_id, int frequency) {
    HeaderTableEntry* entry = (HeaderTableEntry*)malloc(sizeof(HeaderTableEntry));
    entry->item_id = item_id;
    entry->frequency = frequency;
    entry->node_link = nodeLinkList();
    entry->next = NULL;
    return entry;
}

HeaderTableEntry* find_header_table_entry(HeaderTable* table, int item_id) {
    HeaderTableEntry* current = table->head;
    while (current != NULL) {
        if (current->item_id == item_id) {
            return current; // Found
        }
        current = current->next;
    }
    return NULL; // Not found
}

void free_header_table(HeaderTable* table) {
    HeaderTableEntry* current = table->head;
    HeaderTableEntry* next;
    while(current) {
        next = current->next;
        free_node_link_list(current->node_link);
        free(current);
        current = next;
    }
    free(table);
}

typedef struct PatternBase {
    Vector* transaction;
    int count;
    PatternBase* next;
} PatternBase;

typedef struct PatternBaseList {
    PatternBase* head;
    PatternBase* tail;
    int size;
} PatternBaseList;

PatternBaseList* patternBaseList() {
    PatternBaseList* pbl = (PatternBaseList*)malloc(sizeof(PatternBaseList));
    pbl->head = NULL;
    pbl->tail = NULL;
    pbl->size = 0;
    return pbl;
}

PatternBase* patternBase(Vector* transaction, int count) {
    PatternBase* pb = (PatternBase*)malloc(sizeof(PatternBase));
    pb->transaction = transaction;
    pb->count = count;
    pb->next = NULL;
    return pb;
}

void free_pattern_base_list(PatternBaseList* list) {
    PatternBase* current = list->head;
    PatternBase* next;
    while(current) {
        next = current->next;
        free_vector(current->transaction);
        free(current);
        current = next;
    }
    free(list);
}

typedef struct FrequentItem {
    Item item;
    FrequentItem* next;
} FrequentItem;

typedef struct FrequentItemSet {
    FrequentItem* head;
    FrequentItem* tail;
    int size;
} FrequentItemSet;

FrequentItemSet* frequentItemSet() {
    FrequentItemSet* set = (FrequentItemSet*)malloc(sizeof(FrequentItemSet));
    set->head = NULL;
    set->tail = NULL;
    set->size = 0;
    return set;
}

FrequentItem* frequentItem(int item_id, int count) {
    FrequentItem* fi = (FrequentItem*)malloc(sizeof(FrequentItem));
    fi->item.item_id = item_id;
    fi->item.count = count;
    fi->next = NULL;
    return fi;
}

void free_frequent_item_set(FrequentItemSet* set) {
    FrequentItem* current = set->head;
    FrequentItem* next;
    while(current) {
        next = current->next;
        free(current);
        current = next;
    }
    free(set);
}

#endif // FPGROWTH_DEFS_H