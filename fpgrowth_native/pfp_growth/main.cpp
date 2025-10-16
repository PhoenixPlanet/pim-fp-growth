
#include "include/fpgrowth.h"
#include "include/db.hpp"
#include "include/timer.h"

#include <iostream>
#include <functional>

int main(int argc, char* argv[]) {
    if (argc < 4) {
        printf("Usage: %s <data_file> <min_support> <output_file>\n", argv[0]);
        return 1;
    }
    std::string db_path = argv[1];
    int min_support = std::stoi(argv[2]);
    std::string output_file = argv[3];
    Database db(db_path.c_str());
    FPTree fp_tree(min_support, &db);
    fp_tree.build_tree();

    Timer::instance().start("Build FP-Array");
    fp_tree.build_fp_array();
    Timer::instance().stop();

    Timer::instance().start("Build K1 ElePos");
    fp_tree.build_k1_ele_pos();
    Timer::instance().stop();

    fp_tree.mine_frequent_itemsets();

    std::ofstream output(output_file);
    std::function<void(uint32_t)> get_prefix = [&output, &fp_tree, &get_prefix](uint32_t item) {
        if (item < NR_DB_ITEMS) {
            output << item << " ";
        } else {
            const auto& prefix = fp_tree.get_frequent_itemsets_gt1()[item - NR_DB_ITEMS];
            get_prefix(prefix.first);
            if (prefix.second != (uint32_t)-1)
                output << prefix.second << " ";
        }
    };

    for (const auto& itemset : fp_tree.get_frequent_itemsets()) {
        auto [first, second] = itemset;
        if (first < NR_DB_ITEMS) {
            output << first;
            if (second != (uint32_t)-1)
                output << " " << second;
            output << std::endl;
        } else {
            get_prefix(first);
            output << second << std::endl;
        }
    }

    Timer::instance().print_records();
    
    return 0;
}