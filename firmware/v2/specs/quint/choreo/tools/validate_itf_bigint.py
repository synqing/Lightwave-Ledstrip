#!/usr/bin/env python3
"""
ITF ADR-015 Bigint Validator

Validates that ITF JSON file contains zero raw integers or floats.
All integers must be encoded as {"#bigint": "num"} per ADR-015.

Usage:
    python3 validate_itf_bigint.py <path/to/itf.json>
"""

import json
import sys
from pathlib import Path
from typing import Any, List, Tuple

def walk_and_collect_raw_numbers(obj: Any, path: str = "$") -> List[Tuple[str, Any, str]]:
    """Recursively walk JSON object and collect all raw numbers."""
    violations = []
    
    if isinstance(obj, bool) or obj is None:
        return violations
    
    if isinstance(obj, int):
        violations.append((path, obj, "int"))
        return violations
    
    if isinstance(obj, float):
        violations.append((path, obj, "float"))
        return violations
    
    if isinstance(obj, list):
        for i, item in enumerate(obj):
            violations.extend(walk_and_collect_raw_numbers(item, f"{path}[{i}]"))
    
    elif isinstance(obj, dict):
        # Allow bigint wrapper objects only when they look exactly like {"#bigint":"..."}
        if set(obj.keys()) == {"#bigint"} and isinstance(obj["#bigint"], str):
            return violations  # This is a valid #bigint wrapper
        
        for key, value in obj.items():
            violations.extend(walk_and_collect_raw_numbers(value, f"{path}.{key}"))
    
    return violations

def validate_itf_bigint(itf_path: Path) -> bool:
    """Validate ITF file contains zero raw numbers."""
    try:
        with open(itf_path) as f:
            doc = json.load(f)
    except (FileNotFoundError, json.JSONDecodeError) as e:
        print(f"ERROR: Failed to load ITF file: {e}", file=sys.stderr)
        return False
    
    violations = walk_and_collect_raw_numbers(doc)
    
    if violations:
        print("FAIL: Found raw numbers in ITF:", file=sys.stderr)
        for path, value, vtype in violations[:30]:
            print(f"  {path} = {value!r} ({vtype})", file=sys.stderr)
        if len(violations) > 30:
            print(f"  ... and {len(violations) - 30} more", file=sys.stderr)
        return False
    
    print("PASS: No raw numbers found. ITF looks #bigint-clean.")
    return True

def main():
    if len(sys.argv) < 2:
        print("Usage: validate_itf_bigint.py <path/to/itf.json>", file=sys.stderr)
        sys.exit(1)
    
    itf_path = Path(sys.argv[1])
    if not itf_path.exists():
        print(f"ERROR: ITF file not found: {itf_path}", file=sys.stderr)
        sys.exit(1)
    
    success = validate_itf_bigint(itf_path)
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
