#ifndef DB_H
#define DB_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <utility>
#include <optional>

class Database {
public:
    Database(const std::string& file_path): _file_path(file_path) {
        _file.open(_file_path);
        if (!_file.is_open()) {
            throw std::runtime_error("Could not open file: " + _file_path);
        }
    }

    void seek_to_start();
    std::vector<std::pair<int, int>> scan_for_frequent_items(int min_support);
    std::optional<std::vector<int>> filtered_items();

private:
    std::string _file_path;
    std::ifstream _file;
    int _min_support;
    std::map<int, int> _item_count;
};

#endif