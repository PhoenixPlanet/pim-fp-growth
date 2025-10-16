#ifndef FPGROWTH_H
#define FPGROWTH_H

#include <vector>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <barrier>
#include <mutex>
#include <array>

#include "db.hpp"
#include "common.h"
#include "param.h"

struct Node {
    uint32_t id;
    uint32_t item;
    uint32_t count;
    Node* parent;
    uint32_t depth;
    std::list<Node*> child;
    Node* next_leaf;
    Node* prev_leaf;
    bool in_leaf_list;

    Node(uint32_t id, uint32_t item, uint32_t count, Node* parent, uint32_t depth): id(id), item(item), count(count), parent(parent), depth(depth), in_leaf_list(false) {}
    Node(uint32_t id, uint32_t item, uint32_t count, Node* parent): Node(id, item, count, parent, parent ? parent->depth + 1 : 0) {}
};

struct HeaderTableEntry {
    int item;
    int frequency;
    std::list<Node*> node_link;

    HeaderTableEntry(): item(0), frequency(0) {}
    HeaderTableEntry(int item, int frequency): item(item), frequency(frequency) {}
};

struct TempCandidates {
private:
    uint32_t support;
    uint32_t prefix_item;
    uint32_t suffix_item;

public:
    std::array<std::vector<CandidateEntry>, NR_GROUPS> candidates;

    TempCandidates() : support(0), prefix_item(0), suffix_item(0) {}

    TempCandidates(CandidateEntry first, int group_id): support(first.support), prefix_item(first.prefix_item), suffix_item(first.suffix_item) {
        candidates[group_id].push_back(first);
    }

    void add_candidate(const CandidateEntry& candidate, int group_id) {
        candidates[group_id].push_back(candidate);
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

class DpuSetWrapper {
public:
    explicit DpuSetWrapper(int group_id, uint32_t dpus_per_group): 
        group_id(group_id), 
        dpus_per_group(dpus_per_group), 
        system(dpu::DpuSet::allocate(dpus_per_group, DPU_CONFIG)) { }

    DpuSetWrapper(const DpuSetWrapper&) = delete;
    DpuSetWrapper& operator=(const DpuSetWrapper&) = delete;

    DpuSetWrapper(DpuSetWrapper&&) noexcept = default;
    DpuSetWrapper& operator=(DpuSetWrapper&&) noexcept = default;

    dpu::DpuSet& get() { return system; }
    const dpu::DpuSet& get() const { return system; }

private:
    int group_id;
    uint32_t dpus_per_group;
    dpu::DpuSet system;
};

struct SharedCandidateMap {
    static constexpr size_t SHARDS = 64;
    std::array<std::mutex, SHARDS> mutexes;
    std::array<std::unordered_map<uint64_t, TempCandidates>, SHARDS> maps;
    //std::array<std::vector<std::pair<uint64_t, CandidateEntry>>, NR_GROUPS> bins;

    std::array<std::unordered_map<uint64_t, TempCandidates>, SHARDS>& get_maps() {
        return maps;
    }

    // std::unordered_map<uint64_t, TempCandidates> get_merged_map() {
    //     std::unordered_map<uint64_t, TempCandidates> merged_map;
    //     for (int i = 0; i < NR_GROUPS; ++i) {
    //         for (const auto& [key, candidate] : bins[i]) {
    //             auto it = merged_map.try_emplace(key, candidate, i);
    //             if (!it.second) {
    //                 it.first->second.add_candidate(candidate, i);
    //             }
    //         }
    //     }
    //     return merged_map;
    // }

    void clear() {
        // for (auto& bin : bins) {
        //     bin.clear();
        // }
        for (auto& map : maps) {
            map.clear();
        }
    }

    void insert_or_update(uint64_t key, const CandidateEntry& candidate, int group_id) {
        size_t shard = key % SHARDS;
        std::unique_lock lock(mutexes[shard]);
        auto& map = maps[shard];
        auto it = map.try_emplace(key, candidate, group_id);
        if (!it.second) {
            it.first->second.add_candidate(candidate, group_id);
        }
        // auto& bin = bins[group_id];
        // bin.emplace_back(key, candidate);
    }
};

class FPTree {
public:
    FPTree(int min_support, Database* db): _root(new Node(0, 0, 0, nullptr, 0)), _node_cnt(1), _db(db), _min_support(min_support) {}
    FPTree(int min_support): _root(new Node(0, 0, 0, nullptr, 0)), _node_cnt(1), _db(nullptr), _min_support(min_support) {}

    void build_tree();
    void build_fp_array();
    void build_k1_ele_pos();
    void dpu_mine_candidates(dpu::DpuSet& system, const std::vector<ElePosEntry>& ele_pos, int group_id);
    void mine_candidates(dpu::DpuSet& system, const std::vector<ElePosEntry>& ele_pos, int group_id);
    void mine_frequent_itemsets();
    void delete_tree();

    std::vector<std::pair<uint32_t, uint32_t>> get_frequent_itemsets() {
        std::vector<std::pair<uint32_t, uint32_t>> frequent_itemsets(_frequent_itemsets_1.begin(), _frequent_itemsets_1.end());
        frequent_itemsets.insert(frequent_itemsets.end(), _frequent_itemsets_gt1.begin(), _frequent_itemsets_gt1.end());

        return frequent_itemsets;
    }

    const std::vector<std::pair<uint32_t, uint32_t>>& get_frequent_itemsets_gt1() const {
        return _frequent_itemsets_gt1;
    }

private:
    struct GlobalFPArrayEntry {
        FPArrayEntry entry;
        Node* node;
    };

    Node* _root; // Root item number is 0
    uint32_t _node_cnt;
    Node* _leaf_head;
    Database* _db;
    int _min_support;
    std::vector<GlobalFPArrayEntry> _global_fp_array;
    std::array<std::vector<FPArrayEntry>, NR_GROUPS> _local_fp_arrays; // FP Arrays for each DPU(or group)
    std::array<std::vector<ElePosEntry>, NR_GROUPS> _local_elepos_lists; // K=1 ElePos for each DPU(or group)
    std::vector<std::vector<std::pair<int, uint32_t>>> _node_to_groups; // Map node to groups containing it (group_id, local_pos)

    std::vector<std::unique_ptr<DpuSetWrapper>> _dpu_systems; // DPU systems for each group
    SharedCandidateMap _global_candidate_map; // Global candidate map
    bool _thread_running;
    
    std::vector<std::pair<uint32_t, uint32_t>> _frequent_itemsets_1;
    std::vector<std::pair<uint32_t, uint32_t>> _frequent_itemsets_gt1;
    uint32_t _itemset_id = NR_DB_ITEMS;

    void allocate_dpus();
    void merge_candidates();

    template <class Completion>
    void mine_freq_itemsets_worker(std::barrier<Completion>& sync, int group_id);

    void delete_tree(Node* node);
};

#endif