#ifndef FPGROWTH_H
#define FPGROWTH_H

#include <vector>
#include <list>

#include "db.hpp"
#include "common.h"
#include "param.h"

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
    std::unordered_map<uint64_t, CandidateEntry> dpu_mine_candidates(dpu::DpuSet& system, const std::vector<ElePosEntry>& ele_pos);
    void mine_frequent_itemsets();
    void build_conditional_tree(std::vector<std::pair<std::vector<int>, int>>& pattern_base, int min_support);
    void mine_pattern(std::vector<int>& prefix_path, std::vector<std::vector<int>>& frequent_itemsets);
    void delete_tree();

    std::vector<std::vector<uint32_t>> get_frequent_itemsets() {
        std::vector<std::vector<uint32_t>> frequent_itemsets(_frequent_itemsets_1.begin(), _frequent_itemsets_1.end());
        frequent_itemsets.insert(frequent_itemsets.end(), _frequent_itemsets_gt1.begin(), _frequent_itemsets_gt1.end());

        return frequent_itemsets;
    }

private:
    Node* _root; // Root item number is 0
    Node* _leaf_head;
    Database* _db;
    int _min_support;
    std::vector<HeaderTableEntry> _header_table;
    std::vector<FPArrayEntry> _fp_array;
    std::vector<ElePosEntry> _k1_ele_pos;
    std::vector<std::vector<uint32_t>> _frequent_itemsets_1;
    std::vector<std::vector<uint32_t>> _frequent_itemsets_gt1;
    uint32_t _itemset_id = NR_DB_ITEMS;

    void delete_tree(Node* node);
};

#endif