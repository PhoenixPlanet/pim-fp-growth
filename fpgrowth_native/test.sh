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

# Check Python3 installation
echo "Checking Python3 installation..."
if ! command -v python3 &> /dev/null; then
    echo "Error: Python3 is not installed."
    exit 1
else
    echo "Python3 is already installed."
fi

# Check and create virtual environment
echo "Checking Python virtual environment..."
if [ ! -d "$VENV_DIR" ]; then
    echo "Creating Python virtual environment..."
    python3 -m venv "$VENV_DIR"
else
    echo "Python virtual environment already exists."
fi

# Activate virtual environment and install requirements
echo "Activating virtual environment and installing requirements..."
source "$VENV_DIR/bin/activate"
pip install --upgrade pip
pip install -r python/requirements.txt

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
cd upmem
make clean
make
cd ..

# Warm-up phase: Test all executables with a simple test case
echo "=========================================="
echo "WARM-UP PHASE: Testing all executables"
echo "=========================================="

# Check if test input file exists
if [ ! -f "$INPUT_DIR/test.txt" ]; then
    echo "Warning: test.txt not found in input directory. Creating a simple test file..."
    mkdir -p "$INPUT_DIR"
    echo -e "1 2 3\n1 2 4\n1 3 4\n2 3 4\n1 2 3 4" > "$INPUT_DIR/test.txt"
fi

# Test original C++ executable
echo "Warming up original C++ executable..."
if [ -f "$ORIGINAL_EXE" ]; then
    start_time=$(date +%s.%N)
    sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH "$ORIGINAL_EXE" "input_later/T40I10D100K.dat" "800" > output/T40I10D100K.dat.txt
    end_time=$(date +%s.%N)
    original_time=$(echo "$end_time - $start_time" | bc)
    echo "  Original C++: ${original_time}s"
else
    echo "✗ Original C++ executable not found"
fi
echo "----------------------------------------"

# # Test pfp_growth C++ executable
# echo "Warming up pfp_growth C++ executable..."
# if [ -f "$PFP_EXE" ]; then
#     sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH "$PFP_EXE" "$INPUT_DIR/test.txt" "2" > /dev/null 2>&1
# else
#     echo "✗ PFP Growth C++ executable not found"
# fi

# # Test upmem C++ executable
# echo "Warming up upmem C++ executable..."
# if [ -f "$UPMEM_EXE" ]; then
#     sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH "$UPMEM_EXE" "$INPUT_DIR/test.txt" "2" "$OUTPUT_DIR/test.txt" > /dev/null 2>&1
# else
#     echo "✗ UpMem C++ executable not found"
# fi

# # Test Python script
# echo "Warming up Python script..."
# if [ -f "$PY_SCRIPT" ]; then
#     python3 "$PY_SCRIPT" "$INPUT_DIR/test.txt" "2" > /dev/null 2>&1
# else
#     echo "✗ Python script not found"
# fi

# echo "Warm-up phase completed!"
# echo "=========================================="

# # Define min_support values for different datasets
# declare -A min_supports
# min_supports["DataSetA_norm.txt"]="1500 1400 1300"
# min_supports["mushroom.dat.txt"]="3200 2500 1600"
# min_supports["test.txt"]="2 3 4"

# echo "Starting main test phase..."
# echo "=========================================="

# for input_file in "$INPUT_DIR"/*; do
#     input_name=$(basename "$input_file")
#     base_name="${input_name%.*}"
#     ext="${input_name##*.}"
    
#     # Get min_support values for this dataset
#     min_support_list=${min_supports[$input_name]}
#     if [ -z "$min_support_list" ]; then
#         min_support_list="1500"
#     fi
    
#     # Test with each min_support value
#     for min_support in $min_support_list; do
#         echo "=========================================="
#         echo "Testing $input_name with min_support=$min_support"
#         echo "=========================================="
    
#         echo "Running original C++ on $input_file with min_support=$min_support..."
#         start_time=$(date +%s.%N)
#         sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH "$ORIGINAL_EXE" "$input_file" "$min_support" > "$OUTPUT_DIR/${base_name}_ms${min_support}_org.$ext"
#         end_time=$(date +%s.%N)
#         original_time=$(echo "$end_time - $start_time" | bc)
        
#         echo "Running pfp_growth C++ on $input_file with min_support=$min_support..."
#         start_time=$(date +%s.%N)
#         sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH "$PFP_EXE" "$input_file" "$min_support" > "$OUTPUT_DIR/${base_name}_ms${min_support}_pfp.$ext"
#         end_time=$(date +%s.%N)
#         pfp_time=$(echo "$end_time - $start_time" | bc)
        
#         echo "Running upmem C++ on $input_file with min_support=$min_support..."
#         start_time=$(date +%s.%N)
#         sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH "$UPMEM_EXE" "$input_file" "$min_support" "$OUTPUT_DIR/${base_name}_ms${min_support}_upmem.$ext"
#         end_time=$(date +%s.%N)
#         upmem_time=$(echo "$end_time - $start_time" | bc)
        
#         # Skip Python for mushroom dataset with min_support=1600
#         if [[ "$input_name" == "mushroom.dat.txt" && "$min_support" == "1600" ]]; then
#             echo "Skipping Python execution for mushroom.dat.txt with min_support=1600..."
#             python_time="N/A"
#         else
#             echo "Running Python on $input_file with min_support=$min_support..."
#             start_time=$(date +%s.%N)
#             python3 "$PY_SCRIPT" "$input_file" "$min_support" > "$OUTPUT_DIR/${base_name}_ms${min_support}_py.$ext"
#             end_time=$(date +%s.%N)
#             python_time=$(echo "$end_time - $start_time" | bc)
#         fi
        
#         echo "--- Execution Summary for $input_name (min_support=$min_support) ---"
#         echo "  Original C++: ${original_time}s"
#         echo "  PFP Growth C++: ${pfp_time}s"
#         echo "  UpMem C++: ${upmem_time}s"
#         echo "  Python: ${python_time}s"
#         echo "----------------------------------------"
#     done
# done

# # Compare outputs
# echo "Comparing outputs..."
# python3 compare_outputs.py
