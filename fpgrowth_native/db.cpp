#include "db.h"

#include <algorithm>

void Database::seek_to_start() {
    _file.clear();
    _file.seekg(0, std::ios::beg);
}

std::vector<std::pair<int, int>> Database::scan_for_frequent_items(int min_support) {
    std::string line;

    _min_support = min_support;

    seek_to_start();
    _item_count.clear();

    while (std::getline(_file, line)) {
        std::istringstream iss(line);
        int item;
        while (iss >> item) {
            _item_count[item]++;
        }
    }

    std::vector<std::pair<int, int>> frequent_items;
    for (const auto& pair: _item_count) {
        if (pair.second >= min_support) {
            frequent_items.push_back(pair);
        }
    }

    return frequent_items;
}

std::optional<std::vector<int>> Database::filtered_items() {
    std::string line;

    if (std::getline(_file, line)) {
        std::istringstream iss(line);
        std::vector<int> items;
        int item;
        while (iss >> item) {
            if (_item_count.find(item) != _item_count.end() && _item_count[item] >= _min_support) {
                items.push_back(item);
            }
        }

        std::sort(items.begin(), items.end(), [this](int a, int b) {
            return _item_count[a] > _item_count[b];
        });

        return items;
    } else {
        return std::nullopt;
    }
}