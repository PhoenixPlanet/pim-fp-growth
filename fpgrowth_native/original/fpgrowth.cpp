#include "fpgrowth.h"
#include <algorithm>
#include <climits>

#include "timer.h"

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
    Timer::instance().start("Scan for freq items");
    std::vector<std::pair<int, int>> frequent_items = _db->scan_for_frequent_items(_min_support);
    Timer::instance().stop();

    Timer::instance().start("Build FP-Tree");
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
    Timer::instance().stop();

    Timer::instance().start("Build FP-Tree - Filter & Sort");
    std::vector<std::vector<int>> transactions = _db->get_all_filtered_transactions();
    Timer::instance().stop();

    Timer::instance().start("Build FP-Tree");
    for (const auto& items : transactions) {
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
    Timer::instance().stop();

    // count the number of nodes in the tree
    // int node_count = 0;
    // std::function<void(Node*)> count_nodes = [&](Node* node) {
    //     node_count++;
    //     for (Node* child : node->child) {
    //         count_nodes(child);
    //     }
    // };
    // count_nodes(_root);
    // std::cout << "FP-Tree has " << node_count << " nodes." << std::endl;

    // find the max depth of the tree
    // int max_depth = 0;
    // std::function<void(Node*, int)> find_max_depth = [&](Node* node, int depth) {
    //     if (node->child.empty()) {
    //         if (depth > max_depth) {
    //             max_depth = depth;
    //         }
    //     }
    //     for (Node* child : node->child) {
    //         find_max_depth(child, depth + 1);
    //     }
    // };
    // find_max_depth(_root, 1);
    // std::cout << "FP-Tree max depth: " << max_depth << std::endl;
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
    bool is_single_path = true;
    Node* current = _root;
    while (!current->child.empty()) {
        if (current->child.size() > 1) {
            is_single_path = false;
            break;
        }
        current = current->child.front();
    }

    if (is_single_path) {
        std::vector<int> single_path;
        std::vector<int> single_path_counts;  // 추가: 각 노드의 count 저장
        Node* node = _root;
        while (!node->child.empty()) {
            node = node->child.front();
            single_path.push_back(node->item);
            single_path_counts.push_back(node->count);  // count 저장
        }

        int path_size = single_path.size();
        int combinations = (1 << path_size) - 1;
        for (int i = 1; i <= combinations; ++i) {
            std::vector<int> new_pattern = prefix_path;
            int min_count = INT_MAX;  // 조합의 최소 지원도
            
            for (int j = 0; j < path_size; ++j) {
                if (i & (1 << j)) {
                    new_pattern.push_back(single_path[j]);
                    min_count = std::min(min_count, single_path_counts[j]);
                }
            }
            
            // 최소 지원도를 만족하는 경우만 추가
            if (min_count >= _min_support) {
                frequent_itemsets.push_back(new_pattern);
            }
        }
        return;
    }

    for (auto it = _header_table.rbegin(); it != _header_table.rend(); ++it) {
        const HeaderTableEntry& entry = *it;

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