#ifndef DB_H
#define DB_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>
#include <optional>


#include <mutex>
#include <deque>

class Database {
public:
    Database(const std::string& file_path): _file_path(file_path) {
        _file.open(_file_path);
        if (!_file.is_open()) {
            throw std::runtime_error("Could not open file: " + _file_path);
        }
    }

    ~Database() {
        if (_file.is_open()) {
            _file.close();
        }
    }

    void seek_to_start();
    std::vector<std::pair<int, int>> scan_for_frequent_items(int min_support);
    std::deque<std::vector<int>> filtered_items();

private:
    std::unordered_map<int, int> _item_priority = {};
    std::string _file_path;
    std::ifstream _file;
    int _min_support;
    std::vector<int> _item_count;
    // DPU methods removed for CPU build
};

#endif