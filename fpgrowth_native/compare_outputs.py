#!/usr/bin/env python3

import os
import glob
from typing import Set, List, Tuple

def parse_itemset(line: str) -> frozenset:
    """Parse a line into a frozenset of items, ignoring order within the itemset."""
    items = line.strip().split()
    # Filter out empty strings and convert to integers
    items = [item for item in items if item.strip()]
    if not items:
        return frozenset()
    try:
        return frozenset(int(item) for item in items)
    except ValueError:
        # Skip lines that contain non-numeric items (like comments)
        return frozenset()

def load_itemsets(filename: str) -> Set[frozenset]:
    """Load itemsets from a file, returning a set of frozensets (ignoring row order)."""
    itemsets = set()
    if not os.path.exists(filename):
        return itemsets
    
    with open(filename, 'r') as f:
        for line in f:
            itemset = parse_itemset(line)
            if itemset:  # Only add non-empty itemsets
                itemsets.add(itemset)
    return itemsets

def compare_files(file1: str, file2: str) -> Tuple[bool, int, int, int]:
    """
    Compare two files containing frequent itemsets.
    Returns: (is_match, common_count, file1_unique_count, file2_unique_count)
    """
    itemsets1 = load_itemsets(file1)
    itemsets2 = load_itemsets(file2)
    
    common = itemsets1.intersection(itemsets2)
    unique1 = itemsets1 - itemsets2
    unique2 = itemsets2 - itemsets1
    
    is_match = len(unique1) == 0 and len(unique2) == 0
    
    return is_match, len(common), len(unique1), len(unique2)

def format_itemset(itemset: frozenset) -> str:
    """Format an itemset for display."""
    if not itemset:
        return "(empty)"
    return "{" + ", ".join(str(item) for item in sorted(itemset)) + "}"

def main():
    output_dir = "output"
    
    if not os.path.exists(output_dir):
        print(f"Output directory '{output_dir}' not found.")
        return
    
    # Find all *_py.* files
    py_files = glob.glob(os.path.join(output_dir, "*_py.*"))
    
    if not py_files:
        print("No Python output files found to compare.")
        return
    
    org_matches = 0
    org_total = 0
    pfp_matches = 0
    pfp_total = 0
    
    for py_file in sorted(py_files):
        base_name = os.path.basename(py_file)
        # Extract base name: "DataSetA_norm_py.txt" -> "DataSetA_norm"
        base_without_py = base_name.replace("_py.", "_temp.")
        base_prefix = base_without_py.split("_temp.")[0]
        ext = base_name.split(".")[-1]
        
        org_file = os.path.join(output_dir, f"{base_prefix}_org.{ext}")
        pfp_file = os.path.join(output_dir, f"{base_prefix}_pfp.{ext}")
        
        print(f"Comparing {base_name}...")
        
        # Check file sizes first to avoid memory issues
        py_size = os.path.getsize(py_file) if os.path.exists(py_file) else 0
        org_size = os.path.getsize(org_file) if os.path.exists(org_file) else 0
        pfp_size = os.path.getsize(pfp_file) if os.path.exists(pfp_file) else 0
        
        # Skip very large files to avoid memory issues
        max_size = 10 * 1024 * 1024  # 10MB limit
        
        # Compare with org
        if os.path.exists(org_file):
            org_total += 1
            if py_size > max_size or org_size > max_size:
                print(f"  ORG: Skipped (file too large: py={py_size//1024}KB, org={org_size//1024}KB)")
            else:
                is_match, common, py_unique, org_unique = compare_files(py_file, org_file)
                
                if is_match:
                    print(f"  ORG: ✓ Perfect match ({common} itemsets)")
                    org_matches += 1
                else:
                    print(f"  ORG: ✗ Mismatch")
                    print(f"    Common: {common} itemsets")
                    print(f"    Python unique: {py_unique} itemsets")
                    print(f"    Original unique: {org_unique} itemsets")
                    
                    # Show a few examples of differences if not too many
                    if py_unique + org_unique <= 10:
                        py_itemsets = load_itemsets(py_file)
                        org_itemsets = load_itemsets(org_file)
                        py_only = py_itemsets - org_itemsets
                        org_only = org_itemsets - py_itemsets
                        
                        if py_only:
                            print(f"    Examples of Python-only itemsets:")
                            for itemset in sorted(py_only, key=lambda x: (len(x), sorted(x)))[:3]:
                                print(f"      {format_itemset(itemset)}")
                        
                        if org_only:
                            print(f"    Examples of Original-only itemsets:")
                            for itemset in sorted(org_only, key=lambda x: (len(x), sorted(x)))[:3]:
                                print(f"      {format_itemset(itemset)}")
        else:
            print(f"  ORG: File {org_file} not found")
        
        # Compare with pfp
        if os.path.exists(pfp_file):
            pfp_total += 1
            if py_size > max_size or pfp_size > max_size:
                print(f"  PFP: Skipped (file too large: py={py_size//1024}KB, pfp={pfp_size//1024}KB)")
            else:
                is_match, common, py_unique, pfp_unique = compare_files(py_file, pfp_file)
                
                if is_match:
                    print(f"  PFP: ✓ Perfect match ({common} itemsets)")
                    pfp_matches += 1
                else:
                    print(f"  PFP: ✗ Mismatch")
                    print(f"    Common: {common} itemsets")
                    print(f"    Python unique: {py_unique} itemsets")
                    print(f"    PFP unique: {pfp_unique} itemsets")
                    
                    # Show a few examples of differences if not too many
                    if py_unique + pfp_unique <= 10:
                        py_itemsets = load_itemsets(py_file)
                        pfp_itemsets = load_itemsets(pfp_file)
                        py_only = py_itemsets - pfp_itemsets
                        pfp_only = pfp_itemsets - py_itemsets
                        
                        if py_only:
                            print(f"    Examples of Python-only itemsets:")
                            for itemset in sorted(py_only, key=lambda x: (len(x), sorted(x)))[:3]:
                                print(f"      {format_itemset(itemset)}")
                        
                        if pfp_only:
                            print(f"    Examples of PFP-only itemsets:")
                            for itemset in sorted(pfp_only, key=lambda x: (len(x), sorted(x)))[:3]:
                                print(f"      {format_itemset(itemset)}")
        else:
            print(f"  PFP: File {pfp_file} not found")
        
        print()
    
    # Print final scores
    print("=" * 50)
    print("FINAL SCORES:")
    if org_total > 0:
        print(f"ORG Score: {org_matches}/{org_total} matched ({org_matches/org_total*100:.1f}%)")
    else:
        print("ORG Score: No files to compare")
    
    if pfp_total > 0:
        print(f"PFP Score: {pfp_matches}/{pfp_total} matched ({pfp_matches/pfp_total*100:.1f}%)")
    else:
        print("PFP Score: No files to compare")

if __name__ == "__main__":
    main()
