#ifndef FPGROWTH_H
#define FPGROWTH_H

#include <vector>
#include <list>

#include "db.hpp"
#include "common.h"

struct Node {
    uint32_t item;
    uint32_t count;
    uint32_t depth;
    Node* parent;
    std::list<Node*> child;
    Node* next_leaf;
    Node* prev_leaf;

    Node(uint32_t item, uint32_t count, Node* parent, uint32_t depth): item(item), count(count), parent(parent), depth(depth) {}
    Node(uint32_t item, uint32_t count, Node* parent): Node(item, count, parent, parent ? parent->depth + 1 : 0) {}
};

struct HeaderTableEntry {
    int item;
    int frequency;
    std::list<Node*> node_link;
};



class FPTree {
public:
    FPTree(int min_support, Database* db): _root(new Node(0, 0, nullptr, 0)), _min_support(min_support), _db(db) {}
    FPTree(int min_support): _root(new Node(0, 0, nullptr, 0)), _min_support(min_support), _db(nullptr) {}

    void build_tree();
    void build_fp_array();
    void build_k1_ele_pos();
    void dpu_mine_candidates();
    void mine_frequent_itemsets();
    void build_conditional_tree(std::vector<std::pair<std::vector<int>, int>>& pattern_base, int min_support);
    void mine_pattern(std::vector<int>& prefix_path, std::vector<std::vector<int>>& frequent_itemsets);
    void delete_tree();

private:
    Node* _root; // Root item number is 0
    Node* _leaf_head;
    Database* _db;
    int _min_support;
    std::vector<HeaderTableEntry> _header_table;
    std::vector<FPArrayEntry> _fp_array;
    std::vector<ElePosEntry> _k1_ele_pos;

    void delete_tree(Node* node);
};

#endif