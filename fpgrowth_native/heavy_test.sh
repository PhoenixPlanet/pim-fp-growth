#!/bin/bash
set -e

# Paths
INPUT_DIR="heavy_input"
OUTPUT_DIR="heavy_output"
ORIGINAL_EXE="original/fpgrowth"
PFP_EXE="pfp_growth/fpgrowth_cpu"
UPMEM_EXE="upmem_hist/build/main"
HYBRID_EXE="upmem_hybrid/build/main"

# Ensure output directory exists
mkdir -p "$OUTPUT_DIR"

# Build all projects
echo "Building original C++ project..."
cd original
make clean
make
cd ..

echo "Building pfp_growth C++ project..."
cd pfp_growth
make clean
make
cd ..

echo "Building upmem C++ project..."
cd upmem_hist
make clean
make
cd ..

# Define min_support values for different datasets
declare -A min_supports
min_supports["mushroom.dat.txt"]="3200 2500 1600"
min_supports["connect.txt"]="50000 52000 54000 56000 58000"
min_supports["T40I10D100K.dat"]="1400"

echo "Starting main test phase..."
echo "=========================================="

for dataset in "${!min_supports[@]}"; do
    input_file="$INPUT_DIR/$dataset"
    base_name="${dataset%.*}"
    
    # Get min_support values for this dataset
    min_support_list=${min_supports[$dataset]}

    # Test with each min_support value
    for min_support in $min_support_list; do
        echo "=========================================="
        echo "Testing $basename with min_support=$min_support"
        echo "=========================================="
    
        echo "Running original C++ on $input_file with min_support=$min_support..."
        start_time=$(date +%s.%N)
        sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH "$ORIGINAL_EXE" "$input_file" "$min_support" "$OUTPUT_DIR/${base_name}_ms${min_support}_org.txt"
        end_time=$(date +%s.%N)
        original_time=$(echo "$end_time - $start_time" | bc)
        
        echo "Running pfp_growth C++ on $input_file with min_support=$min_support..."
        start_time=$(date +%s.%N)
        sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH "$PFP_EXE" "$input_file" "$min_support" "$OUTPUT_DIR/${base_name}_ms${min_support}_pfp.txt"
        end_time=$(date +%s.%N)
        pfp_time=$(echo "$end_time - $start_time" | bc)
        
        echo "Running upmem C++ on $input_file with min_support=$min_support..."
        start_time=$(date +%s.%N)
        sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH "$UPMEM_EXE" "$input_file" "$min_support" "$OUTPUT_DIR/${base_name}_ms${min_support}_upmem.txt"
        end_time=$(date +%s.%N)
        upmem_time=$(echo "$end_time - $start_time" | bc)

        echo "Running hybrid C++ on $input_file with min_support=$min_support..."
        start_time=$(date +%s.%N)
        sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH "$HYBRID_EXE" "$input_file" "$min_support" "$OUTPUT_DIR/${base_name}_ms${min_support}_hybrid.txt"
        end_time=$(date +%s.%N)
        hybrid_time=$(echo "$end_time - $start_time" | bc)
        
        echo "--- Execution Summary for $input_name (min_support=$min_support) ---"
        echo "  Original C++: ${original_time}s"
        echo "  PFP Growth C++: ${pfp_time}s"
        echo "  UpMem C++: ${upmem_time}s"
        echo "  Hybrid C++: ${hybrid_time}s"
        echo "----------------------------------------"
    done
done