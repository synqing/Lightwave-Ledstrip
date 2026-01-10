#!/usr/bin/env python3
"""
update_attempt.py - Ralph Loop Attempt Recording

Appends a structured attempt to an item's attempts[] array in feature_list.json.
Implements append-only mutation (never modifies existing attempts).

Usage:
    python3 update_attempt.py ITEM_ID RESULT [OPTIONS]

Arguments:
    ITEM_ID         Feature item ID (e.g., FIX-TEST-ASSERTIONS)
    RESULT          Attempt result: PASSED, FAILED, or IN_PROGRESS

Options:
    --run-id ID     Worker/agent run identifier (default: generated)
    --commands CMD  Comma-separated list of commands run
    --summary TEXT  Results summary (required for PASSED/FAILED)
    --commit SHA    Git commit SHA (optional)
    --reverted      Mark as reverted (for FAILED attempts)
    --harness-dir PATH  Path to .claude/harness directory

Example:
    python3 update_attempt.py FIX-TEST-ASSERTIONS PASSED \\
        --run-id worker-008 \\
        --commands "pio run -e esp32dev_wifi,grep NUM_LEDS" \\
        --summary "Build passes, LED count correct" \\
        --commit abc1234
"""

import json
import sys
from pathlib import Path
from datetime import datetime, timezone
from typing import Dict, List, Any, Optional
import subprocess

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

def get_git_commit_sha(repo_path: Path) -> Optional[str]:
    """Get current git commit SHA."""
    try:
        result = subprocess.run(
            ['git', 'rev-parse', '--short', 'HEAD'],
            cwd=repo_path,
            capture_output=True,
            text=True,
            check=True
        )
        return result.stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None

def generate_run_id() -> str:
    """Generate a default run ID based on timestamp."""
    now = datetime.now(timezone.utc)
    return f"run-{now.strftime('%Y%m%d-%H%M%S')}"

def create_attempt(
    result: str,
    run_id: Optional[str] = None,
    commands: Optional[List[str]] = None,
    summary: Optional[str] = None,
    commit: Optional[str] = None,
    reverted: bool = False,
    repo_path: Optional[Path] = None
) -> Dict[str, Any]:
    """
    Create a structured attempt object.

    Args:
        result: PASSED, FAILED, or IN_PROGRESS
        run_id: Worker/agent identifier
        commands: List of commands executed
        summary: Results summary
        commit: Git commit SHA
        reverted: Whether changes were reverted
        repo_path: Repository path for auto-detecting commit

    Returns:
        Structured attempt dict
    """
    # Validate result
    valid_results = ["PASSED", "FAILED", "IN_PROGRESS"]
    if result not in valid_results:
        raise ValueError(f"result must be one of {valid_results}, got '{result}'")

    # Generate run_id if not provided
    if not run_id:
        run_id = generate_run_id()

    # Auto-detect commit if not provided
    if not commit and repo_path:
        commit = get_git_commit_sha(repo_path)

    # Build attempt object
    attempt = {
        "run_id": run_id,
        "timestamp": datetime.now(timezone.utc).isoformat().replace('+00:00', 'Z'),
        "result": result,
        "evidence": {
            "commands_run": commands or [],
            "results_summary": summary or ""
        }
    }

    # Add optional fields
    if commit:
        attempt["commit"] = commit

    if reverted:
        attempt["reverted"] = True

    return attempt

def update_attempt(
    harness_dir: Path,
    item_id: str,
    result: str,
    **kwargs
) -> Dict[str, Any]:
    """
    Append attempt to item's attempts[] array.

    Args:
        harness_dir: Path to .claude/harness directory
        item_id: Feature item ID
        result: Attempt result (PASSED/FAILED/IN_PROGRESS)
        **kwargs: Additional attempt fields (run_id, commands, summary, etc.)

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

    # Create attempt object
    try:
        # Pass repo_path for auto commit detection
        repo_path = harness_dir.parent.parent  # .claude/harness -> repo root
        attempt = create_attempt(result, repo_path=repo_path, **kwargs)
    except Exception as e:
        return {
            "success": False,
            "error": "invalid_attempt_data",
            "message": str(e)
        }

    # Append to attempts[] array (create if doesn't exist)
    if "attempts" not in item:
        item["attempts"] = []

    item["attempts"].append(attempt)

    # Update item in data
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

    return {
        "success": True,
        "item_id": item_id,
        "attempt": attempt,
        "total_attempts": len(item["attempts"]),
        "message": f"Attempt recorded for {item_id}"
    }

def main():
    """CLI entry point."""
    import argparse

    parser = argparse.ArgumentParser(
        description="Append attempt to item's attempts[] array",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Record successful attempt
  python3 update_attempt.py FIX-TEST-ASSERTIONS PASSED \\
      --run-id worker-008 \\
      --commands "pio run,grep NUM_LEDS" \\
      --summary "Build passes, LED count correct" \\
      --commit abc1234

  # Record failed attempt
  python3 update_attempt.py OTA-UPDATES FAILED \\
      --commands "pio run -e esp32dev_wifi" \\
      --summary "Compilation error in WebServer.cpp" \\
      --reverted

  # Record in-progress attempt
  python3 update_attempt.py HEAP-FRAG-MONITOR IN_PROGRESS \\
      --summary "Investigating heap monitoring code"
        """
    )

    parser.add_argument("item_id", help="Feature item ID")
    parser.add_argument(
        "result",
        choices=["PASSED", "FAILED", "IN_PROGRESS"],
        help="Attempt result"
    )
    parser.add_argument("--run-id", help="Worker/agent run identifier")
    parser.add_argument(
        "--commands",
        help="Comma-separated list of commands executed"
    )
    parser.add_argument("--summary", help="Results summary")
    parser.add_argument("--commit", help="Git commit SHA")
    parser.add_argument(
        "--reverted",
        action="store_true",
        help="Mark as reverted (for failed attempts)"
    )
    parser.add_argument(
        "--harness-dir",
        type=Path,
        help="Path to .claude/harness directory"
    )

    args = parser.parse_args()

    # Validate required fields for PASSED/FAILED
    if args.result in ["PASSED", "FAILED"] and not args.summary:
        parser.error(f"{args.result} result requires --summary")

    try:
        harness_dir = args.harness_dir or find_harness_dir()

        # Parse commands if provided
        commands = None
        if args.commands:
            commands = [cmd.strip() for cmd in args.commands.split(',')]

        result = update_attempt(
            harness_dir,
            args.item_id,
            args.result,
            run_id=args.run_id,
            commands=commands,
            summary=args.summary,
            commit=args.commit,
            reverted=args.reverted
        )

        print(json.dumps(result, indent=2))

        sys.exit(0 if result.get("success") else 1)

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
