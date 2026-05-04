#!/usr/bin/env python3
"""Fail on stale broad native_test harness routes.

The old native_test PlatformIO environment was an ambiguous aggregate that
compiled an incoherent mix of tests and provider objects. Native validation
must use the scoped environments or scripts/native_harness_matrix.py instead.
"""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]

SKIP_FILES = {
    "docs/superpowers/plans/2026-05-05-native-harness-hardening.md",
    "firmware-v3/scripts/check_native_harness_routes.py",
}

TEXT_SUFFIXES = {
    ".cpp",
    ".h",
    ".ini",
    ".md",
    ".py",
    ".sh",
    ".txt",
    ".yaml",
    ".yml",
}

STALE_PATTERNS = (
    re.compile(r"\bpio\s+(?:run|test)\b[^\n]*\s-e\s+native_test(?![A-Za-z0-9_:-])"),
    re.compile(r"^\s*\[env:native_test\]\s*$"),
)


def is_skipped(path: Path) -> bool:
    rel = path.relative_to(REPO_ROOT).as_posix()
    if rel in SKIP_FILES:
        return True
    return rel.startswith("docs/tooling/notebooklm-bundles/")


def iter_text_files() -> list[Path]:
    files: list[Path] = []
    result = subprocess.run(
        ("git", "ls-files", "--cached", "--others", "--exclude-standard"),
        cwd=REPO_ROOT,
        check=True,
        capture_output=True,
        text=True,
    )
    for rel in result.stdout.splitlines():
        path = REPO_ROOT / rel
        if not path.is_file() or is_skipped(path):
            continue
        if path.suffix in TEXT_SUFFIXES:
            files.append(path)
    return files


def main() -> int:
    violations: list[tuple[str, int, str]] = []
    for path in iter_text_files():
        try:
            lines = path.read_text(encoding="utf-8").splitlines()
        except UnicodeDecodeError:
            continue
        rel = path.relative_to(REPO_ROOT).as_posix()
        for lineno, line in enumerate(lines, start=1):
            if any(pattern.search(line) for pattern in STALE_PATTERNS):
                violations.append((rel, lineno, line.strip()))

    if violations:
        print("Stale native_test routes found. Use scoped envs or scripts/native_harness_matrix.py:")
        for rel, lineno, line in violations:
            print(f"{rel}:{lineno}: {line}")
        return 1

    print("PASS: no stale broad native_test routes found")
    return 0


if __name__ == "__main__":
    sys.exit(main())
