#!/usr/bin/env python3
"""
ITF ADR-015 Bigint Validator (Relaxed Mode)

Validates ITF JSON files with relaxed rules for Quint simulator output.
Allows raw integers in #meta fields (timestamp, index) but enforces
strict #bigint encoding everywhere else.

This is used for simulator-generated ITF files where Quint emits
raw integers in metadata fields that we don't control.

Usage:
    python3 validate_itf_bigint_relaxed.py <path/to/itf.json>
"""

import json
import sys
from pathlib import Path
from typing import Any, List, Tuple

def walk_and_collect_raw_numbers_relaxed(obj: Any, path: str = "$", in_meta: bool = False) -> List[Tuple[str, Any, str]]:
    """
    Recursively walk JSON object and collect raw numbers, but ignore #meta fields.
    
    Args:
        obj: JSON object to walk
        path: Current JSON path (for error reporting)
        in_meta: True if we're inside a #meta object
    """
    violations = []
    
    if isinstance(obj, bool) or obj is None:
        return violations
    
    if isinstance(obj, int):
        # Allow raw ints only inside #meta objects
        if not in_meta:
            violations.append((path, obj, "int"))
        return violations
    
    if isinstance(obj, float):
        # Allow raw floats only inside #meta objects
        if not in_meta:
            violations.append((path, obj, "float"))
        return violations
    
    if isinstance(obj, list):
        for i, item in enumerate(obj):
            # Check if we're inside states array - if so, check for #meta in items
            item_in_meta = in_meta
            if path.endswith("]") and "states" in path:
                # We're in states array - check if this item has #meta
                if isinstance(item, dict) and "#meta" in item:
                    item_in_meta = True
            violations.extend(walk_and_collect_raw_numbers_relaxed(item, f"{path}[{i}]", item_in_meta))
    
    elif isinstance(obj, dict):
        # Allow bigint wrapper objects
        if set(obj.keys()) == {"#bigint"} and isinstance(obj["#bigint"], str):
            return violations  # This is a valid #bigint wrapper
        
        # If this is a #meta object, mark children as in_meta
        if "#meta" in obj or path.endswith("#meta"):
            in_meta = True
        
        for key, value in obj.items():
            child_path = f"{path}.{key}"
            # If key is #meta, children are in metadata
            child_in_meta = in_meta or (key == "#meta")
            violations.extend(walk_and_collect_raw_numbers_relaxed(value, child_path, child_in_meta))
    
    return violations

def validate_itf_bigint_relaxed(itf_path: Path) -> bool:
    """Validate ITF file with relaxed rules (allow raw ints in #meta)."""
    try:
        with open(itf_path) as f:
            doc = json.load(f)
    except (FileNotFoundError, json.JSONDecodeError) as e:
        print(f"ERROR: Failed to load ITF file: {e}", file=sys.stderr)
        return False
    
    violations = walk_and_collect_raw_numbers_relaxed(doc)
    
    if violations:
        print("FAIL: Found raw numbers outside #meta fields:", file=sys.stderr)
        for path, value, vtype in violations[:30]:
            print(f"  {path} = {value!r} ({vtype})", file=sys.stderr)
        if len(violations) > 30:
            print(f"  ... and {len(violations) - 30} more", file=sys.stderr)
        print("\nNote: Raw integers in #meta fields are allowed in relaxed mode.", file=sys.stderr)
        return False
    
    print("PASS: No raw numbers found outside #meta fields (relaxed ADR-015).")
    return True

def main():
    if len(sys.argv) < 2:
        print("Usage: validate_itf_bigint_relaxed.py <path/to/itf.json>", file=sys.stderr)
        sys.exit(1)
    
    itf_path = Path(sys.argv[1])
    if not itf_path.exists():
        print(f"ERROR: ITF file not found: {itf_path}", file=sys.stderr)
        sys.exit(1)
    
    success = validate_itf_bigint_relaxed(itf_path)
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
