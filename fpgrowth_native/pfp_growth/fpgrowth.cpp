#include "include/fpgrowth.h"
#include <algorithm>
#include <thread>
#include "include/mine_candidates_cpu.h"
#include <vector>
#include <map>
#include <functional>
#include <unordered_map>
#include <utility>

#include "include/param.h"

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

    // Group by item and aggregate support
    std::unordered_map<uint32_t, uint32_t> item_support;
    std::unordered_map<uint32_t, uint32_t> item_first_pos;
    
    for (uint32_t i = 0; i < _fp_array.size(); ++i) {
        const FPArrayEntry& entry = _fp_array[i];
        if (entry.item == 0) continue; // Skip root nodes
        
        if (item_support.find(entry.item) == item_support.end()) {
            item_first_pos[entry.item] = i;
            item_support[entry.item] = 0;
        }
        item_support[entry.item] += entry.support;
    }
    
    // Create K1 elements only for items that meet min_support
    for (const auto& [item, total_support] : item_support) {
        if (total_support >= _min_support) {
            _k1_ele_pos.push_back(ElePosEntry {
                item, 
                item_first_pos[item], 
                total_support, 
                0
            });
        }
    }
}

void FPTree::cpu_mine_candidates(const std::vector<ElePosEntry>& ele_pos, std::unordered_map<uint64_t, CandidateEntry>& candidate_map) {
    int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;
    std::vector<CandidateEntry> candidates;
    ::cpu_mine_candidates(ele_pos, _fp_array, candidates, num_threads);
    for (const auto& candidate : candidates) {
        if (candidate.suffix_item == 0) continue; // Skip root item
        uint64_t key = (static_cast<uint64_t>(candidate.prefix_item) << 32) | candidate.suffix_item;
        if (candidate_map.find(key) == candidate_map.end()) {
            candidate_map[key] = candidate;
        } else {
            candidate_map[key].support += candidate.support;
        }
    }
}

std::unordered_map<uint64_t, CandidateEntry> FPTree::mine_candidates(const std::vector<ElePosEntry>& ele_pos) {
    std::unordered_map<uint64_t, CandidateEntry> candidate_map;
    cpu_mine_candidates(ele_pos, candidate_map);
    return candidate_map;
}

void FPTree::mine_frequent_itemsets() {
    // Use traditional FP-Growth approach - mine patterns recursively
    std::vector<std::vector<int>> all_frequent_itemsets;
    std::vector<int> empty_prefix;
    mine_pattern(empty_prefix, all_frequent_itemsets);
    
    // Convert to the expected format and add to _frequent_itemsets_gt1
    for (const auto& itemset : all_frequent_itemsets) {
        std::vector<uint32_t> uint_itemset;
        for (int item : itemset) {
            uint_itemset.push_back(static_cast<uint32_t>(item));
        }
        _frequent_itemsets_gt1.push_back(uint_itemset);
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
            while (current != nullptr && current->item != 0) {  // Fixed: root item is 0, not -1
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
            if (!conditional_tree._header_table.empty()) {  // Only mine if there are frequent items
                conditional_tree.mine_pattern(new_prefix_path, frequent_itemsets);
            }
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