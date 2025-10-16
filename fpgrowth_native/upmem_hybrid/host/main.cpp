#include"fpgrowth.h"
#include"db.hpp"

#include <iostream>
#include <filesystem>
#include <string>

#include "timer.h"

int main(int argc, char* argv[]) {
    if (argc < 4) {
        printf("Usage: %s <data_file> <min_support> <output_file>\n", argv[0]);
        return 1;
    }
    std::string db_path = argv[1];
    int min_support = std::stoi(argv[2]);
    std::string output_file = argv[3];

    std::filesystem::path exe_path = std::filesystem::canonical(argv[0]);
    std::filesystem::path upmem_dir = exe_path.parent_path().parent_path(); // Go up from build/ to upmem/
    std::string dpu_db_count_path = (upmem_dir / "build" / "db_count_item").string();
    std::string dpu_mine_candidates_path = (upmem_dir / "build" / "mine_candidates").string();
    
    setenv("DPU_DB_COUNT_ITEM_PATH", dpu_db_count_path.c_str(), 1);
    setenv("DPU_MINE_CANDIDATES_PATH", dpu_mine_candidates_path.c_str(), 1);
    
    Database db(db_path.c_str()); 

    FPTree fp_tree(min_support, &db);

    //Timer::instance().start("Build FP-Tree");
    fp_tree.build_tree();
    //Timer::instance().stop();

    Timer::instance().start("Build FP-Array");
    fp_tree.build_fp_array();
    Timer::instance().stop();

    Timer::instance().start("Build K1 ElePos");
    fp_tree.build_k1_ele_pos();
    Timer::instance().stop();
    
    fp_tree.mine_frequent_itemsets();

    // std::vector<std::vector<int>> frequent_itemsets;
    // std::vector<int> prefix_path;
    // fp_tree.mine_pattern(prefix_path, frequent_itemsets);

    // make output file
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
    Timer::instance2().print_records();

    for (int group_id = 0; group_id < NR_GROUPS; ++group_id) {
        std::cout << "Group " << group_id << " Timers -----------------" << std::endl;
        Timer::local_instance(group_id).print_records();
    }
    std::cout << std::endl;

    return 0;
}