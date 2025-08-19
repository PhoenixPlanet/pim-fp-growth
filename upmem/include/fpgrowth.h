#ifndef FPGROWTH_H
#define FPGROWTH_H

#include <vector>
#include <list>

#include "db.hpp"

struct Node {
    int item;
    int count;
    Node* parent;
    std::list<Node*> child;
    Node* next_leaf;
    Node* prev_leaf;

    Node(int item, int count, Node* parent): item(item), count(count), parent(parent) {}
};

struct HeaderTableEntry {
    int item;
    int frequency;
    std::list<Node*> node_link;
};

struct FPArrayEntry {
    int32_t item;
    int32_t parent_index;
    int32_t support;
    int32_t padding;
};

struct ElePosEntry {
    int32_t item;
    int32_t pos;
    int32_t support;
    int32_t padding;
};

class FPTree {
public:
    FPTree(int min_support, Database* db): _root(new Node(-1, 0, nullptr)), _min_support(min_support), _db(db) {}
    FPTree(int min_support): _root(new Node(-1, 0, nullptr)), _min_support(min_support), _db(nullptr) {}

    void build_tree();
    void build_fp_array();
    void build_k1_ele_pos();
    void build_conditional_tree(std::vector<std::pair<std::vector<int>, int>>& pattern_base, int min_support);
    void mine_pattern(std::vector<int>& prefix_path, std::vector<std::vector<int>>& frequent_itemsets);
    void delete_tree();

private:
    Node* _root;
    Node* _leaf_head;
    Database* _db;
    int _min_support;
    std::vector<HeaderTableEntry> _header_table;
    std::vector<FPArrayEntry> _fp_array;
    std::vector<ElePosEntry> _k1_ele_pos;

    void delete_tree(Node* node);
};

#endif