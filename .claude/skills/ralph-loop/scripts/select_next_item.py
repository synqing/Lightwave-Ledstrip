#!/usr/bin/env python3
"""
select_next_item.py - Ralph Loop Item Selection Logic

Selects the highest priority FAILING item with ralph_loop.enabled=true from feature_list.json.
Implements dependency checking and provides clear feedback for edge cases.

Usage:
    python3 select_next_item.py [--harness-dir PATH]

Output:
    JSON object with selected item or error status
"""

import json
import sys
from pathlib import Path
from typing import Dict, List, Optional, Any

def find_harness_dir(start_path: Optional[Path] = None) -> Path:
    """
    Find the .claude/harness directory by walking up from current directory.

    Args:
        start_path: Starting directory (defaults to cwd)

    Returns:
        Path to .claude/harness directory

    Raises:
        FileNotFoundError: If .claude/harness not found
    """
    current = start_path or Path.cwd()

    for _ in range(10):  # Limit search depth
        harness_candidate = current / ".claude" / "harness"
        if harness_candidate.is_dir():
            return harness_candidate

        if current.parent == current:  # Reached filesystem root
            break
        current = current.parent

    raise FileNotFoundError(
        "Could not find .claude/harness directory. "
        "Ensure you're running from within the project."
    )

def load_feature_list(harness_dir: Path) -> Dict[str, Any]:
    """
    Load and parse feature_list.json.

    Args:
        harness_dir: Path to .claude/harness directory

    Returns:
        Parsed feature_list.json data

    Raises:
        FileNotFoundError: If feature_list.json doesn't exist
        json.JSONDecodeError: If JSON is malformed
    """
    feature_list_path = harness_dir / "feature_list.json"

    if not feature_list_path.exists():
        raise FileNotFoundError(f"feature_list.json not found at {feature_list_path}")

    with open(feature_list_path, 'r', encoding='utf-8') as f:
        return json.load(f)

def get_item_by_id(items: List[Dict], item_id: str) -> Optional[Dict]:
    """Find item by ID in items list."""
    for item in items:
        if item.get("id") == item_id:
            return item
    return None

def check_dependencies(item: Dict, all_items: List[Dict]) -> Dict[str, Any]:
    """
    Check if item's dependencies are satisfied.

    Args:
        item: Item to check
        all_items: All items in feature list

    Returns:
        Dict with 'satisfied' (bool), 'blocking_deps' (list of IDs), 'warnings' (list)
    """
    dependencies = item.get("dependencies", [])

    if not dependencies:
        return {"satisfied": True, "blocking_deps": [], "warnings": []}

    blocking_deps = []
    warnings = []

    for dep_id in dependencies:
        dep_item = get_item_by_id(all_items, dep_id)

        if not dep_item:
            warnings.append(f"Dependency '{dep_id}' not found in feature list")
            continue

        dep_status = dep_item.get("status", "FAILING")

        if dep_status == "FAILING":
            blocking_deps.append(dep_id)
        elif dep_status == "BLOCKED":
            warnings.append(
                f"Dependency '{dep_id}' is BLOCKED (may need human intervention)"
            )

    return {
        "satisfied": len(blocking_deps) == 0,
        "blocking_deps": blocking_deps,
        "warnings": warnings
    }

def select_next_item(harness_dir: Path) -> Dict[str, Any]:
    """
    Select the next item for Ralph Loop execution.

    Selection criteria:
    1. ralph_loop.enabled = true
    2. status = FAILING
    3. Dependencies satisfied (PASSING or CANCELLED)
    4. Highest priority (lowest number)

    Args:
        harness_dir: Path to .claude/harness directory

    Returns:
        JSON-serializable dict with selection result
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

    if not items:
        return {
            "success": False,
            "error": "empty_feature_list",
            "message": "feature_list.json contains no items"
        }

    # Filter for Ralph Loop eligible items
    eligible_items = []

    for item in items:
        # Check ralph_loop.enabled
        ralph_loop = item.get("ralph_loop", {})
        if not ralph_loop.get("enabled", False):
            continue

        # Check status = FAILING
        if item.get("status") != "FAILING":
            continue

        # Check dependencies
        dep_check = check_dependencies(item, items)

        eligible_items.append({
            "item": item,
            "dependency_check": dep_check
        })

    if not eligible_items:
        # Provide helpful feedback on why no items were selected
        ralph_enabled_count = sum(
            1 for item in items
            if item.get("ralph_loop", {}).get("enabled", False)
        )
        failing_count = sum(1 for item in items if item.get("status") == "FAILING")

        return {
            "success": True,
            "selected": None,
            "message": "No eligible items for Ralph Loop execution",
            "stats": {
                "total_items": len(items),
                "ralph_enabled": ralph_enabled_count,
                "failing": failing_count,
                "eligible": 0
            }
        }

    # Sort by priority (lowest number = highest priority)
    # Items without priority go to the end
    eligible_items.sort(
        key=lambda x: (
            x["item"].get("priority", float('inf')),
            x["item"].get("id", "")
        )
    )

    # Select first item (highest priority)
    selected = eligible_items[0]
    item = selected["item"]
    dep_check = selected["dependency_check"]

    result = {
        "success": True,
        "selected": {
            "id": item.get("id"),
            "title": item.get("title"),
            "priority": item.get("priority"),
            "ralph_loop": item.get("ralph_loop", {}),
            "prd_reference": item.get("prd_reference"),
            "dependencies": item.get("dependencies", []),
            "acceptance_criteria": item.get("acceptance_criteria", []),
            "verification": item.get("verification", [])
        },
        "dependency_check": dep_check,
        "stats": {
            "total_items": len(items),
            "ralph_enabled": sum(
                1 for item in items
                if item.get("ralph_loop", {}).get("enabled", False)
            ),
            "failing": sum(1 for item in items if item.get("status") == "FAILING"),
            "eligible": len(eligible_items)
        }
    }

    # Add warnings if dependencies have issues
    if dep_check.get("warnings"):
        result["warnings"] = dep_check["warnings"]

    return result

def main():
    """CLI entry point."""
    import argparse

    parser = argparse.ArgumentParser(
        description="Select next item for Ralph Loop execution"
    )
    parser.add_argument(
        "--harness-dir",
        type=Path,
        help="Path to .claude/harness directory (auto-detected if not specified)"
    )

    args = parser.parse_args()

    try:
        harness_dir = args.harness_dir or find_harness_dir()
        result = select_next_item(harness_dir)

        # Pretty print JSON result
        print(json.dumps(result, indent=2))

        # Exit code: 0 if item selected, 1 if no eligible items, 2 on error
        if not result.get("success"):
            sys.exit(2)
        elif result.get("selected") is None:
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
