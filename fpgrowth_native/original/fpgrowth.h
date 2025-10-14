#ifndef FPGROWTH_H
#define FPGROWTH_H

#include <vector>
#include <list>

#include "db.h"

struct Node {
    int item;
    int count;
    Node* parent;
    std::list<Node*> child;

    Node(int item, int count, Node* parent): item(item), count(count), parent(parent) {}
};

struct HeaderTableEntry {
    int item;
    int frequency;
    std::list<Node*> node_link;
};

class FPTree {
public:
    FPTree(int min_support, Database* db)
        : _root(new Node(-1, 0, nullptr)), _db(db), _min_support(min_support) {}
    FPTree(int min_support)
        : _root(new Node(-1, 0, nullptr)), _db(nullptr), _min_support(min_support) {}

    void build_tree();
    void build_conditional_tree(std::vector<std::pair<std::vector<int>, int>>& pattern_base, int min_support);
    void build_conditional_tree(std::vector<std::pair<std::vector<int>, int>>& pattern_base, int min_support, const std::map<int, int>& global_order);
    void mine_pattern(std::vector<int>& prefix_path, std::vector<std::vector<int>>& frequent_itemsets);
    void delete_tree();

private:
    Node* _root;
    Database* _db;
    int _min_support;
    std::vector<HeaderTableEntry> _header_table;
    std::map<int, int> _global_item_order;  // Maps item -> order (lower is more frequent)

    void delete_tree(Node* node);
};

extern int mem_count;

#endif