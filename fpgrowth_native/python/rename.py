import sys

def main():
    if len(sys.argv) != 3:
        print("Usage: python rename_items.py <input_file> <output_file>")
        sys.exit(1)

    input_path = sys.argv[1]
    output_path = sys.argv[2]

    with open(input_path, 'r', encoding='utf-8') as f:
        lines = [line.strip() for line in f if line.strip()]

    transactions = [list(map(int, line.split())) for line in lines]
    unique_items = sorted({item for tx in transactions for item in tx})
    item_to_newid = {old: i + 1 for i, old in enumerate(unique_items)}
    remapped_transactions = [[item_to_newid[item] for item in tx] for tx in transactions]

    with open(output_path, 'w', encoding='utf-8') as f:
        for tx in remapped_transactions:
            f.write(' '.join(map(str, tx)) + '\n')
            
    print(f"Total unique items: {len(unique_items)}\n")
    
if __name__ == "__main__":
    main()