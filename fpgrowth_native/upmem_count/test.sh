#!/bin/bash

UPMEM_EXE="./build/main"
INPUT_DIR="./test"
OUTPUT_DIR="./output"

echo "Build..."
make clean
make

declare -A min_supports
min_supports["DataSetA_norm.txt"]="1500"
min_supports["mushroom.dat.txt"]="3200"
min_supports["test.txt"]="2"

for input_file in "$INPUT_DIR"/*; do
    input_name=$(basename "$input_file")
    base_name="${input_name%.*}"
    ext="${input_name##*.}"
    
    # Get min_support values for this dataset
    min_support_list=${min_supports[$input_name]}
    if [ -z "$min_support_list" ]; then
        min_support_list="1500"
    fi
    
    # Test with each min_support value
    for min_support in $min_support_list; do
        echo "=========================================="
        echo "Testing $input_name with min_support=$min_support"
        echo "=========================================="
    
        echo "Running upmem C++ on $input_file with min_support=$min_support..."
        start_time=$(date +%s.%N)
        sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH "$UPMEM_EXE" "$input_file" "$min_support" "$OUTPUT_DIR/${base_name}_ms${min_support}_upmem.$ext"
        end_time=$(date +%s.%N)
        upmem_time=$(echo "$end_time - $start_time" | bc)

        echo "--- Execution Summary for $input_name (min_support=$min_support) ---"
        echo "  UpMem C++: ${upmem_time}s"
        echo "----------------------------------------"
    done
done