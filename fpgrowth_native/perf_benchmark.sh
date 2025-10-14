#!/bin/bash
set -e

# Performance Benchmark Script for FP-Growth Implementations
# This script uses perf stat to compare performance characteristics

# Paths
INPUT_DIR="input"
OUTPUT_DIR="output"
PERF_OUTPUT_DIR="perf_results"
ORIGINAL_EXE="original/fpgrowth"
PFP_EXE="pfp_growth/fpgrowth_cpu"
UPMEM_EXE="upmem/build/main"
PY_SCRIPT="python/fp_growth.py"
VENV_DIR="python/venv"

# Ensure output directories exist
mkdir -p "$OUTPUT_DIR"
mkdir -p "$PERF_OUTPUT_DIR"

# Check if perf is available and accessible
PERF_AVAILABLE=false
if command -v perf &> /dev/null; then
    # Test if perf works without sudo
    if perf stat echo "test" >/dev/null 2>&1; then
        PERF_AVAILABLE=true
        PERF_CMD="perf stat"
    elif sudo perf stat echo "test" >/dev/null 2>&1; then
        PERF_AVAILABLE=true
        PERF_CMD="sudo perf stat"
        echo "Note: Using sudo for perf access"
    else
        echo "Warning: perf is not accessible due to security restrictions."
        echo "Performance monitoring will fall back to basic timing."
        echo "To enable full perf access, you can run:"
        echo "  echo 'kernel.perf_event_paranoid = 1' | sudo tee -a /etc/sysctl.conf"
        echo "  sudo sysctl -p"
        PERF_AVAILABLE=false
    fi
else
    echo "Warning: perf is not installed. Install with: sudo apt install linux-perf"
    echo "Performance monitoring will fall back to basic timing."
    PERF_AVAILABLE=false
fi

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
"$VENV_DIR/bin/pip" install --upgrade pip
"$VENV_DIR/bin/pip" install -r python/requirements.txt

# Function to run Python with virtual environment
run_python() {
    "$VENV_DIR/bin/python" "$@"
}

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
if command -v dpu-pkg-config &> /dev/null; then
    make clean
    make
    echo "UPMEM build successful"
else
    echo "Warning: DPU development environment not found. Skipping UPMEM build."
    echo "To build UPMEM components, install the UPMEM SDK first."
fi
cd ..

# Performance benchmark function
run_perf_benchmark() {
    local exe_name="$1"
    local exe_path="$2"
    local input_file="$3"
    local min_support="$4"
    local output_file="$5"
    local perf_output="$6"
    
    echo "Running benchmark on $exe_name..."
    if [ -f "$exe_path" ]; then
        if [ "$PERF_AVAILABLE" = true ]; then
            # Run with full perf stats
            $PERF_CMD -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses,page-faults,context-switches,cpu-migrations,L1-dcache-loads,L1-dcache-load-misses,LLC-loads,LLC-load-misses,dTLB-loads,dTLB-load-misses -o "$perf_output" "$exe_path" "$input_file" "$min_support" > "$output_file" 2>&1
            echo "✓ $exe_name completed - perf results saved to $perf_output"
        else
            # Fallback to basic timing
            echo "# Fallback timing benchmark for $exe_name" > "$perf_output"
            start_time=$(date +%s.%N)
            "$exe_path" "$input_file" "$min_support" > "$output_file" 2>&1
            end_time=$(date +%s.%N)
            execution_time=$(echo "$end_time - $start_time" | bc -l)
            echo "     ${execution_time} seconds time elapsed" >> "$perf_output"
            echo "✓ $exe_name completed in ${execution_time}s - basic timing saved to $perf_output"
        fi
    else
        echo "✗ $exe_name executable not found at $exe_path"
        echo "N/A - executable not found" > "$perf_output"
    fi
}

# Warm-up phase: Test all executables with a simple test case
echo "=========================================="
echo "PERFORMANCE BENCHMARK: FP-Growth Implementations"
echo "=========================================="

# Check if test input file exists
if [ ! -f "$INPUT_DIR/test.txt" ]; then
    echo "Warning: test.txt not found in input directory. Creating a simple test file..."
    mkdir -p "$INPUT_DIR"
    echo -e "1 2 3\n1 2 4\n1 3 4\n2 3 4\n1 2 3 4" > "$INPUT_DIR/test.txt"
fi

# Test dataset configurations
declare -A test_configs
test_configs["test.txt"]="2"
test_configs["mushroom.dat.txt"]="1600"
test_configs["connect.txt"]="58000"

echo "Available test datasets:"
for dataset in "${!test_configs[@]}"; do
    if [ -f "$INPUT_DIR/$dataset" ]; then
        echo "  ✓ $dataset (min_support=${test_configs[$dataset]})"
    else
        echo "  ✗ $dataset (not found)"
    fi
done
echo "----------------------------------------"

# Run benchmarks for each available dataset
for dataset in "${!test_configs[@]}"; do
    input_file="$INPUT_DIR/$dataset"
    min_support="${test_configs[$dataset]}"
    
    if [ ! -f "$input_file" ]; then
        echo "Skipping $dataset - file not found"
        continue
    fi
    
    base_name="${dataset%.*}"
    timestamp=$(date +%Y%m%d_%H%M%S)
    
    echo "=========================================="
    echo "BENCHMARKING: $dataset (min_support=$min_support)"
    echo "=========================================="
    
    # Benchmark Original C++ implementation
    run_perf_benchmark \
        "Original C++" \
        "$ORIGINAL_EXE" \
        "$input_file" \
        "$min_support" \
        "$OUTPUT_DIR/${base_name}_original_${timestamp}.txt" \
        "$PERF_OUTPUT_DIR/${base_name}_original_${timestamp}_perf.txt"
    
    # Benchmark PFP Growth C++ implementation
    run_perf_benchmark \
        "PFP Growth C++" \
        "$PFP_EXE" \
        "$input_file" \
        "$min_support" \
        "$OUTPUT_DIR/${base_name}_pfp_${timestamp}.txt" \
        "$PERF_OUTPUT_DIR/${base_name}_pfp_${timestamp}_perf.txt"
    
    # Benchmark UPMEM implementation (if available)
    if [ -f "$UPMEM_EXE" ]; then
        echo "Running benchmark on UPMEM..."
        if [ "$PERF_AVAILABLE" = true ]; then
            if $PERF_CMD -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses,page-faults,context-switches,cpu-migrations,L1-dcache-loads,L1-dcache-load-misses,LLC-loads,LLC-load-misses,dTLB-loads,dTLB-load-misses -o "$PERF_OUTPUT_DIR/${base_name}_upmem_${timestamp}_perf.txt" "$UPMEM_EXE" "$input_file" "$min_support" "$OUTPUT_DIR/${base_name}_upmem_${timestamp}.txt" 2>&1; then
                echo "✓ UPMEM completed - perf results saved to $PERF_OUTPUT_DIR/${base_name}_upmem_${timestamp}_perf.txt"
            else
                echo "✗ UPMEM execution failed"
                echo "N/A - execution failed" > "$PERF_OUTPUT_DIR/${base_name}_upmem_${timestamp}_perf.txt"
            fi
        else
            # Fallback to basic timing for UPMEM
            echo "# Fallback timing benchmark for UPMEM" > "$PERF_OUTPUT_DIR/${base_name}_upmem_${timestamp}_perf.txt"
            start_time=$(date +%s.%N)
            if "$UPMEM_EXE" "$input_file" "$min_support" "$OUTPUT_DIR/${base_name}_upmem_${timestamp}.txt" 2>&1; then
                end_time=$(date +%s.%N)
                execution_time=$(echo "$end_time - $start_time" | bc -l)
                echo "     ${execution_time} seconds time elapsed" >> "$PERF_OUTPUT_DIR/${base_name}_upmem_${timestamp}_perf.txt"
                echo "✓ UPMEM completed in ${execution_time}s - basic timing saved"
            else
                echo "✗ UPMEM execution failed"
                echo "N/A - execution failed" > "$PERF_OUTPUT_DIR/${base_name}_upmem_${timestamp}_perf.txt"
            fi
        fi
    else
        echo "Skipping UPMEM - executable not available"
        echo "N/A - executable not available" > "$PERF_OUTPUT_DIR/${base_name}_upmem_${timestamp}_perf.txt"
    fi
    
    echo "----------------------------------------"
done

echo "=========================================="
echo "PERFORMANCE ANALYSIS SUMMARY"
echo "=========================================="

# Function to extract key metrics from perf output
extract_perf_metrics() {
    local perf_file="$1"
    local impl_name="$2"
    
    if [ ! -f "$perf_file" ] || grep -q "N/A" "$perf_file"; then
        printf "%-15s | %12s | %12s | %12s | %12s\n" "$impl_name" "N/A" "N/A" "N/A" "N/A"
        return
    fi
    
    # Extract metrics (handle different perf output formats)
    local cycles=$(grep -E "cycles|CPUs utilized" "$perf_file" | head -1 | awk '{print $1}' | sed 's/,//g')
    local instructions=$(grep "instructions" "$perf_file" | awk '{print $1}' | sed 's/,//g')
    local cache_misses=$(grep "cache-misses" "$perf_file" | awk '{print $1}' | sed 's/,//g')
    local time_elapsed=$(grep "seconds time elapsed" "$perf_file" | awk '{print $1}')
    
    # Format large numbers
    cycles=$(echo "$cycles" | numfmt --to=si 2>/dev/null || echo "$cycles")
    instructions=$(echo "$instructions" | numfmt --to=si 2>/dev/null || echo "$instructions")
    cache_misses=$(echo "$cache_misses" | numfmt --to=si 2>/dev/null || echo "$cache_misses")
    
    printf "%-15s | %12s | %12s | %12s | %12s\n" "$impl_name" "$cycles" "$instructions" "$cache_misses" "${time_elapsed}s"
}

# Print performance comparison table
for dataset in "${!test_configs[@]}"; do
    input_file="$INPUT_DIR/$dataset"
    
    if [ ! -f "$input_file" ]; then
        continue
    fi
    
    base_name="${dataset%.*}"
    echo "Dataset: $dataset"
    printf "%-15s | %12s | %12s | %12s | %12s\n" "Implementation" "Cycles" "Instructions" "Cache Misses" "Time"
    printf "%-15s-+-%12s-+-%12s-+-%12s-+-%12s\n" "---------------" "------------" "------------" "------------" "------------"
    
    # Find the latest perf files for this dataset
    original_perf=$(ls -t "$PERF_OUTPUT_DIR/${base_name}_original_"*"_perf.txt" 2>/dev/null | head -1)
    pfp_perf=$(ls -t "$PERF_OUTPUT_DIR/${base_name}_pfp_"*"_perf.txt" 2>/dev/null | head -1)
    upmem_perf=$(ls -t "$PERF_OUTPUT_DIR/${base_name}_upmem_"*"_perf.txt" 2>/dev/null | head -1)
    
    extract_perf_metrics "$original_perf" "Original C++"
    extract_perf_metrics "$pfp_perf" "PFP Growth C++"
    extract_perf_metrics "$upmem_perf" "UPMEM"
    
    echo ""
done

echo "=========================================="
echo "Detailed perf results saved in: $PERF_OUTPUT_DIR/"
echo "Algorithm outputs saved in: $OUTPUT_DIR/"
echo "=========================================="


# # Compare outputs
# echo "Comparing outputs..."
# python3 compare_outputs.py
