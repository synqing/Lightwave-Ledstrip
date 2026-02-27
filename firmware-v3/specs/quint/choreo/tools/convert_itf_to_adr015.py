#!/usr/bin/env python3
"""
ITF ADR-015 Converter

Converts ITF JSON files from Quint's native format (raw integers) to ADR-015
compliant format (all integers encoded as {"#bigint": "num"}).

This is used as a post-processing step for Quint's --out-itf output, which
doesn't conform to ADR-015 by default.

Usage:
    python3 convert_itf_to_adr015.py <input.itf.json> <output.itf.json>
    python3 convert_itf_to_adr015.py <input.itf.json> <output.itf.json> --preserve-meta
"""

import json
import sys
from pathlib import Path
from typing import Any, Dict, List


def wrap_bigint(value: int) -> Dict[str, str]:
    """Wrap integer value in ADR-015 #bigint format."""
    return {"#bigint": str(value)}


def convert_to_adr015(obj: Any, preserve_meta: bool = False, in_meta: bool = False) -> Any:
    """
    Recursively convert all integers in JSON object to ADR-015 #bigint format.
    
    Args:
        obj: JSON object to convert
        preserve_meta: If True, preserve raw integers in #meta fields
        in_meta: True if we're currently inside a #meta object
    
    Returns:
        Converted object with all integers wrapped in {"#bigint": "num"}
    """
    # Convert integers to #bigint format
    if isinstance(obj, int):
        # Preserve raw ints in #meta if requested
        if preserve_meta and in_meta:
            return obj
        return wrap_bigint(obj)
    
    # Convert floats (treat as integers for ADR-015 - though ADR-015 is primarily about ints)
    if isinstance(obj, float):
        # Preserve raw floats in #meta if requested
        if preserve_meta and in_meta:
            return obj
        # Convert float to int if it's a whole number, otherwise wrap as bigint
        if obj.is_integer():
            return wrap_bigint(int(obj))
        # For non-integer floats, we'll wrap them too (though ADR-015 doesn't specify)
        return wrap_bigint(int(obj)) if obj.is_integer() else obj
    
    # Preserve booleans and None
    if isinstance(obj, bool) or obj is None:
        return obj
    
    # Handle strings (no conversion needed)
    if isinstance(obj, str):
        return obj
    
    # Handle lists
    if isinstance(obj, list):
        return [convert_to_adr015(item, preserve_meta, in_meta) for item in obj]
    
    # Handle dictionaries
    if isinstance(obj, dict):
        # Check if this is already a #bigint wrapper - preserve it
        if set(obj.keys()) == {"#bigint"} and isinstance(obj.get("#bigint"), str):
            return obj  # Already in ADR-015 format
        
        # Check if we're entering a #meta object
        is_meta_obj = "#meta" in obj or any(key == "#meta" for key in obj.keys())
        child_in_meta = in_meta or is_meta_obj
        
        # Convert all key-value pairs
        converted = {}
        for key, value in obj.items():
            # If key is #meta, children are in metadata
            child_is_meta = child_in_meta or (key == "#meta")
            converted[key] = convert_to_adr015(value, preserve_meta, child_is_meta)
        
        return converted
    
    # Fallback: return as-is (shouldn't happen with valid JSON)
    return obj


def convert_itf_file(input_path: Path, output_path: Path, preserve_meta: bool = False) -> bool:
    """
    Convert ITF file from Quint format to ADR-015 format.
    
    Args:
        input_path: Path to input ITF file (Quint format)
        output_path: Path to output ITF file (ADR-015 format)
        preserve_meta: If True, preserve raw integers in #meta fields
    
    Returns:
        True if conversion succeeded, False otherwise
    """
    try:
        with open(input_path) as f:
            doc = json.load(f)
    except (FileNotFoundError, json.JSONDecodeError) as e:
        print(f"ERROR: Failed to load input ITF file: {e}", file=sys.stderr)
        return False
    
    # Convert to ADR-015 format
    converted = convert_to_adr015(doc, preserve_meta=preserve_meta)
    
    # Write output
    try:
        with open(output_path, 'w') as f:
            json.dump(converted, f, indent=2, ensure_ascii=False)
    except (IOError, OSError) as e:
        print(f"ERROR: Failed to write output ITF file: {e}", file=sys.stderr)
        return False
    
    print(f"Converted {input_path} to ADR-015 format: {output_path}")
    return True


def main():
    if len(sys.argv) < 3:
        print("Usage: convert_itf_to_adr015.py <input.itf.json> <output.itf.json> [--preserve-meta]", file=sys.stderr)
        print("", file=sys.stderr)
        print("Options:", file=sys.stderr)
        print("  --preserve-meta    Preserve raw integers in #meta fields (relaxed ADR-015)", file=sys.stderr)
        sys.exit(1)
    
    input_path = Path(sys.argv[1])
    output_path = Path(sys.argv[2])
    preserve_meta = "--preserve-meta" in sys.argv
    
    if not input_path.exists():
        print(f"ERROR: Input ITF file not found: {input_path}", file=sys.stderr)
        sys.exit(1)
    
    # Create output directory if needed
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    success = convert_itf_file(input_path, output_path, preserve_meta=preserve_meta)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
