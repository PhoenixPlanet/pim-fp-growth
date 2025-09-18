#include "fpgrowth.h"
#include <algorithm>
#include <vector>
#include <map>
#include <functional>
#include <unordered_map>
#include <utility>

#include "param.h"

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
    std::sort(frequent_items.begin(), frequent_items.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    _frequent_itemsets_1.clear();
    for (const auto& item : frequent_items) {
        _frequent_itemsets_1.push_back({static_cast<uint32_t>(item.first)});
    }

    _header_table.clear();
    for (const auto& item : frequent_items) {
        HeaderTableEntry entry;
        entry.item = item.first;
        entry.frequency = item.second;
        _header_table.push_back(entry);
    }

    _db->seek_to_start();
    _leaf_head = nullptr;
    std::deque<std::vector<int>> items_list = _db->filtered_items();
    while (items_list.size() > 0) {
        std::vector<int> items = items_list.front();
        items_list.pop_front();

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
                Node* new_node = new Node(item, 1, current_node);
                
                if (current_node != _root && current_node->child.empty()) {
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
                }

                if (_leaf_head == nullptr) {
                    _leaf_head = new_node;
                    new_node->next_leaf = nullptr;
                    new_node->prev_leaf = nullptr;
                } else {
                    new_node->prev_leaf = nullptr;
                    new_node->next_leaf = _leaf_head;
                    _leaf_head->prev_leaf = new_node;
                    _leaf_head = new_node;
                }

                current_node->child.push_back(new_node);
                current_node = current_node->child.back();
                auto htb_entry = std::find_if(_header_table.begin(), _header_table.end(), [&item](const HeaderTableEntry& entry) {
                    return entry.item == item;
                });
                if (htb_entry != _header_table.end()) {
                    htb_entry->node_link.push_back(new_node);
                } else {
                    std::cerr << "Error: Item not found in header table: " << item << std::endl;
                }
            }
        }
    }
}

void FPTree::build_fp_array() {
    std::map<Node*, int> item_idx_table;
    _fp_array.clear();

    Node* target_leaf = _leaf_head;
    while (target_leaf) {
        Node* current = target_leaf;
        _fp_array.push_back(FPArrayEntry {current->item, -1, current->count, current->depth});
        while (current) {
            if (current->item == 0) break;

            Node* parent = current->parent;
            FPArrayEntry& last_entry = _fp_array.back();
            if (item_idx_table.find(parent) != item_idx_table.end()) {
                last_entry.parent_pos = item_idx_table[parent];
                break;
            }

            item_idx_table[parent] = _fp_array.size();
            last_entry.parent_pos = item_idx_table[parent];

            _fp_array.push_back({parent->item, -1, parent->count, parent->depth});
            current = parent;
        }
        target_leaf = target_leaf->next_leaf;
    }
}

void FPTree::build_k1_ele_pos() {
    _k1_ele_pos.clear();

    for (uint32_t i = 0; i < _fp_array.size(); ++i) {
        const FPArrayEntry& entry = _fp_array[i];
        _k1_ele_pos.push_back(ElePosEntry {entry.item, i, entry.support, 0});
    }
}

void FPTree::dpu_mine_candidates(dpu::DpuSet& system, const std::vector<ElePosEntry>& ele_pos, 
                                 std::unordered_map<uint64_t, TempCandidates>& candidate_map) {
    int nr_of_dpus = system.dpus().size();

    // Distribute ElePos across DPUs
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
            candidate_start_idx += _fp_array[distributed.back()[j].pos].depth - 1;
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

    system.copy("k_elepos_size", counts);
    system.copy(DPU_MRAM_HEAP_POINTER_NAME, MRAM_FP_ARRAY_SZ, distributed);
    system.exec();

    std::vector<std::vector<CandidateEntry>> candidates(nr_of_dpus, std::vector<CandidateEntry>(max_candidates));
    system.copy(candidates, DPU_MRAM_HEAP_POINTER_NAME, MRAM_FP_ARRAY_SZ + MRAM_FP_ELEPOS_SZ);

    for (int i = 0; i < nr_of_dpus; ++i) {
        for (int j = 0; j < candidate_cnts[i]; ++j) {
            const CandidateEntry& candidate = candidates[i][j];
            if (candidate.suffix_item == 0) continue; // Skip root item
            uint64_t key = (static_cast<uint64_t>(candidate.prefix_item) << 32) | candidate.suffix_item;
            
            if (candidate_map.find(key) == candidate_map.end()) {
                candidate_map[key] = TempCandidates(candidate);
            } else {
                candidate_map[key].add_candidate(candidate);
            }
        }
    }
}

std::unordered_map<uint64_t, TempCandidates> FPTree::mine_candidates(dpu::DpuSet& system, const std::vector<ElePosEntry>& ele_pos) {
    int nr_of_dpus = system.dpus().size();
    std::unordered_map<uint64_t, TempCandidates> candidate_map;
    
    int max_elepos = (MRAM_FP_ELEPOS_SZ / sizeof(ElePosEntry)) * nr_of_dpus;
    for (int partition = 0; partition < (int)ele_pos.size(); partition += max_elepos) {
        int end = std::min(static_cast<int>(ele_pos.size()), partition + max_elepos);
        std::vector<ElePosEntry> ele_pos_partition(ele_pos.begin() + partition, ele_pos.begin() + end);

        dpu_mine_candidates(system, ele_pos_partition, candidate_map);
    }

    return candidate_map;
}

void FPTree::mine_frequent_itemsets() {
    try {
        dpu::DpuSet system = dpu::DpuSet::allocate(NR_DPUS, DPU_CONFIG);
        system.load(DPU_MINE_CANDIDATES);

        system.copy(DPU_MRAM_HEAP_POINTER_NAME, _fp_array);
        
        std::vector<ElePosEntry> ele_pos = _k1_ele_pos;
        while (ele_pos.size() > 0) {
            auto candidates = std::move(mine_candidates(system, ele_pos));

            std::vector<ElePosEntry> next_ele_pos;
            for (const auto& [key, candidate_set] : candidates) {
                if ((int)candidate_set.get_support() >= _min_support) {
                    if (candidate_set.get_prefix_item() < NR_DB_ITEMS) {
                        _frequent_itemsets_gt1.push_back({candidate_set.get_prefix_item(), candidate_set.get_suffix_item()});
                    } else {
                        const auto& prefix = _frequent_itemsets_gt1[candidate_set.get_prefix_item() - NR_DB_ITEMS];
                        std::vector<uint32_t> itemset(prefix.begin(), prefix.end());
                        itemset.push_back(candidate_set.get_suffix_item());
                        _frequent_itemsets_gt1.push_back(itemset);
                    }
                    
                    uint32_t itemset_id = _itemset_id++;
                    for (const auto& candidate : candidate_set.candidates) {
                        next_ele_pos.push_back(ElePosEntry {
                            itemset_id,
                            candidate.suffix_item_pos,
                            candidate.support,
                            0
                        });
                    }
                }
            }
            ele_pos = std::move(next_ele_pos);
        }
    } catch (const dpu::DpuError& e) {
        std::cerr << "DPU error: " << e.what() << std::endl;
    }
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