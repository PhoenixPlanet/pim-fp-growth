#ifndef DB_COUNT_ITEM_CPU_H
#define DB_COUNT_ITEM_CPU_H

#include <vector>
#include <cstdint>

void cpu_count_items(const std::vector<int32_t>& buffer, std::vector<uint32_t>& histogram, uint32_t count, int num_threads);

#endif
