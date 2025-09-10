
#include "include/fpgrowth.h"
#include "include/db.hpp"

#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: %s <data_file> <min_support>\n", argv[0]);
        return 1;
    }
    std::string db_path = argv[1];
    int min_support = std::stoi(argv[2]);
    
    Database db(db_path.c_str());
    FPTree fp_tree(min_support, &db);
    fp_tree.build_tree();
    fp_tree.build_fp_array();
    fp_tree.build_k1_ele_pos();
    fp_tree.mine_frequent_itemsets();

    for (const auto& itemset : fp_tree.get_frequent_itemsets()) {
        for (int item : itemset) {
            std::cout << item << " ";
        }
        std::cout << std::endl;
    }
    return 0;
}