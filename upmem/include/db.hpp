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
#include <dpu>

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

    void seek_to_start();
    std::vector<std::pair<int, int>> scan_for_frequent_items(int min_support);
    std::deque<std::vector<int>> filtered_items();

private:
    std::string _file_path;
    std::ifstream _file;
    int _min_support;
    std::mutex mutex;
    std::vector<int> _item_count;
    void dpu_count_items(dpu::DpuSet& system, std::vector<std::vector<int32_t>>& buffers);
    std::vector<int> dpu_filter_items(dpu::DpuSet& system, std::vector<int>& item_count);
};

#endif