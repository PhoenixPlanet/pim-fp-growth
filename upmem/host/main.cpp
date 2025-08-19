#include"fpgrowth.h"
#include"db.hpp"

#include <iostream>

#define MIN_SUPPORT 3
#define DB_PATH "data.txt"

int main() {
    Database db(DB_PATH); 

    FPTree fp_tree(MIN_SUPPORT, &db);
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