#!/usr/bin/env python3
"""
increment_iteration.py - Ralph Loop Iteration Tracking

Increments ralph_loop.current_iteration and checks against max_iterations safety limit.
Optionally blocks item if max iterations exceeded without convergence.

Usage:
    python3 increment_iteration.py ITEM_ID [OPTIONS]

Arguments:
    ITEM_ID         Feature item ID (e.g., FIX-TEST-ASSERTIONS)

Options:
    --block-on-max  Change status to BLOCKED if max_iterations exceeded
    --reason TEXT   Override reason (required if --block-on-max)
    --harness-dir PATH  Path to .claude/harness directory

Example:
    # Increment iteration (warning if max exceeded)
    python3 increment_iteration.py FIX-TEST-ASSERTIONS

    # Increment and block if max exceeded
    python3 increment_iteration.py FIX-TEST-ASSERTIONS \\
        --block-on-max \\
        --reason "Max iterations (5) reached without convergence. Requires human review."
"""

import json
import sys
from pathlib import Path
from typing import Dict, Any, Optional

def find_harness_dir(start_path: Optional[Path] = None) -> Path:
    """Find .claude/harness directory by walking up from current directory."""
    current = start_path or Path.cwd()

    for _ in range(10):
        harness_candidate = current / ".claude" / "harness"
        if harness_candidate.is_dir():
            return harness_candidate

        if current.parent == current:
            break
        current = current.parent

    raise FileNotFoundError(
        "Could not find .claude/harness directory. "
        "Ensure you're running from within the project."
    )

def load_feature_list(harness_dir: Path) -> Dict[str, Any]:
    """Load and parse feature_list.json."""
    feature_list_path = harness_dir / "feature_list.json"

    if not feature_list_path.exists():
        raise FileNotFoundError(f"feature_list.json not found at {feature_list_path}")

    with open(feature_list_path, 'r', encoding='utf-8') as f:
        return json.load(f)

def save_feature_list(harness_dir: Path, data: Dict[str, Any]) -> None:
    """Save feature_list.json with pretty formatting."""
    feature_list_path = harness_dir / "feature_list.json"

    with open(feature_list_path, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
        f.write('\n')  # Trailing newline

def increment_iteration(
    harness_dir: Path,
    item_id: str,
    block_on_max: bool = False,
    override_reason: Optional[str] = None
) -> Dict[str, Any]:
    """
    Increment ralph_loop.current_iteration and check safety limits.

    Args:
        harness_dir: Path to .claude/harness directory
        item_id: Feature item ID
        block_on_max: Change status to BLOCKED if max exceeded
        override_reason: Reason for blocking (required if block_on_max)

    Returns:
        JSON-serializable result dict
    """
    try:
        data = load_feature_list(harness_dir)
    except Exception as e:
        return {
            "success": False,
            "error": "failed_to_load_feature_list",
            "message": str(e)
        }

    items = data.get("items", [])

    # Find item by ID
    item = None
    item_index = None
    for i, candidate in enumerate(items):
        if candidate.get("id") == item_id:
            item = candidate
            item_index = i
            break

    if not item:
        return {
            "success": False,
            "error": "item_not_found",
            "message": f"No item with id '{item_id}' in feature_list.json"
        }

    # Check ralph_loop configuration
    ralph_loop = item.get("ralph_loop")
    if not ralph_loop:
        return {
            "success": False,
            "error": "no_ralph_loop_config",
            "message": f"Item '{item_id}' has no ralph_loop configuration"
        }

    # Get current values
    current_iteration = ralph_loop.get("current_iteration", 0)
    max_iterations = ralph_loop.get("max_iterations", 5)

    # Increment
    new_iteration = current_iteration + 1
    ralph_loop["current_iteration"] = new_iteration

    # Check if max exceeded
    max_exceeded = new_iteration >= max_iterations
    warnings = []

    if max_exceeded:
        warnings.append(
            f"Max iterations ({max_iterations}) reached or exceeded "
            f"(current: {new_iteration})"
        )

        if block_on_max:
            if not override_reason:
                return {
                    "success": False,
                    "error": "missing_override_reason",
                    "message": "--block-on-max requires --reason"
                }

            item["status"] = "BLOCKED"
            item["override_reason"] = override_reason

            warnings.append(
                f"Status changed to BLOCKED: {override_reason}"
            )

    # Update item in data
    item["ralph_loop"] = ralph_loop
    items[item_index] = item
    data["items"] = items

    # Save back to file
    try:
        save_feature_list(harness_dir, data)
    except Exception as e:
        return {
            "success": False,
            "error": "failed_to_save_feature_list",
            "message": str(e)
        }

    result = {
        "success": True,
        "item_id": item_id,
        "previous_iteration": current_iteration,
        "current_iteration": new_iteration,
        "max_iterations": max_iterations,
        "max_exceeded": max_exceeded,
        "message": f"Incremented iteration {current_iteration} -> {new_iteration}"
    }

    if warnings:
        result["warnings"] = warnings

    if block_on_max and max_exceeded:
        result["status_changed"] = "BLOCKED"
        result["override_reason"] = override_reason

    return result

def main():
    """CLI entry point."""
    import argparse

    parser = argparse.ArgumentParser(
        description="Increment ralph_loop.current_iteration and check limits",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Increment iteration (warn if max exceeded)
  python3 increment_iteration.py FIX-TEST-ASSERTIONS

  # Increment and block on max iterations
  python3 increment_iteration.py HEAP-FRAG-MONITOR \\
      --block-on-max \\
      --reason "Max iterations (5) reached. Requires human architectural review."

Exit codes:
  0  Success
  1  Max iterations exceeded (warning only, not blocked)
  2  Error (item not found, invalid config, etc.)
        """
    )

    parser.add_argument("item_id", help="Feature item ID")
    parser.add_argument(
        "--block-on-max",
        action="store_true",
        help="Change status to BLOCKED if max_iterations exceeded"
    )
    parser.add_argument(
        "--reason",
        help="Override reason for blocking (required with --block-on-max)"
    )
    parser.add_argument(
        "--harness-dir",
        type=Path,
        help="Path to .claude/harness directory"
    )

    args = parser.parse_args()

    # Validate required fields
    if args.block_on_max and not args.reason:
        parser.error("--block-on-max requires --reason")

    try:
        harness_dir = args.harness_dir or find_harness_dir()

        result = increment_iteration(
            harness_dir,
            args.item_id,
            block_on_max=args.block_on_max,
            override_reason=args.reason
        )

        print(json.dumps(result, indent=2))

        # Exit codes: 0=success, 1=max exceeded (warn), 2=error
        if not result.get("success"):
            sys.exit(2)
        elif result.get("max_exceeded") and not args.block_on_max:
            sys.exit(1)
        else:
            sys.exit(0)

    except Exception as e:
        error_result = {
            "success": False,
            "error": "unexpected_error",
            "message": str(e)
        }
        print(json.dumps(error_result, indent=2), file=sys.stderr)
        sys.exit(2)

if __name__ == "__main__":
    main()
