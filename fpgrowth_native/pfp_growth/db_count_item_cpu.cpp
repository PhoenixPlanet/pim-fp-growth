#include <vector>
#include <thread>
#include <cstdint>
#include <mutex>
#include <algorithm>
#include "include/param.h"
#include "include/db_count_item_cpu.h"

// CPU version of DPU db_count_item.c
// Parallel histogram counting using std::thread
void cpu_count_items(const std::vector<int32_t>& buffer, std::vector<uint32_t>& histogram, uint32_t count, int num_threads) {
    std::vector<std::vector<uint32_t>> local_hists(num_threads, std::vector<uint32_t>(NR_DB_ITEMS, 0));
    uint32_t stride = (count + num_threads - 1) / num_threads;
    auto worker = [&](int tid) {
        uint32_t start = tid * stride;
        uint32_t end = std::min(count, start + stride);
        for (uint32_t i = start; i < end; ++i) {
            int32_t item = buffer[i];
            if (item >= 0 && item < NR_DB_ITEMS) {
                local_hists[tid][item]++;
            }
        }
    };
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back(worker, t);
    }
    for (auto& th : threads) th.join();
    // Reduce local histograms
    std::fill(histogram.begin(), histogram.end(), 0);
    for (int t = 0; t < num_threads; ++t) {
        for (int i = 0; i < NR_DB_ITEMS; ++i) {
            histogram[i] += local_hists[t][i];
        }
    }
}
