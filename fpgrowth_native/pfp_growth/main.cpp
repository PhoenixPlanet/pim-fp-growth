
#include "include/fpgrowth.h"
#include "include/db.hpp"

#include <iostream>

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
    fp_tree.build_fp_array();
    fp_tree.build_k1_ele_pos();
    fp_tree.mine_frequent_itemsets();

    std::ofstream ofs(output_file);
    for (const auto& itemset : fp_tree.get_frequent_itemsets()) {
        for (int item : itemset) {
            ofs << item << " ";
        }
        ofs << std::endl;
    }
    return 0;
}