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

    Node(uint32_t item, uint32_t count, Node* parent, uint32_t depth): item(item), count(count), parent(parent), depth(depth), next_leaf(nullptr), prev_leaf(nullptr) {}
    Node(uint32_t item, uint32_t count, Node* parent): Node(item, count, parent, parent ? parent->depth + 1 : 0) {}
};

struct HeaderTableEntry {
    int item;
    int frequency;
    std::list<Node*> node_link;
};

struct TempCandidates {
private:
    uint32_t support;
    uint32_t prefix_item;
    uint32_t suffix_item;

public:
    std::vector<CandidateEntry> candidates;

    TempCandidates() : support(0), prefix_item(0), suffix_item(0) {}

    TempCandidates(CandidateEntry first): support(first.support), prefix_item(first.prefix_item), suffix_item(first.suffix_item) {
        candidates.push_back(first);
    }

    void add_candidate(const CandidateEntry& candidate) {
        candidates.push_back(candidate);
        support += candidate.support;
    }

    uint32_t get_support() const {
        return support;
    }

    uint32_t get_prefix_item() const {
        return prefix_item;
    }

    uint32_t get_suffix_item() const {
        return suffix_item;
    }
};

class FPTree {
public:
    FPTree(int min_support, Database* db): _root(new Node(0, 0, nullptr, 0)), _leaf_head(nullptr), _min_support(min_support), _db(db), _itemset_id(NR_DB_ITEMS) {}
    FPTree(int min_support): _root(new Node(0, 0, nullptr, 0)), _leaf_head(nullptr), _min_support(min_support), _db(nullptr), _itemset_id(NR_DB_ITEMS) {}
    ~FPTree() {
        // Clear header table to avoid dangling pointers
        for (auto& entry : _header_table) {
            entry.node_link.clear();
        }
        _header_table.clear();
        
        // Reset leaf pointers
        _leaf_head = nullptr;
        
        // Delete the tree
        if (_root) {
            delete_tree(_root);
            _root = nullptr;
        }
    }

    void build_tree();
    void build_fp_array();
    void build_k1_ele_pos();
    void cpu_mine_candidates(const std::vector<ElePosEntry>& ele_pos, 
                                 std::unordered_map<uint64_t, TempCandidates>& candidate_map);
    void mine_frequent_itemsets();
    std::unordered_map<uint64_t, TempCandidates> mine_candidates(const std::vector<ElePosEntry>& ele_pos);
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