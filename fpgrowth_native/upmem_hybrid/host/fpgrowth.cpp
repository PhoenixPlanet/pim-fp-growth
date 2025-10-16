#include "fpgrowth.h"
#include <algorithm>
#include <vector>
#include <map>
#include <functional>
#include <unordered_map>
#include <utility>
#include <thread>
#include <barrier>

#include "param.h"
#include "timer.h"

void print_tree(Node* node, int depth = 0) {
    for (int i = 0; i < depth; ++i) std::cout << "  ";
    #ifdef PRINT
    if (node->item == -1)
        std::cout << "(root)";
    else
        std::cout << node->item << " (" << node->count << ")";
    std::cout << std::endl;
    #endif

    for (Node* child : node->child) {
        print_tree(child, depth + 1);
    }
}

void FPTree::build_tree() {
    std::vector<std::pair<int, int>> frequent_items = _db->scan_for_frequent_items(_min_support);
    Timer::instance().start("Build FP-Tree");
    std::sort(frequent_items.begin(), frequent_items.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    _frequent_itemsets_1.clear();
    for (const auto& item : frequent_items) {
        _frequent_itemsets_1.push_back({static_cast<uint32_t>(item.first), -1});
    }

    _db->seek_to_start();
    _leaf_head = nullptr;
    Timer::instance().stop();

    Timer::instance().start("Build FP-Tree - Filter & Sort");
    std::vector<std::vector<int>> items_list = _db->filtered_items();
    Timer::instance().stop();

    Timer::instance().start("Build FP-Tree");
    for (const auto& items : items_list) {
        Node* current_node = _root;
        for (int item : items) {
            bool found = false;
            for (Node* child : current_node->child) {
                if (int(child->item) == item) {
                    child->count++;
                    current_node = child;
                    found = true;
                    break;
                }
            }

            if (!found) {
                Node* new_node = new Node(_node_cnt++, item, 1, current_node);
                
                if (current_node->in_leaf_list) {
                    Node* prev = current_node->prev_leaf;
                    Node* next = current_node->next_leaf;

                    if (_leaf_head == current_node) {
                        _leaf_head = current_node->next_leaf;
                        if (_leaf_head) {
                            _leaf_head->prev_leaf = nullptr;
                        }
                    }

                    if (prev) {
                        prev->next_leaf = next;
                    }
                    if (next) {
                        next->prev_leaf = prev;
                    }

                    current_node->next_leaf = nullptr;
                    current_node->prev_leaf = nullptr;
                    current_node->in_leaf_list = false;
                }

                current_node->child.push_back(new_node);
                current_node = new_node;
            }
        }

        if (current_node != _root && !current_node->in_leaf_list && current_node->child.empty()) {
            if (_leaf_head) {
                _leaf_head->prev_leaf = current_node;
            }
            current_node->next_leaf = _leaf_head;
            current_node->prev_leaf = nullptr;
            current_node->in_leaf_list = true;
            _leaf_head = current_node;
        }
    }
    Timer::instance().stop();
}

void FPTree::build_fp_array() {
    std::vector<int> global_item_idx_table(_node_cnt, -1);
    _global_fp_array.clear();

    std::vector<std::vector<int>> local_item_idx_tables(NR_GROUPS, std::vector<int>(_node_cnt, -1));

    _node_to_groups.resize(_node_cnt, std::vector<std::pair<int, uint32_t>>());

    Node* target_leaf = _leaf_head;
    int idx = 0;
    while (target_leaf) {
        Node* current = target_leaf;
        int group_id = idx % NR_GROUPS;

        std::vector<FPArrayEntry>& local_fp_array = _local_fp_arrays[group_id];
        std::vector<int>& local_item_idx_table = local_item_idx_tables[group_id];

        _global_fp_array.push_back(GlobalFPArrayEntry {FPArrayEntry {current->item, -1, current->count, current->depth}, current});
        local_fp_array.push_back(FPArrayEntry {current->item, -1, current->count, current->depth});

        _node_to_groups[current->id].push_back({group_id, (int)local_fp_array.size() - 1});

        bool global_finished = false;
        bool local_finished = false;

        while (current) {
            if (current->item == 0) break;

            Node* parent = current->parent;
            FPArrayEntry& global_last_entry = _global_fp_array.back().entry;
            if (global_item_idx_table[parent->id] != -1) {
                global_last_entry.parent_pos = global_item_idx_table[parent->id];
                global_finished = true;
            }

            FPArrayEntry& local_last_entry = local_fp_array.back();
            if (local_item_idx_table[parent->id] != -1) {
                local_last_entry.parent_pos = local_item_idx_table[parent->id];
                local_finished = true;
            }

            if (global_finished && local_finished) {
                break;
            }

            if (!global_finished) {
                int global_pos = _global_fp_array.size();
                global_item_idx_table[parent->id] = global_pos;
                global_last_entry.parent_pos = global_pos;

                _global_fp_array.push_back(GlobalFPArrayEntry {FPArrayEntry {parent->item, -1, parent->count, parent->depth}, parent});
            }

            if (!local_finished) {
                int local_pos = local_fp_array.size();
                local_item_idx_table[parent->id] = local_pos;
                local_last_entry.parent_pos = local_pos;

                local_fp_array.push_back(FPArrayEntry {parent->item, -1, parent->count, parent->depth});
                _node_to_groups[parent->id].push_back({group_id, local_pos});
            }

            current = parent;
        }

        target_leaf = target_leaf->next_leaf;
        idx++;
    }

    //std::cout << "FP-Array has " << _fp_array.size() << " nodes." << std::endl;
    //std::cout << "FP-Array size: " << _fp_array.size() * sizeof(FPArrayEntry) / 1024.0 << " KB" << std::endl;
    // for (GlobalFPArrayEntry& entry : _global_fp_array) {
    //     std::cout << "Global FP-Array Entry: (" << entry.entry.item << ", " << entry.entry.parent_pos << ", " << entry.entry.support << ", " << entry.entry.depth << ")" << std::endl;
    // }
    // std::cout << std::endl;

    for (int i = 0; i < NR_GROUPS; ++i) {
        std::vector<FPArrayEntry>& local_fp_array = _local_fp_arrays[i];
        std::cout << "Group " << i << " - FP-Array size: " << local_fp_array.size() * sizeof(FPArrayEntry) / 1024.0 << " KB" << std::endl;
    }
    std::cout << std::endl;
}

void FPTree::build_k1_ele_pos() {
    for (uint32_t i = 0; i < _global_fp_array.size(); ++i) {
        const GlobalFPArrayEntry& entry = _global_fp_array[i];
        auto& node_groups = _node_to_groups[entry.node->id];
        auto [group_id, local_pos] = node_groups[i % node_groups.size()];

        _local_elepos_lists[group_id].push_back(ElePosEntry {entry.entry.item, local_pos, entry.entry.support, 0});
    }

    // for (int i = 0; i < NR_GROUPS; ++i) {
    //     std::cout << "Group " << i << " - K=1 ElePos size: " << _local_elepos_lists[i].size() * sizeof(ElePosEntry) / 1024.0 << " KB" << std::endl;
    //     for (const auto& elepos : _local_elepos_lists[i]) {
    //         std::cout << "Group " << i << " - K=1 ElePos: (" << elepos.item << ", " << elepos.pos << ", " << elepos.support << ")" << std::endl;
    //     }
    // }
    // std::cout << std::endl;
}

void FPTree::dpu_mine_candidates(dpu::DpuSet& system, const std::vector<ElePosEntry>& ele_pos, int group_id) {
    std::vector<FPArrayEntry>& fp_array = _local_fp_arrays[group_id];

    Timer::local_instance(group_id).start("Mine Freq Items - Preprocess");
    int nr_of_dpus = system.dpus().size();

    // Distribute ElePos across DPUs
    std::cout << "ElePos size: " << ele_pos.size() * sizeof(ElePosEntry) / 1024.0 << " KB, distributing across " << nr_of_dpus << " DPUs, Group ID" << group_id << std::endl;
    std::vector<std::vector<ElePosEntry>> distributed;
    int quotent = ele_pos.size() / nr_of_dpus;
    int remainder = ele_pos.size() % nr_of_dpus;
    
    int max_candidates = 0;
    std::vector<int> candidate_cnts(nr_of_dpus, 0);
    for (int i = 0; i < nr_of_dpus; ++i) {
        int start = i * quotent + std::min(i, remainder);
        int end = start + quotent + (i < remainder ? 1 : 0);
        distributed.push_back(std::vector<ElePosEntry>(ele_pos.begin() + start, ele_pos.begin() + end));

        // TODO: Consider this logic to be moved to the DPU code
        int candidate_start_idx = 0;
        for (int j = 0; j < (int)distributed.back().size(); ++j) {
            distributed.back()[j].candidate_start_idx = candidate_start_idx;
            if (distributed.back()[j].item == 0) continue;
            candidate_start_idx += fp_array[distributed.back()[j].pos].depth - 1;
        }
        candidate_cnts[i] = candidate_start_idx;
        max_candidates = std::max(max_candidates, candidate_start_idx);
    }

    std::vector<std::vector<uint32_t>> counts(nr_of_dpus, std::vector<uint32_t>(1, 0));
    for (int i = 0; i < nr_of_dpus; ++i) {
        counts[i][0] = distributed[i].size();
        int padding = distributed[0].size() - distributed[i].size();
        if (padding > 0) {
            distributed[i].resize(distributed[i].size() + padding, {0, 0, 0, 0}); // Pad with zeros
        }
    }
    Timer::local_instance(group_id).stop();

    Timer::local_instance(group_id).start("Mine Freq Items - Transfer kElePos(To DPU)");
    system.copy("k_elepos_size", counts);
    system.copy(DPU_MRAM_HEAP_POINTER_NAME, MRAM_FP_ARRAY_SZ, distributed);
    Timer::local_instance(group_id).stop();

    Timer::local_instance(group_id).start("Mine Freq Items - Exec");
    system.exec();
    Timer::local_instance(group_id).stop();

    Timer::local_instance(group_id).start("Mine Freq Items - Transfer Candidates(To CPU)");
    std::vector<std::vector<CandidateEntry>> candidates(nr_of_dpus, std::vector<CandidateEntry>(max_candidates));
    system.copy(candidates, DPU_MRAM_HEAP_POINTER_NAME, MRAM_FP_ARRAY_SZ + MRAM_FP_ELEPOS_SZ);
    Timer::local_instance(group_id).stop();

    Timer::local_instance(group_id).start("Mine Freq Items - Postprocess");
    for (int i = 0; i < nr_of_dpus; ++i) {
        for (int j = 0; j < candidate_cnts[i]; ++j) {
            const CandidateEntry& candidate = candidates[i][j];
            if (candidate.suffix_item == 0) continue; // Skip root item
            uint64_t key = (static_cast<uint64_t>(candidate.prefix_item) << 32) | candidate.suffix_item;
            _global_candidate_map.insert_or_update(key, candidate, group_id);
        }
    }
    Timer::local_instance(group_id).stop();
}

void FPTree::mine_candidates(dpu::DpuSet& system, const std::vector<ElePosEntry>& ele_pos, int group_id) {
    int nr_of_dpus = system.dpus().size();
    
    int max_elepos = (MRAM_FP_ELEPOS_SZ / sizeof(ElePosEntry)) * nr_of_dpus;
    for (int partition = 0; partition < (int)ele_pos.size(); partition += max_elepos) {
        int end = std::min(static_cast<int>(ele_pos.size()), partition + max_elepos);
        std::vector<ElePosEntry> ele_pos_partition(ele_pos.begin() + partition, ele_pos.begin() + end);

        dpu_mine_candidates(system, ele_pos_partition, group_id);
    }
}

void FPTree::allocate_dpus() {
    uint32_t dpus_per_group = NR_DPUS / NR_GROUPS;

    _dpu_systems.clear();
    _dpu_systems.resize(NR_GROUPS);

    _global_candidate_map.clear();

    _thread_running = true;

    for (int group_id = 0; group_id < NR_GROUPS; ++group_id) {
        _dpu_systems[group_id] = std::make_unique<DpuSetWrapper>(group_id, dpus_per_group);
        _dpu_systems[group_id]->get().load(DPU_MINE_CANDIDATES);
    }
}

template <class Completion>
void FPTree::mine_freq_itemsets_worker(std::barrier<Completion>& sync, int group_id) {
    dpu::DpuSet& system = _dpu_systems[group_id]->get();
    Timer::local_instance(group_id).start("Mine Freq Items - Transfer FP-Array(To DPU)");
    system.copy(DPU_MRAM_HEAP_POINTER_NAME, _local_fp_arrays[group_id]);
    Timer::local_instance(group_id).stop();

    while (true) {
        if (_local_elepos_lists[group_id].size() > 0) {
            mine_candidates(system, _local_elepos_lists[group_id], group_id);
        }

        sync.arrive_and_wait();

        if (!_thread_running) break;
    }
}

void FPTree::merge_candidates() {
    Timer::instance2().start("Mine Freq Items - Merge Results");
    for (auto& elepos_list: _local_elepos_lists) {
        elepos_list.clear();
    }

    for (const auto& local_map : _global_candidate_map.get_maps()) {
        for (const auto& [key, candidate_set] : local_map) {
            if ((int)candidate_set.get_support() >= _min_support) {
                uint32_t prefix_item = candidate_set.get_prefix_item();
                _frequent_itemsets_gt1.push_back({prefix_item, candidate_set.get_suffix_item()});
                
                uint32_t itemset_id = _itemset_id++;
                for (int i = 0; i < NR_GROUPS; ++i) {
                    for (const auto& candidate : candidate_set.candidates[i]) {
                        _local_elepos_lists[i].emplace_back(ElePosEntry {
                            itemset_id,
                            candidate.suffix_item_pos,
                            candidate.support,
                            0
                        });
                        //std::cout << "Group " << i << " - New ElePos: (" << itemset_id << ", " << candidate.suffix_item_pos << ", " << candidate.support << ")" << std::endl;
                    }
                }
            }
        }
    }
    //std::cout << std::endl;

    // for (const auto& [key, candidate_set] : _global_candidate_map.get_merged_map()) {
    //     if ((int)candidate_set.get_support() >= _min_support) {
    //         uint32_t prefix_item = candidate_set.get_prefix_item();
    //         _frequent_itemsets_gt1.push_back({prefix_item, candidate_set.get_suffix_item()});
            
    //         uint32_t itemset_id = _itemset_id++;
    //         for (int i = 0; i < NR_GROUPS; ++i) {
    //             for (const auto& candidate : candidate_set.candidates[i]) {
    //                 _local_elepos_lists[i].emplace_back(ElePosEntry {
    //                     itemset_id,
    //                     candidate.suffix_item_pos,
    //                     candidate.support,
    //                     0
    //                 });
    //                 //std::cout << "Group " << i << " - New ElePos: (" << itemset_id << ", " << candidate.suffix_item_pos << ", " << candidate.support << ")" << std::endl;
    //             }
    //         }
    //     }
    // }

    bool all_empty = true;
    for (const auto& elepos_list : _local_elepos_lists) {
        if (!elepos_list.empty()) {
            all_empty = false;
            break;
        }
    }

    if (all_empty) {
        _thread_running = false;
    }
    // _thread_running = false;
    Timer::instance2().stop();

    // for (int i = 0; i < NR_GROUPS; ++i) {
    //     std::cout << "Group " << i << " - K=1 ElePos size: " << _local_elepos_lists[i].size() * sizeof(ElePosEntry) / 1024.0 << " KB" << std::endl;
    //     for (const auto& elepos : _local_elepos_lists[i]) {
    //         std::cout << "Group " << i << " - K=1 ElePos: (" << elepos.item << ", " << elepos.pos << ", " << elepos.support << ")" << std::endl;
    //     }
    // }
    // std::cout << std::endl;

    _global_candidate_map.clear();
}

void FPTree::mine_frequent_itemsets() {
    allocate_dpus();

    std::barrier sync(NR_GROUPS, [this]() {
        merge_candidates();
    });

    Timer::instance().start("Mine Freq Items - Total");
    std::vector<std::thread> threads;
    for (int group_id = 0; group_id < NR_GROUPS; ++group_id) {
        threads.emplace_back([this, group_id, &sync]() {
            mine_freq_itemsets_worker(sync, group_id);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
    Timer::instance().stop();
}

void FPTree::delete_tree(Node* node) {
    for (Node* child : node->child) {
        delete_tree(child);
    }
    delete node;
}

void FPTree::delete_tree() {
    delete_tree(_root);
}