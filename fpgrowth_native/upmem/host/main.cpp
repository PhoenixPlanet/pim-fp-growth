#include"fpgrowth.h"
#include"db.hpp"

#include <iostream>
#include <filesystem>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: %s <data_file> <min_support>\n", argv[0]);
        return 1;
    }
    std::string db_path = argv[1];
    int min_support = std::stoi(argv[2]);

    std::filesystem::path exe_path = std::filesystem::canonical(argv[0]);
    std::filesystem::path upmem_dir = exe_path.parent_path().parent_path(); // Go up from build/ to upmem/
    std::string dpu_db_count_path = (upmem_dir / "build" / "db_count_item").string();
    std::string dpu_mine_candidates_path = (upmem_dir / "build" / "mine_candidates").string();
    
    setenv("DPU_DB_COUNT_ITEM_PATH", dpu_db_count_path.c_str(), 1);
    setenv("DPU_MINE_CANDIDATES_PATH", dpu_mine_candidates_path.c_str(), 1);
    
    Database db(db_path.c_str()); 

    FPTree fp_tree(min_support, &db);
    fp_tree.build_tree();
    fp_tree.build_fp_array();
    fp_tree.build_k1_ele_pos();
    fp_tree.mine_frequent_itemsets();

    // std::vector<std::vector<int>> frequent_itemsets;
    // std::vector<int> prefix_path;
    // fp_tree.mine_pattern(prefix_path, frequent_itemsets);

    for (const auto& itemset : fp_tree.get_frequent_itemsets()) {
        for (int item : itemset) {
            std::cout << item << " ";
        }
        std::cout << std::endl;
    }

    return 0;
}