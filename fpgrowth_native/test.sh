#!/bin/bash
set -e

# Paths
INPUT_DIR="input"
OUTPUT_DIR="output"
ORIGINAL_EXE="original/fpgrowth"
PFP_EXE="pfp_growth/fpgrowth_cpu"
UPMEM_EXE="upmem/build/main"
PY_SCRIPT="python/fp_growth.py"
VENV_DIR="python/venv"

# Ensure output directory exists
mkdir -p "$OUTPUT_DIR"

# Activate Python venv
source "$VENV_DIR/bin/activate"

# Define min_support values for different datasets
declare -A min_supports
min_supports["DataSetA_norm.txt"]=1500
min_supports["mushroom.dat.txt"]=3200
min_supports["test.txt"]=2

for input_file in "$INPUT_DIR"/*; do
    input_name=$(basename "$input_file")
    base_name="${input_name%.*}"
    ext="${input_name##*.}"
    
    # Get min_support for this dataset
    min_support=${min_supports[$input_name]}
    if [ -z "$min_support" ]; then
        echo "Warning: No min_support defined for $input_name, using default 1500"
        min_support=1500
    fi
    
    echo "Running original C++ on $input_file with min_support=$min_support..."
    "$ORIGINAL_EXE" "$input_file" "$min_support" > "$OUTPUT_DIR/${base_name}_org.$ext"
    echo "Running pfp_growth C++ on $input_file with min_support=$min_support..."
    "$PFP_EXE" "$input_file" "$min_support" > "$OUTPUT_DIR/${base_name}_pfp.$ext"
    echo "Running upmem C++ on $input_file with min_support=$min_support..."
    "$UPMEM_EXE" "$input_file" "$min_support" > "$OUTPUT_DIR/${base_name}_upmem.$ext"
    echo "Running Python on $input_file with min_support=$min_support..."
    python3 "$PY_SCRIPT" "$input_file" "$min_support" > "$OUTPUT_DIR/${base_name}_py.$ext"
done
