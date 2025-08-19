#include "db.hpp"

#include <algorithm>
#include <vector>
#include <cstdint>
#include <thread>

#include "param.h"

#define MAX_ELEMS (MRAM_AVAILABLE / sizeof(int32_t))

std::streampos find_next_line(std::ifstream& file, std::streampos start) {
    file.seekg(start);
    char ch;
    while(file.get(ch)) {
        if(ch == '\n') {
            return file.tellg();
        }
    }
    return file.tellg();
}

std::vector<std::pair<std::streampos, std::streampos>> divide_file(std::ifstream& file, int num_parts) {
    std::vector<std::pair<std::streampos, std::streampos>> parts;
    file.seekg(0, std::ios::end);
    std::streampos end = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::streampos part_size = end / num_parts;
    std::streampos start = 0;

    for (int i = 0; i < num_parts; ++i) {
        std::streampos next_start = find_next_line(file, start + part_size);
        parts.emplace_back(start, next_start);
        start = next_start;
    }

    return parts;
}

void Database::seek_to_start() {
    _file.clear();
    _file.seekg(0, std::ios::beg);
}

void Database::dpu_count_items(dpu::DpuSet& system, std::vector<std::vector<int32_t>>& buffers) {
    uint32_t nr_of_dpus = system.dpus().size();
    std::vector<std::vector<uint32_t>> counts(nr_of_dpus, std::vector<uint32_t>(1, 0));
    std::vector<std::vector<uint32_t>> results(nr_of_dpus, std::vector<uint32_t>(NR_DB_ITEMS * NR_TASKLETS));

    for (int i = 0; i < nr_of_dpus; i++) {
        counts[i][0] = buffers[i].size();
        if (counts[i][0] % 2 == 1) {
            buffers[i].push_back(0);
        }
    }

    system.copy(DPU_MRAM_HEAP_POINTER_NAME, buffers);
    system.copy("count", counts);

    system.exec();

    system.copy(results, DPU_MRAM_HEAP_POINTER_NAME, MRAM_AVAILABLE);

    // Reduce results
    for (int i = 0; i < NR_DB_ITEMS; i++) {
        uint32_t acc = 0;
        for (int d = 0; d < nr_of_dpus; d++) {
            for (int t = 0; t < NR_TASKLETS; t++) {
                acc += results[d][NR_TASKLETS * t + i];
            }
        }
        _item_count[i] += acc;
    }
}

std::vector<std::pair<int, int>> Database::scan_for_frequent_items(int min_support) {
    _min_support = min_support;

    _item_count.resize(NR_DB_ITEMS, 0);

    std::vector<std::pair<int, int>> frequent_items;
    seek_to_start();
    
    try {
        auto system = dpu::DpuSet::allocate(NR_DPUS);
        system.load(DPU_DB_COUNT_ITEM);
        
        uint32_t nr_of_dpus = system.dpus().size();
        std::vector<std::vector<int32_t>> buffers(nr_of_dpus, std::vector<int32_t>());

        int buffer_idx = 0;
        std::string line;
        while (std::getline(_file, line)) {
            std::istringstream iss(line);
            int item;
            while (iss >> item) {
                if (buffers[buffer_idx].size() >= MAX_ELEMS) {
                    dpu_count_items(system, buffers);
                    buffer_idx = 0;
                    for (int i = 0; i < nr_of_dpus; i++) {
                        buffers[i].clear();
                    }
                }
                buffers[buffer_idx++].push_back(item);
            }
        }
        if (buffers[0].size() > 0) {
            dpu_count_items(system, buffers);
        }

        for (int i = 0; i < NR_DB_ITEMS; i++) {
            if (_item_count[i] >= _min_support) {
                std::lock_guard<std::mutex> lock(mutex);
                frequent_items.push_back({i, _item_count[i]});
            }
        }
    } catch (const dpu::DpuError & e) {
        std::cerr << e.what() << std::endl;
    }
        
    
    return frequent_items;
}

std::deque<std::vector<int>> Database::filtered_items() {

    std::vector<std::deque<std::vector<int>>> all_results(NR_THREADS);
    std::vector<std::pair<std::streampos, std::streampos>> parts = divide_file(_file, NR_THREADS);
    std::vector<std::thread> threads;

    for (int i = 0; i <NR_THREADS; i++) {
        threads.emplace_back([this, i, &parts, &all_results]() {
            _file.seekg(parts[i].first);
            std::string line;
            while (_file.tellg() < parts[i].second && std::getline(_file, line)) {
                std::istringstream iss(line);
                std::vector<int> items;
                int item;
                while (iss >> item) {
                    if (_item_count[item] >= _min_support) {
                        items.push_back(item);
                    }
                }
                if (!items.empty()) {
                    std::sort(items.begin(), items.end(), [this](int a, int b) {
                        return _item_count[a] > _item_count[b];
                    });
                    all_results[i].push_back(items);
                }
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    std::deque<std::vector<int>> results;
    for (const auto& result : all_results) {
        results.insert(results.end(), result.begin(), result.end());
    }
    return results;
}