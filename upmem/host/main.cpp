#include"fpgrowth.h"
#include"db.hpp"

#include <iostream>

#define MIN_SUPPORT 3
#define DB_PATH "../test/test.txt"

int main() {
    Database db(DB_PATH); 

    FPTree fp_tree(MIN_SUPPORT, &db);
    fp_tree.build_tree();
    printf("Build tree completed.\n");
    fp_tree.build_fp_array();
    printf("Build FP array completed.\n");
    fp_tree.build_k1_ele_pos();
    printf("Build K1 ElePos completed.\n");
    fp_tree.mine_frequent_itemsets();
    printf("Mine frequent itemsets completed.\n");

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