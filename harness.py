#!/usr/bin/env python3
"""
Domain Memory Harness - Guidance, Not Gatekeeping

A lightweight helper for managing domain memory files. Validates, suggests,
and assists - but doesn't block unless data loss is at risk.

CLI Usage:
    python harness.py next                    # Show next FAILING item
    python harness.py validate                # Validate feature_list.json
    python harness.py status                  # Show all items by status
    python harness.py lock <agent_id> <feature_id>  # Acquire lock
    python harness.py unlock                  # Release lock
    python harness.py --force unlock          # Force break stale lock

Library Usage:
    from harness import DomainHarness
    harness = DomainHarness()
    harness.load()
    next_item = harness.get_next_failing()
"""

import argparse
import json
import re
import socket
import sys
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional


# ANSI color codes
class Colors:
    RED = "\033[91m"
    GREEN = "\033[92m"
    YELLOW = "\033[93m"
    BLUE = "\033[94m"
    MAGENTA = "\033[95m"
    CYAN = "\033[96m"
    RESET = "\033[0m"
    BOLD = "\033[1m"


@dataclass
class ValidationResult:
    """Results of validation with severity levels."""
    critical: list[str] = field(default_factory=list)   # MUST fix - blocks
    warnings: list[str] = field(default_factory=list)   # Log but continue
    suggestions: list[str] = field(default_factory=list)  # Helpful hints

    @property
    def ok(self) -> bool:
        """True if no critical issues."""
        return len(self.critical) == 0

    def print_report(self):
        """Print colored validation report."""
        if self.critical:
            print(f"\n{Colors.RED}{Colors.BOLD}CRITICAL ({len(self.critical)}):{Colors.RESET}")
            for msg in self.critical:
                print(f"  {Colors.RED}[X] {msg}{Colors.RESET}")

        if self.warnings:
            print(f"\n{Colors.YELLOW}{Colors.BOLD}WARNINGS ({len(self.warnings)}):{Colors.RESET}")
            for msg in self.warnings:
                print(f"  {Colors.YELLOW}[!] {msg}{Colors.RESET}")

        if self.suggestions:
            print(f"\n{Colors.CYAN}{Colors.BOLD}SUGGESTIONS ({len(self.suggestions)}):{Colors.RESET}")
            for msg in self.suggestions:
                print(f"  {Colors.CYAN}[i] {msg}{Colors.RESET}")

        if self.ok and not self.warnings:
            print(f"\n{Colors.GREEN}{Colors.BOLD}[OK] Validation passed{Colors.RESET}")
        elif self.ok:
            print(f"\n{Colors.YELLOW}{Colors.BOLD}[OK] Validation passed with warnings{Colors.RESET}")
        else:
            print(f"\n{Colors.RED}{Colors.BOLD}[FAIL] Validation failed - fix critical issues{Colors.RESET}")


class DomainHarness:
    """Domain memory harness - helper, not enforcer."""

    VALID_STATUSES = {"FAILING", "PASSING", "BLOCKED", "CANCELLED"}
    REQUIRED_ITEM_FIELDS = {"id", "title", "status"}
    LOCK_TIMEOUT_MINUTES = 30

    def __init__(self, root_dir: str = "."):
        self.root_dir = Path(root_dir)
        self.feature_list_path = self.root_dir / "feature_list.json"
        self.progress_path = self.root_dir / "agent-progress.md"
        self.lock_path = self.root_dir / ".harness.lock"
        self._data: Optional[dict] = None

    def load(self) -> dict:
        """Load feature_list.json with validation on read."""
        if self._data is not None:
            return self._data

        try:
            with open(self.feature_list_path, "r") as f:
                self._data = json.load(f)
        except FileNotFoundError:
            raise FileNotFoundError(f"feature_list.json not found at {self.feature_list_path}")
        except json.JSONDecodeError as e:
            raise ValueError(f"CRITICAL: feature_list.json is corrupted: {e}")

        return self._data

    def save(self):
        """Save feature_list.json."""
        if self._data is None:
            raise ValueError("No data loaded - call load() first")

        with open(self.feature_list_path, "w") as f:
            json.dump(self._data, f, indent=2)
            f.write("\n")  # Trailing newline

    def validate(self) -> ValidationResult:
        """Validate current state, return issues by severity."""
        result = ValidationResult()

        try:
            data = self.load()
        except FileNotFoundError:
            result.critical.append("feature_list.json not found")
            return result
        except ValueError as e:
            result.critical.append(str(e))
            return result

        # Check schema version
        if "schema_version" not in data:
            result.warnings.append("Missing schema_version field")

        # Check items exist
        items = data.get("items", [])
        if not items:
            result.warnings.append("No items in feature list")
            return result

        # Validate each item
        seen_ids = set()
        for idx, item in enumerate(items):
            item_id = item.get("id", f"[index {idx}]")

            # Required fields
            for field in self.REQUIRED_ITEM_FIELDS:
                if field not in item:
                    result.warnings.append(f"{item_id}: Missing required field '{field}'")

            # Duplicate ID check
            if item_id in seen_ids:
                result.critical.append(f"Duplicate item ID: {item_id}")
            seen_ids.add(item_id)

            # Valid status
            status = item.get("status", "")
            if status and status not in self.VALID_STATUSES:
                result.warnings.append(f"{item_id}: Invalid status '{status}', treating as FAILING")

            # PASSING requires evidence
            if status == "PASSING":
                attempts = item.get("attempts", [])
                has_evidence = any(
                    a.get("result") == "PASSED" and a.get("evidence")
                    for a in attempts
                )
                if not has_evidence and not item.get("override_reason"):
                    result.critical.append(f"{item_id}: Status is PASSING but no evidence in attempts[]")

            # BLOCKED requires reason
            if status == "BLOCKED":
                has_reason = any(
                    a.get("result") == "BLOCKED" and a.get("failure_reason")
                    for a in item.get("attempts", [])
                ) or item.get("override_reason")
                if not has_reason:
                    result.critical.append(f"{item_id}: Status is BLOCKED but no reason provided")

            # Dependencies exist
            for dep_id in item.get("dependencies", []):
                if dep_id not in seen_ids and not any(i.get("id") == dep_id for i in items):
                    result.warnings.append(f"{item_id}: Dependency '{dep_id}' not found")

        # Check for circular dependencies
        circular = self._detect_circular_dependencies(items)
        if circular:
            result.critical.append(f"Circular dependency detected: {' -> '.join(circular)}")

        # Suggestions
        failing_count = sum(1 for i in items if i.get("status") == "FAILING")
        passing_count = sum(1 for i in items if i.get("status") == "PASSING")
        if failing_count > 0:
            result.suggestions.append(f"{failing_count} FAILING items, {passing_count} PASSING")

        return result

    def _detect_circular_dependencies(self, items: list) -> list[str]:
        """Detect circular dependencies. Returns cycle path if found."""
        id_to_deps = {i.get("id"): i.get("dependencies", []) for i in items}

        def visit(node, path, visited):
            if node in path:
                return path[path.index(node):] + [node]
            if node in visited:
                return []
            visited.add(node)
            path.append(node)
            for dep in id_to_deps.get(node, []):
                cycle = visit(dep, path, visited)
                if cycle:
                    return cycle
            path.pop()
            return []

        visited = set()
        for item_id in id_to_deps:
            cycle = visit(item_id, [], visited)
            if cycle:
                return cycle
        return []

    def get_items_by_status(self, status: str) -> list[dict]:
        """Filter items by status."""
        data = self.load()
        return [i for i in data.get("items", []) if i.get("status") == status]

    def get_next_failing(self, ignore_dependencies: bool = False) -> tuple[Optional[dict], list[str]]:
        """
        Get highest priority FAILING item.
        Returns (item, warnings) tuple. Warnings list includes dependency issues.
        """
        data = self.load()
        items = data.get("items", [])
        failing = [i for i in items if i.get("status") == "FAILING"]

        if not failing:
            return None, []

        # Sort by priority (lower number = higher priority)
        failing.sort(key=lambda x: x.get("priority", 999))

        warnings = []
        status_map = {i.get("id"): i.get("status") for i in items}

        for item in failing:
            deps = item.get("dependencies", [])
            unmet_deps = [d for d in deps if status_map.get(d) != "PASSING"]

            if unmet_deps and not ignore_dependencies:
                dep_warnings = [f"Dependency {d} is {status_map.get(d, 'UNKNOWN')}" for d in unmet_deps]
                warnings.extend(dep_warnings)
                # Still return this item, just with warnings (soft warning, not blocking)

            return item, warnings

        return None, []

    def get_next_run_id(self, mode: str = "worker") -> str:
        """Parse agent-progress.md, return next sequential run ID."""
        try:
            with open(self.progress_path, "r") as f:
                content = f.read()
        except FileNotFoundError:
            return f"{mode}-001"

        # Find all run numbers
        matches = re.findall(r"## Run (\d+)", content)
        if not matches:
            return f"{mode}-001"

        max_num = max(int(m) for m in matches)
        return f"{mode}-{max_num + 1:03d}"

    def record_attempt(self, item_id: str, result: str, evidence: dict = None,
                       notes: str = "", run_id: str = None, commit: str = None):
        """Append attempt to item's attempts[] array."""
        data = self.load()
        items = data.get("items", [])

        item = next((i for i in items if i.get("id") == item_id), None)
        if not item:
            raise ValueError(f"Item not found: {item_id}")

        if "attempts" not in item:
            item["attempts"] = []

        attempt = {
            "run_id": run_id or self.get_next_run_id(),
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "result": result,
            "evidence": evidence or {},
            "commit": commit,
            "reverted": False
        }

        if notes:
            attempt["notes"] = notes

        item["attempts"].append(attempt)
        self.save()

    def update_status(self, item_id: str, new_status: str, evidence: dict = None,
                      reason: str = None) -> ValidationResult:
        """
        Update item status with required evidence.
        Returns ValidationResult - check .ok before proceeding.
        """
        result = ValidationResult()

        if new_status not in self.VALID_STATUSES:
            result.critical.append(f"Invalid status: {new_status}")
            return result

        data = self.load()
        item = next((i for i in data.get("items", []) if i.get("id") == item_id), None)

        if not item:
            result.critical.append(f"Item not found: {item_id}")
            return result

        # Validate transitions
        if new_status == "PASSING" and not evidence and not item.get("override_reason"):
            result.critical.append("Status -> PASSING requires evidence")
            return result

        if new_status == "BLOCKED" and not reason and not item.get("override_reason"):
            result.critical.append("Status -> BLOCKED requires reason")
            return result

        item["status"] = new_status
        self.save()

        return result

    # ==================== CONCURRENCY (Phase 4) ====================

    def check_lock(self) -> Optional[dict]:
        """Check if lock exists and return lock info if so."""
        if not self.lock_path.exists():
            return None

        try:
            with open(self.lock_path, "r") as f:
                lock_data = json.load(f)
        except (json.JSONDecodeError, FileNotFoundError):
            return None

        return lock_data

    def is_lock_stale(self, lock_data: dict) -> bool:
        """Check if lock is older than timeout."""
        try:
            lock_time = datetime.fromisoformat(lock_data.get("timestamp", ""))
            age_minutes = (datetime.now(timezone.utc) - lock_time).total_seconds() / 60
            return age_minutes > self.LOCK_TIMEOUT_MINUTES
        except (ValueError, TypeError):
            return True  # Invalid timestamp = stale

    def acquire_lock(self, agent_id: str, feature_id: str, force: bool = False) -> tuple[bool, list[str]]:
        """
        Acquire lock for modifying feature_list.json.
        Returns (success, warnings).
        """
        warnings = []
        existing = self.check_lock()

        if existing:
            if self.is_lock_stale(existing):
                warnings.append(f"Breaking stale lock from {existing.get('agent_id')} "
                               f"(age > {self.LOCK_TIMEOUT_MINUTES} min)")
            elif force:
                warnings.append(f"Force-breaking lock from {existing.get('agent_id')}")
            else:
                warnings.append(f"Lock held by {existing.get('agent_id')} "
                               f"on {existing.get('feature_id')} - another agent may be working")
                # Still allow - this is advisory, not blocking
                warnings.append("Proceeding anyway (advisory lock)")

        lock_data = {
            "agent_id": agent_id,
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "feature_id": feature_id,
            "hostname": socket.gethostname()
        }

        with open(self.lock_path, "w") as f:
            json.dump(lock_data, f, indent=2)

        return True, warnings

    def release_lock(self) -> bool:
        """Release lock. Returns True if lock was released."""
        if self.lock_path.exists():
            self.lock_path.unlink()
            return True
        return False


# ==================== CLI ====================

def cmd_next(harness: DomainHarness, args):
    """Show next FAILING item."""
    item, warnings = harness.get_next_failing(ignore_dependencies=args.ignore_dependencies)

    if warnings:
        print(f"{Colors.YELLOW}{Colors.BOLD}WARNINGS:{Colors.RESET}")
        for w in warnings:
            print(f"  {Colors.YELLOW}[!] {w}{Colors.RESET}")
        print()

    if not item:
        print(f"{Colors.GREEN}No FAILING items - all done!{Colors.RESET}")
        return 0

    print(f"{Colors.BOLD}Next item:{Colors.RESET}")
    print(f"  {Colors.CYAN}ID:{Colors.RESET} {item.get('id')}")
    print(f"  {Colors.CYAN}Title:{Colors.RESET} {item.get('title')}")
    print(f"  {Colors.CYAN}Priority:{Colors.RESET} {item.get('priority', 'N/A')}")

    if item.get("dependencies"):
        print(f"  {Colors.CYAN}Dependencies:{Colors.RESET} {', '.join(item['dependencies'])}")

    if item.get("acceptance_criteria"):
        print(f"  {Colors.CYAN}Acceptance Criteria:{Colors.RESET}")
        for ac in item["acceptance_criteria"]:
            print(f"    - {ac}")

    attempts = item.get("attempts", [])
    if attempts:
        print(f"  {Colors.CYAN}Previous Attempts:{Colors.RESET} {len(attempts)}")
        last = attempts[-1]
        print(f"    Last: {last.get('result')} ({last.get('run_id')})")

    return 0


def cmd_validate(harness: DomainHarness, args):
    """Validate feature_list.json."""
    result = harness.validate()
    result.print_report()
    return 0 if result.ok else 2


def cmd_status(harness: DomainHarness, args):
    """Show all items by status."""
    data = harness.load()
    items = data.get("items", [])

    by_status = {}
    for item in items:
        status = item.get("status", "UNKNOWN")
        by_status.setdefault(status, []).append(item)

    status_colors = {
        "PASSING": Colors.GREEN,
        "FAILING": Colors.RED,
        "BLOCKED": Colors.YELLOW,
        "CANCELLED": Colors.MAGENTA
    }

    for status in ["FAILING", "PASSING", "BLOCKED", "CANCELLED"]:
        if status in by_status:
            color = status_colors.get(status, "")
            print(f"\n{color}{Colors.BOLD}{status} ({len(by_status[status])}):{Colors.RESET}")
            for item in by_status[status]:
                priority = item.get("priority", "?")
                print(f"  [{priority}] {item.get('id')}: {item.get('title')}")

    return 0


def cmd_lock(harness: DomainHarness, args):
    """Acquire lock."""
    success, warnings = harness.acquire_lock(args.agent_id, args.feature_id, force=args.force)

    if warnings:
        for w in warnings:
            print(f"{Colors.YELLOW}[!] {w}{Colors.RESET}")

    if success:
        print(f"{Colors.GREEN}Lock acquired for {args.feature_id}{Colors.RESET}")
        return 0
    return 1


def cmd_unlock(harness: DomainHarness, args):
    """Release lock."""
    existing = harness.check_lock()

    if existing and not args.force:
        if not harness.is_lock_stale(existing):
            print(f"{Colors.YELLOW}Lock held by {existing.get('agent_id')} - use --force to break{Colors.RESET}")
            # Still release it - advisory lock
            harness.release_lock()
            print(f"{Colors.GREEN}Lock released{Colors.RESET}")
            return 0

    if harness.release_lock():
        print(f"{Colors.GREEN}Lock released{Colors.RESET}")
    else:
        print("No lock to release")
    return 0


def cmd_run_id(harness: DomainHarness, args):
    """Get next run ID."""
    run_id = harness.get_next_run_id(args.mode)
    print(run_id)
    return 0


def main():
    parser = argparse.ArgumentParser(
        description="Domain Memory Harness - Guidance, Not Gatekeeping",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--force", action="store_true", help="Force operation (break locks, skip warnings)")
    parser.add_argument("--dir", default=".", help="Project root directory")

    subparsers = parser.add_subparsers(dest="command", help="Commands")

    # next
    next_parser = subparsers.add_parser("next", help="Show next FAILING item")
    next_parser.add_argument("--ignore-dependencies", action="store_true",
                            help="Don't warn about unmet dependencies")

    # validate
    subparsers.add_parser("validate", help="Validate feature_list.json")

    # status
    subparsers.add_parser("status", help="Show all items by status")

    # lock
    lock_parser = subparsers.add_parser("lock", help="Acquire lock")
    lock_parser.add_argument("agent_id", help="Agent identifier (e.g., worker-005)")
    lock_parser.add_argument("feature_id", help="Feature being worked on")

    # unlock
    subparsers.add_parser("unlock", help="Release lock")

    # run-id
    run_id_parser = subparsers.add_parser("run-id", help="Get next run ID")
    run_id_parser.add_argument("--mode", default="worker", help="Mode (worker/init)")

    args = parser.parse_args()

    if not args.command:
        parser.print_help()
        return 1

    harness = DomainHarness(args.dir)

    commands = {
        "next": cmd_next,
        "validate": cmd_validate,
        "status": cmd_status,
        "lock": cmd_lock,
        "unlock": cmd_unlock,
        "run-id": cmd_run_id
    }

    try:
        return commands[args.command](harness, args)
    except FileNotFoundError as e:
        print(f"{Colors.RED}Error: {e}{Colors.RESET}")
        return 1
    except ValueError as e:
        print(f"{Colors.RED}Error: {e}{Colors.RESET}")
        return 2


if __name__ == "__main__":
    sys.exit(main())
