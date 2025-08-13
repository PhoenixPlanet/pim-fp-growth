#include"fpgrowth.h"
#include"db.h"

#include <stdio.h>

#define MIN_SUPPORT 3
#define DB_PATH "data.txt"

int main() {

    FILE* file = fopen(DB_PATH, "r");
    if (!file) {
        fprintf(stderr, "Could not open file: %s\n", DB_PATH);
        return 1;
    }
    

    FPTree fp_tree(MIN_SUPPORT, &db);
    fp_tree.build_tree();

    std::vector<std::vector<int>> frequent_itemsets;
    std::vector<int> prefix_path;
    fp_tree.mine_pattern(prefix_path, frequent_itemsets);

    for (const auto& itemset : frequent_itemsets) {
        for (int item : itemset) {
            std::cout << item << " ";
        }
        std::cout << std::endl;
    }

    return 0;
}