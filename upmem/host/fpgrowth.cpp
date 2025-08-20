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
    if (node->item == -1)
        std::cout << "(root)";
    else
        std::cout << node->item << " (" << node->count << ")";
    std::cout << std::endl;

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
                if (child->item == item) {
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

std::unordered_map<uint64_t, CandidateEntry> FPTree::dpu_mine_candidates(dpu::DpuSet& system, const std::vector<ElePosEntry>& ele_pos) {
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
        for (int j = 0; j < distributed.back().size(); ++j) {
            distributed.back()[j].candidate_start_idx = candidate_start_idx;
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
    system.copy(DPU_MRAM_HEAP_POINTER_NAME, _fp_array);
    system.copy(DPU_MRAM_HEAP_POINTER_NAME, MRAM_FP_ARRAY_SZ, distributed);
    system.exec();

    std::vector<std::vector<CandidateEntry>> candidates(nr_of_dpus, std::vector<CandidateEntry>(max_candidates));
    system.copy(candidates, DPU_MRAM_HEAP_POINTER_NAME, MRAM_FP_ARRAY_SZ + MRAM_FP_ELEPOS_SZ);

    std::unordered_map<uint64_t, CandidateEntry> candidate_map;
    for (int i = 0; i < nr_of_dpus; ++i) {
        for (int j = 0; j < candidate_cnts[i]; ++j) {
            const CandidateEntry& candidate = candidates[i][j];
            if (candidate.suffix_item == 0) continue; // Skip root item
            uint64_t key = (static_cast<uint64_t>(candidate.prefix_item) << 32) | candidate.suffix_item;
            
            if (candidate_map.find(key) == candidate_map.end()) {
                candidate_map[key] = candidate;
            } else {
                candidate_map[key].support += candidate.support;
            }
        }
    }

    return candidate_map;
}

void FPTree::mine_frequent_itemsets() {
    try {
        dpu::DpuSet system = dpu::DpuSet::allocate(NR_DPUS, DPU_CONFIG);
        system.load(DPU_MINE_CANDIDATES);
        printf("DPU program loaded.\n");
        std::vector<ElePosEntry> ele_pos = _k1_ele_pos;
        while (ele_pos.size() > 0) {
            auto candidates = std::move(dpu_mine_candidates(system, ele_pos));

            std::vector<ElePosEntry> next_ele_pos;
            for (const auto& [key, candidate] : candidates) {
                if (candidate.support >= _min_support) {
                    if (candidate.prefix_item < NR_DB_ITEMS) {
                        _frequent_itemsets_gt1.push_back({candidate.prefix_item, candidate.suffix_item});
                    } else {
                        const auto& prefix = _frequent_itemsets_gt1[candidate.prefix_item - NR_DB_ITEMS];
                        std::vector<uint32_t> itemset(prefix.begin(), prefix.end());
                        itemset.push_back(candidate.suffix_item);
                        _frequent_itemsets_gt1.push_back(itemset);
                    }

                    next_ele_pos.push_back(ElePosEntry {
                        _itemset_id++,
                        candidate.suffix_item_pos,
                        candidate.support,
                        0
                    });
                }
            }
            ele_pos = std::move(next_ele_pos);
        }
    } catch (const dpu::DpuError& e) {
        std::cerr << "DPU error: " << e.what() << std::endl;
    }
}

void FPTree::build_conditional_tree(std::vector<std::pair<std::vector<int>, int>>& pattern_base, int min_support) {
    std::map<int, int> item_count;
    for (const auto& transaction : pattern_base) {
        for (int item : transaction.first) {
            item_count[item] += transaction.second;
        }
    }

    _header_table.clear();
    for (const auto& [item, count] : item_count) {
        if (count >= min_support) {
            HeaderTableEntry entry;
            entry.item = item;
            entry.frequency = count;
            _header_table.push_back(entry);
        }
    }

    std::sort(_header_table.begin(), _header_table.end(), [](const auto& a, const auto& b) {
        return a.frequency > b.frequency;
    });


    for (const auto& [transaction, count] : pattern_base) {
        std::vector<int> filtered_items;
        for (int item : transaction) {
            if (item_count[item] >= min_support) {
                filtered_items.push_back(item);
            }
        }

        std::sort(filtered_items.begin(), filtered_items.end(), [&](int a, int b) {
            return item_count[a] > item_count[b];
        });

        Node* current_node = _root;
        for (int item : filtered_items) {
            bool found = false;
            for (Node* child : current_node->child) {
                if (child->item == item) {
                    child->count += count;
                    current_node = child;
                    found = true;
                    break;
                }
            }
            if (!found) {
                Node* new_node = new Node(item, count, current_node);
                current_node->child.push_back(new_node);
                current_node = new_node;

                auto htb_entry = std::find_if(_header_table.begin(), _header_table.end(),
                    [&](const HeaderTableEntry& entry) {
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

void FPTree::mine_pattern(std::vector<int>& prefix_path, std::vector<std::vector<int>>& frequent_itemsets) {
    for (const auto& entry : _header_table) {
        int item = entry.item;
        std::vector<int> new_prefix_path = prefix_path;
        new_prefix_path.push_back(item);
        frequent_itemsets.push_back(new_prefix_path);

        std::vector<std::pair<std::vector<int>, int>> conditional_pattern_base;

        for (Node* node : entry.node_link) {
            Node* current = node->parent;
            std::vector<int> path;
            while (current != nullptr && current->item != -1) {
                path.push_back(current->item);
                current = current->parent;
            }

            if (!path.empty()) {
                std::reverse(path.begin(), path.end());
                conditional_pattern_base.push_back({path, node->count});
            }
        }
        if (!conditional_pattern_base.empty()) {
            FPTree conditional_tree(_min_support);
            conditional_tree.build_conditional_tree(conditional_pattern_base, _min_support);
            conditional_tree.mine_pattern(new_prefix_path, frequent_itemsets);
        }
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