## Removed 'set -e' to allow script to continue on errors

OUTPUT_DIR="output"

found=0
org_match=0
org_total=0
pfp_match=0
pfp_total=0
for py_file in "$OUTPUT_DIR"/*_py.*; do
    if [ ! -e "$py_file" ]; then
        continue
    fi
    found=1
    base_name="${py_file%_py.*}"
    ext="${py_file##*.}"
    org_file="${base_name}_org.$ext"
    pfp_file="${base_name}_pfp.$ext"

    echo "Comparing $(basename "$py_file") with org and pfp..."

    # Compare org (ignore order)
    org_total=$((org_total+1))
    if diff -q <(sort "$py_file") <(sort "$org_file") > /dev/null; then
        org_match=$((org_match+1))
    fi

    # Compare pfp (ignore order)
    pfp_total=$((pfp_total+1))
    if diff -q <(sort "$py_file") <(sort "$pfp_file") > /dev/null; then
        pfp_match=$((pfp_match+1))
    fi
done

if [ $found -eq 0 ]; then
    echo "No Python output files found to compare."
else
    echo "ORG Score: $org_match/$org_total matched."
    echo "PFP Score: $pfp_match/$pfp_total matched."
fi