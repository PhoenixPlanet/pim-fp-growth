#include "fpgrowth.h"
#include <algorithm>
#include <vector>
#include <map>

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
    std::map<int32_t, int> item_idx_table;
    _fp_array.clear();

    Node* target_leaf = _leaf_head;
    while (target_leaf) {
        Node* current = target_leaf;
        _fp_array.push_back({current->item, -1, current->count, 0});
        while (current) {
            Node* parent = current->parent;
            FPArrayEntry& last_entry = _fp_array.back();
            if (item_idx_table.find(parent->item) != item_idx_table.end()) {
                last_entry.parent_index = item_idx_table[parent->item];
                break;
            }

            item_idx_table[parent->item] = _fp_array.size();
            last_entry.parent_index = item_idx_table[parent->item];

            _fp_array.push_back({parent->item, -1, parent->count, 0});
            current = parent;
        }
        target_leaf = target_leaf->next_leaf;
    }
}

void FPTree::build_k1_ele_pos() {
    _k1_ele_pos.clear();

    for (int i = 0; i < _fp_array.size(); ++i) {
        const FPArrayEntry& entry = _fp_array[i];
        _k1_ele_pos.push_back({entry.item, i, entry.support, 0});
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