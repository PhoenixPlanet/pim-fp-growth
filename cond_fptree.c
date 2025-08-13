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