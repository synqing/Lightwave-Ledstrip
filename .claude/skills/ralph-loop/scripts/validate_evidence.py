#!/usr/bin/env python3
"""
validate_evidence.py - Ralph Loop Evidence Validation

Validates that items marked PASSING have proper evidence in their attempts[] array.
Can be used as pre-flight check before changing status or as audit tool.

Usage:
    python3 validate_evidence.py [ITEM_ID] [OPTIONS]

Arguments:
    ITEM_ID         Optional: Validate specific item (omit to validate all)

Options:
    --strict        Strict mode: fail on any validation issue
    --status STATUS Filter items by status (PASSING, FAILING, BLOCKED, CANCELLED)
    --harness-dir PATH  Path to .claude/harness directory

Examples:
    # Validate all PASSING items (default)
    python3 validate_evidence.py

    # Validate specific item
    python3 validate_evidence.py FIX-TEST-ASSERTIONS

    # Validate all items with strict checking
    python3 validate_evidence.py --strict

    # Audit all FAILING items
    python3 validate_evidence.py --status FAILING

Exit codes:
    0  All validations passed
    1  Validation warnings (or failures in strict mode)
    2  Error (file not found, invalid data, etc.)
"""

import json
import sys
from pathlib import Path
from typing import Dict, List, Any, Optional

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

def validate_attempt_evidence(attempt: Dict, attempt_index: int) -> List[str]:
    """
    Validate evidence structure in a single attempt.

    Args:
        attempt: Attempt object to validate
        attempt_index: Index in attempts[] array (for error reporting)

    Returns:
        List of validation issues (empty if valid)
    """
    issues = []

    # Check required fields
    if "result" not in attempt:
        issues.append(f"Attempt[{attempt_index}]: missing 'result' field")
        return issues  # Can't continue validation without result

    result = attempt["result"]
    valid_results = ["PASSED", "FAILED", "IN_PROGRESS"]
    if result not in valid_results:
        issues.append(
            f"Attempt[{attempt_index}]: invalid result '{result}' "
            f"(must be {valid_results})"
        )

    # Check evidence structure for PASSED/FAILED attempts
    if result in ["PASSED", "FAILED"]:
        if "evidence" not in attempt:
            issues.append(
                f"Attempt[{attempt_index}]: {result} result requires 'evidence' field"
            )
        else:
            evidence = attempt["evidence"]

            # Validate evidence structure
            if not isinstance(evidence, dict):
                issues.append(
                    f"Attempt[{attempt_index}]: 'evidence' must be object/dict"
                )
            else:
                # Check required evidence fields
                if "commands_run" not in evidence:
                    issues.append(
                        f"Attempt[{attempt_index}]: evidence missing 'commands_run'"
                    )
                elif not isinstance(evidence["commands_run"], list):
                    issues.append(
                        f"Attempt[{attempt_index}]: 'commands_run' must be array"
                    )

                if "results_summary" not in evidence:
                    issues.append(
                        f"Attempt[{attempt_index}]: evidence missing 'results_summary'"
                    )
                elif not evidence["results_summary"].strip():
                    issues.append(
                        f"Attempt[{attempt_index}]: 'results_summary' is empty"
                    )

    # Check timestamp format
    if "timestamp" not in attempt:
        issues.append(f"Attempt[{attempt_index}]: missing 'timestamp' field")
    else:
        timestamp = attempt["timestamp"]
        # Basic ISO8601 format check
        if not isinstance(timestamp, str) or len(timestamp) < 19:
            issues.append(
                f"Attempt[{attempt_index}]: invalid timestamp format '{timestamp}'"
            )

    # Check run_id
    if "run_id" not in attempt:
        issues.append(f"Attempt[{attempt_index}]: missing 'run_id' field")

    return issues

def validate_item_evidence(item: Dict) -> Dict[str, Any]:
    """
    Validate evidence for a single item.

    Args:
        item: Item object to validate

    Returns:
        Validation result dict
    """
    item_id = item.get("id", "<unknown>")
    status = item.get("status", "UNKNOWN")
    issues = []
    warnings = []

    # Check if item has attempts
    attempts = item.get("attempts", [])

    if not attempts:
        if status == "PASSING":
            issues.append("PASSING status requires at least one attempt with evidence")
        else:
            warnings.append(f"No attempts recorded (status: {status})")

        return {
            "item_id": item_id,
            "status": status,
            "total_attempts": 0,
            "passed_attempts": 0,
            "issues": issues,
            "warnings": warnings,
            "valid": len(issues) == 0
        }

    # Validate each attempt
    passed_attempts = 0

    for i, attempt in enumerate(attempts):
        attempt_issues = validate_attempt_evidence(attempt, i)
        issues.extend(attempt_issues)

        if attempt.get("result") == "PASSED":
            passed_attempts += 1

    # Check PASSING status requirements
    if status == "PASSING":
        if passed_attempts == 0:
            issues.append(
                "PASSING status requires at least one PASSED attempt in attempts[]"
            )

        # Check if most recent attempt is PASSED
        if attempts:
            last_attempt = attempts[-1]
            if last_attempt.get("result") != "PASSED":
                warnings.append(
                    f"Most recent attempt has result '{last_attempt.get('result')}' "
                    "but status is PASSING"
                )

    return {
        "item_id": item_id,
        "status": status,
        "total_attempts": len(attempts),
        "passed_attempts": passed_attempts,
        "issues": issues,
        "warnings": warnings,
        "valid": len(issues) == 0
    }

def validate_evidence(
    harness_dir: Path,
    item_id: Optional[str] = None,
    status_filter: Optional[str] = None,
    strict: bool = False
) -> Dict[str, Any]:
    """
    Validate evidence in feature_list.json.

    Args:
        harness_dir: Path to .claude/harness directory
        item_id: Optional specific item to validate
        status_filter: Optional status filter (PASSING, FAILING, etc.)
        strict: Strict mode treats warnings as failures

    Returns:
        JSON-serializable validation result
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

    # Filter items
    items_to_validate = items

    if item_id:
        items_to_validate = [item for item in items if item.get("id") == item_id]
        if not items_to_validate:
            return {
                "success": False,
                "error": "item_not_found",
                "message": f"No item with id '{item_id}' in feature_list.json"
            }

    if status_filter:
        items_to_validate = [
            item for item in items_to_validate
            if item.get("status") == status_filter
        ]

    # Validate each item
    results = []
    total_issues = 0
    total_warnings = 0

    for item in items_to_validate:
        validation = validate_item_evidence(item)
        results.append(validation)

        total_issues += len(validation["issues"])
        total_warnings += len(validation["warnings"])

    # Determine overall success
    has_failures = total_issues > 0 or (strict and total_warnings > 0)

    return {
        "success": not has_failures,
        "total_items_validated": len(items_to_validate),
        "total_items_in_file": len(items),
        "valid_items": sum(1 for r in results if r["valid"]),
        "total_issues": total_issues,
        "total_warnings": total_warnings,
        "results": results,
        "message": (
            "All validations passed"
            if not has_failures
            else f"{total_issues} issue(s), {total_warnings} warning(s)"
        )
    }

def main():
    """CLI entry point."""
    import argparse

    parser = argparse.ArgumentParser(
        description="Validate evidence in feature_list.json attempts[] arrays",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Validate all PASSING items
  python3 validate_evidence.py

  # Validate specific item
  python3 validate_evidence.py FIX-TEST-ASSERTIONS

  # Strict validation (treat warnings as errors)
  python3 validate_evidence.py --strict

  # Audit FAILING items
  python3 validate_evidence.py --status FAILING

Exit codes:
  0  All validations passed
  1  Validation issues/warnings found
  2  Error (file not found, etc.)
        """
    )

    parser.add_argument(
        "item_id",
        nargs="?",
        help="Optional: validate specific item (omit to validate all)"
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Strict mode: treat warnings as failures"
    )
    parser.add_argument(
        "--status",
        choices=["PASSING", "FAILING", "BLOCKED", "CANCELLED"],
        help="Filter items by status"
    )
    parser.add_argument(
        "--harness-dir",
        type=Path,
        help="Path to .claude/harness directory"
    )

    args = parser.parse_args()

    try:
        harness_dir = args.harness_dir or find_harness_dir()

        result = validate_evidence(
            harness_dir,
            item_id=args.item_id,
            status_filter=args.status,
            strict=args.strict
        )

        print(json.dumps(result, indent=2))

        # Exit codes
        if not result.get("success"):
            sys.exit(1 if "results" in result else 2)
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
