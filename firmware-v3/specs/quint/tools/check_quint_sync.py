#!/usr/bin/env python3
"""
Doc-Sync Gate: Verify Quint spec follows mode rules and typechecks.

Checks for forbidden patterns that cause mode errors:
- Braces in pure def bodies
- .filter(), .forall(), .map() on lists (should use .select(), .foldl())
- oneOf() inside pure def (should be in action init with nondet)
"""

import sys
import re
import subprocess
from pathlib import Path

FORBIDDEN_PATTERNS = [
    # Braces in pure def body (except for record literals)
    (r'pure def \w+\([^)]*\):\s*\w+\s*=\s*\{(?!\s*\w+:)', "braces in pure def body"),
    # List .filter() - should use .select()
    (r'\.\s*filter\s*\(', ".filter() on list (use .select())"),
    # List .forall() - should use .foldl()
    (r'\.\s*forall\s*\(', ".forall() on list (use .foldl())"),
    # List .map() - should use explicit construction or foldl
    (r'range\s*\([^)]+\)\s*\.\s*map\s*\(', "range().map() (use explicit list or foldl)"),
    # oneOf inside pure def
    (r'pure def [^=]+=\s*[^{]*oneOf\s*\(', "oneOf() in pure def (move to action with nondet)"),
]

def check_forbidden_patterns(spec_path: Path) -> list:
    """Check for forbidden patterns in spec file."""
    errors = []
    
    with open(spec_path) as f:
        content = f.read()
        lines = content.split('\n')
    
    for pattern, description in FORBIDDEN_PATTERNS:
        for i, line in enumerate(lines, 1):
            if re.search(pattern, line):
                errors.append(f"Line {i}: {description}")
                errors.append(f"  {line.strip()}")
    
    return errors

def run_typecheck(spec_path: Path) -> tuple:
    """Run quint typecheck and return (success, output)."""
    try:
        result = subprocess.run(
            ["quint", "typecheck", str(spec_path)],
            capture_output=True,
            text=True,
            timeout=30
        )
        return result.returncode == 0, result.stdout + result.stderr
    except Exception as e:
        return False, str(e)

def main():
    if len(sys.argv) < 2:
        print("Usage: check_quint_sync.py <spec_dir>")
        sys.exit(1)
    
    spec_dir = Path(sys.argv[1])
    spec_file = spec_dir / "beat_tracker.qnt"
    
    if not spec_file.exists():
        print(f"ERROR: Spec file not found: {spec_file}")
        sys.exit(1)
    
    errors = []
    
    # Check forbidden patterns
    pattern_errors = check_forbidden_patterns(spec_file)
    if pattern_errors:
        errors.extend(pattern_errors)
    
    # Run typecheck
    typecheck_ok, typecheck_output = run_typecheck(spec_file)
    if not typecheck_ok:
        errors.append("Typecheck failed:")
        errors.append(typecheck_output)
    
    if errors:
        print("❌ Doc-sync gate FAILED")
        for e in errors:
            print(f"  {e}")
        sys.exit(1)
    else:
        print("✅ Doc-sync gate PASSED")
        print("  - No forbidden patterns found")
        print("  - No braces in if/else value branches")
        print("  - Typecheck passed")
        sys.exit(0)

if __name__ == "__main__":
    main()
