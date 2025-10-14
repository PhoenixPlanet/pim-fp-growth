#include "fpgrowth.h"
#include "db.h"
#include <iostream>
#include <vector>
#include <string>

#include "timer.h"

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cout << "Usage: " << argv[0] << " <data_file> <min_support> <output_file>" << std::endl;
        return 1;
    }
    std::string db_path = argv[1];
    int min_support = std::stoi(argv[2]);
    Database db(db_path.c_str());

    FPTree fp_tree(min_support, &db);
    fp_tree.build_tree();

    std::vector<std::vector<int>> frequent_itemsets;
    std::vector<int> prefix_path;

    Timer::instance().start("Mine Patterns");
    fp_tree.mine_pattern(prefix_path, frequent_itemsets);
    Timer::instance().stop();

    std::string output_file = argv[3];
    std::ofstream output(output_file);
    for (const auto& itemset : frequent_itemsets) {
        for (int item : itemset) {
            output << item << " ";
        }
        output << std::endl;
    }

    Timer::instance().print_records();

    return 0;
}