#include "db.hpp"

#include <algorithm>
#include <vector>
#include <cstdint>
#include <thread>

#include "param.h"

#define MAX_ELEMS (MRAM_TRX_ARRAY_SZ / sizeof(int32_t))

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
    if (num_parts <= 0) {
        return {};
    }

    std::vector<std::pair<std::streampos, std::streampos>> parts;
    file.seekg(0, std::ios::end);
    std::streampos end = file.tellg();
    file.seekg(0, std::ios::beg);

    if (end == 0) {
        return {};
    }

    if (num_parts == 1) {
        parts.emplace_back(0, end);
        return parts;
    }

    std::streampos part_size = end / num_parts;
    std::streampos current_start = 0;

    for (int i = 0; i < num_parts - 1; ++i) {
        std::streampos ideal_split_point = (i + 1) * part_size;
        
        if (ideal_split_point >= end) {
            break; 
        }

        std::streampos next_start = find_next_line(file, ideal_split_point);
        
        if (next_start > current_start && next_start < end) {
            parts.emplace_back(current_start, next_start);
            current_start = next_start;
        } else {
            break;
        }
    }

    if (current_start < end) {
        parts.emplace_back(current_start, end);
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
    std::vector<std::vector<uint32_t>> results(nr_of_dpus, std::vector<uint32_t>(NR_DB_ITEMS, 0));

    for (uint32_t i = 0; i < nr_of_dpus; i++) {
        counts[i][0] = buffers[i].size();
        if (i == 0) {
            if (counts[0][0] % 2 == 1) {
                buffers[0].push_back(0); // Ensure even number of elements for DPU processing
            }
        } else {
            int padding = buffers[0].size() - buffers[i].size();
            if (padding > 0) {
                buffers[i].resize(buffers[i].size() + padding, 0); // Pad with zeros
            } 
        }
    }

    system.copy(DPU_MRAM_HEAP_POINTER_NAME, buffers);
    system.copy("count", counts);

    system.exec();

    system.copy(results, DPU_MRAM_HEAP_POINTER_NAME, MRAM_TRX_ARRAY_SZ);

    // Reduce results
    for (uint32_t d = 0; d < nr_of_dpus; d++) {
        for (uint32_t i = 0; i < NR_DB_ITEMS; i++) {
            _item_count[i] += results[d][i];
        }
    }
}

std::vector<std::pair<int, int>> Database::scan_for_frequent_items(int min_support) {
    _min_support = min_support;

    _item_count.resize(NR_DB_ITEMS, 0);

    std::vector<std::pair<int, int>> frequent_items;
    seek_to_start();
    
    try {
        auto system = dpu::DpuSet::allocate(NR_DPUS, DPU_CONFIG);
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
                    for (uint32_t i = 0; i < nr_of_dpus; i++) {
                        buffers[i].clear();
                    }
                }
                buffers[buffer_idx].push_back(item);
                buffer_idx = (buffer_idx + 1) % nr_of_dpus;
            }
        }
        if (buffers[0].size() > 0) {
            dpu_count_items(system, buffers);
        }

        for (int i = 0; i < NR_DB_ITEMS; i++) {
            if (_item_count[i] >= _min_support) {
                frequent_items.push_back({i, _item_count[i]});
            }
        }
    } catch (const dpu::DpuError & e) {
        std::cerr << e.what() << std::endl;
    }

    std::sort(frequent_items.begin(), frequent_items.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    for (int i = 0; i < (int)frequent_items.size(); i++) {
        _item_priority[frequent_items[i].first] = frequent_items.size() - i;
    }
    
    return frequent_items;
}

std::deque<std::vector<int>> Database::filtered_items() {

    std::vector<std::deque<std::vector<int>>> all_results(NR_THREADS);
    std::vector<std::pair<std::streampos, std::streampos>> parts = divide_file(_file, NR_THREADS);
    std::vector<std::thread> threads;

    for (int i = 0; i < NR_THREADS; i++) {
        threads.emplace_back([this, i, &parts, &all_results]() {
            std::ifstream thread_file(_file_path); // Open a separate file stream for each thread
            thread_file.seekg(parts[i].first);
            std::string line;
            while (thread_file.tellg() < parts[i].second && std::getline(thread_file, line)) {
                //if(thread_file.eof()) break; // Check for end of file
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
                        return _item_priority[a] > _item_priority[b];
                    });
                    all_results[i].push_back(items);
                }
            }
        });
    }
    for (auto& t : threads) t.join();
    // Combine results from all threads
    std::deque<std::vector<int>> results;
    for (const auto& result : all_results) {
        results.insert(results.end(), result.begin(), result.end());
    }

    #ifdef PRINT
    for (auto& items : results) {
        printf("Filtered items: ");
        for (int item : items) {
            printf("%d ", item);
        }
        printf("\n");
    }
    #endif
    return results;
}