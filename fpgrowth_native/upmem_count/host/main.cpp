#include"fpgrowth.h"
#include"db.hpp"

#include <iostream>
#include <filesystem>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 4) {
        printf("Usage: %s <data_file> <min_support> <output_file>\n", argv[0]);
        return 1;
    }
    std::string db_path = argv[1];
    int min_support = std::stoi(argv[2]);
    std::string output_file = argv[3];

    std::string dpu_db_count_path = "./build/db_count_item";
    std::string dpu_mine_candidates_path = "./build/mine_candidates";
    
    setenv("DPU_DB_COUNT_ITEM_PATH", dpu_db_count_path.c_str(), 1);
    setenv("DPU_MINE_CANDIDATES_PATH", dpu_mine_candidates_path.c_str(), 1);
    
    Database db(db_path.c_str());
    std::vector<std::pair<int, int>> frequent_items = db.scan_for_frequent_items(min_support);

    // std::vector<std::vector<int>> frequent_itemsets;
    // std::vector<int> prefix_path;
    // fp_tree.mine_pattern(prefix_path, frequent_itemsets);

    // make output file
    std::ofstream output(output_file);
    for (const auto& itemset : frequent_items) {
        output << itemset.first << " " << itemset.second << std::endl;
    }

    return 0;
}