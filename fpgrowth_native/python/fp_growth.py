import pandas as pd
from mlxtend.frequent_patterns import fpgrowth
import sys

# Load your dataset (space-separated integers per line)
def load_transactions(filename):
    transactions = []
    with open(filename) as f:
        for line in f:
            line = line.strip()
            # Skip empty lines and comment lines
            if not line or line.startswith('#'):
                continue
            items = line.split()
            transactions.append(items)
    return transactions

def transactions_to_df(transactions):
    all_items = sorted({item for tx in transactions for item in tx})
    encoded = []
    for tx in transactions:
        encoded.append({item: (item in tx) for item in all_items})
    return pd.DataFrame(encoded)

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <data_file> <min_support_count>")
        sys.exit(1)
    
    filename = sys.argv[1]
    min_support_count = int(sys.argv[2])
    
    transactions = load_transactions(filename)
    df = transactions_to_df(transactions)
    min_support = min_support_count / len(transactions)
    frequent_itemsets = fpgrowth(df, min_support=min_support, use_colnames=True)
    # Print frequent itemsets as space-separated integers, one per line
    for _, row in frequent_itemsets.iterrows():
        itemset = row['itemsets']
        print(' '.join(str(item) for item in itemset))
