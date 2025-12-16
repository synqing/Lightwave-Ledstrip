import sys, os, json, glob

def check_required_files(pr_dir):
    required = ['*.cpp', '*.h', '*_card.json', '*.mp4', '*.md']
    missing = []
    for pattern in required:
        if not glob.glob(os.path.join(pr_dir, pattern)):
            missing.append(pattern)
    return missing

def check_card_metadata(card_path):
    with open(card_path) as f:
        card = json.load(f)
    required_fields = ['metadata', 'technical', 'parameters', 'media']
    return [f for f in required_fields if f not in card]

def main():
    pr_dir = sys.argv[1] if len(sys.argv) > 1 else '.'
    missing = check_required_files(pr_dir)
    if missing:
        print(f"Missing required files: {missing}")
        sys.exit(1)
    card_path = glob.glob(os.path.join(pr_dir, '*_card.json'))[0]
    missing_meta = check_card_metadata(card_path)
    if missing_meta:
        print(f"Missing metadata fields: {missing_meta}")
        sys.exit(1)
    print("Pattern linter passed.")
    sys.exit(0)

if __name__ == "__main__":
    main() 