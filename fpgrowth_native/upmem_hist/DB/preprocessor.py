import csv
import os
import sys

hash_table = {}

def normalizer(item):
    if hash_table.get(item):
        return hash_table[item]
    else:
        hash_table[item] = len(hash_table) + 1
        return hash_table[item]


def main():
    if len(sys.argv) != 2:
        print("Usage: python preprocessor.py <input_file>")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = os.path.splitext(input_file)[0] + "_norm.txt"
    hash_file = os.path.splitext(input_file)[0] + "_hash.txt"

    with open(input_file, 'r') as infile, open(output_file, 'w', newline='') as outfile:
        reader = csv.reader(infile)

        for row in reader:
            # Process each row as needed
            processed_row = [normalizer(item.strip()) for item in row if item.strip()]
            print(f"Processing row: {processed_row}")
            if processed_row:
                outfile.write(' '.join(map(str, processed_row)) + '\n')

    with open(hash_file, 'w') as hash_outfile:
        for item, index in hash_table.items():
            hash_outfile.write(f"{item} {index}\n")
    print(f"Processed data saved to {output_file}")

if __name__ == "__main__":
    main()