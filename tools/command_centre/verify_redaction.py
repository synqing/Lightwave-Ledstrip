#!/usr/bin/env python3
"""
Post-Redaction Secret Scanner for CI Verification

Scans audit logs and JSON artifacts after redaction to verify no secrets leaked.
Fails with non-zero exit code if any secret patterns are detected.

Usage:
    python verify_redaction.py [--reports-dir REPORTS_DIR]
"""

import re
import sys
from pathlib import Path
from typing import List, Tuple

# Import patterns from mcp_safety to stay in sync
try:
    from mcp_safety import AuditRedactor
    SECRET_PATTERNS = AuditRedactor.SECRET_PATTERNS
except ImportError:
    # Fallback if import fails
    SECRET_PATTERNS = [
        r'(?i)(api[_-]?key|secret|token|password|auth)\s*[=:]\s*["\']?[^\s"\']{8,}',
        r'sk_live_[a-zA-Z0-9]+',
        r'ghp_[a-zA-Z0-9]+',
        r'(?i)aws[_-]?(access|secret)',
    ]

# Additional patterns for absolute paths that should be redacted
PATH_PATTERNS = [
    r'/Users/[a-zA-Z0-9_-]+/',
    r'/home/[a-zA-Z0-9_-]+/',
]


def scan_file_for_secrets(file_path: Path) -> List[Tuple[int, str, str]]:
    """
    Scan a file for secret patterns.
    
    Returns:
        List of (line_number, pattern_matched, snippet) tuples
    """
    leaks = []
    try:
        content = file_path.read_text(encoding="utf-8")
    except Exception as e:
        print(f"  Warning: Could not read {file_path}: {e}", file=sys.stderr)
        return leaks
    
    lines = content.splitlines()
    
    for line_num, line in enumerate(lines, start=1):
        # Check secret patterns
        for pattern in SECRET_PATTERNS:
            matches = re.finditer(pattern, line, re.IGNORECASE)
            for match in matches:
                # Skip if it's already redacted
                if "[REDACTED]" in match.group(0):
                    continue
                snippet = match.group(0)[:30] + "..." if len(match.group(0)) > 30 else match.group(0)
                leaks.append((line_num, pattern[:40], snippet))
        
        # Check path patterns
        for pattern in PATH_PATTERNS:
            matches = re.finditer(pattern, line)
            for match in matches:
                if "[REDACTED]" in match.group(0):
                    continue
                leaks.append((line_num, "absolute_path", match.group(0)))
    
    return leaks


def verify_redaction(reports_dir: Path) -> bool:
    """
    Verify all files in reports directory have been properly redacted.
    
    Returns:
        True if no secrets found, False otherwise
    """
    all_clean = True
    files_scanned = 0
    total_leaks = 0
    
    # Scan JSON files
    for json_file in reports_dir.rglob("*.json"):
        files_scanned += 1
        leaks = scan_file_for_secrets(json_file)
        if leaks:
            all_clean = False
            total_leaks += len(leaks)
            print(f"LEAK DETECTED in {json_file}:")
            for line_num, pattern, snippet in leaks:
                print(f"  Line {line_num}: [{pattern}] {snippet}")
    
    # Scan JSONL files (audit logs)
    for jsonl_file in reports_dir.rglob("*.jsonl"):
        files_scanned += 1
        leaks = scan_file_for_secrets(jsonl_file)
        if leaks:
            all_clean = False
            total_leaks += len(leaks)
            print(f"LEAK DETECTED in {jsonl_file}:")
            for line_num, pattern, snippet in leaks:
                print(f"  Line {line_num}: [{pattern}] {snippet}")
    
    # Also scan mcp_calls directory if it exists
    mcp_calls_dir = reports_dir.parent / "mcp_calls"
    if mcp_calls_dir.exists():
        for jsonl_file in mcp_calls_dir.rglob("*.jsonl"):
            files_scanned += 1
            leaks = scan_file_for_secrets(jsonl_file)
            if leaks:
                all_clean = False
                total_leaks += len(leaks)
                print(f"LEAK DETECTED in {jsonl_file}:")
                for line_num, pattern, snippet in leaks:
                    print(f"  Line {line_num}: [{pattern}] {snippet}")
    
    if all_clean:
        print(f"OK: Scanned {files_scanned} files, no secrets detected")
    else:
        print(f"FAIL: Found {total_leaks} potential leaks in {files_scanned} files")
    
    return all_clean


def main():
    import argparse
    
    parser = argparse.ArgumentParser(description="Verify redaction of secrets in reports")
    parser.add_argument(
        "--reports-dir",
        type=Path,
        default=Path("../reports"),
        help="Directory containing reports to scan (default: ../reports)",
    )
    args = parser.parse_args()
    
    # Resolve relative to script location if relative
    if not args.reports_dir.is_absolute():
        script_dir = Path(__file__).parent
        reports_dir = (script_dir / args.reports_dir).resolve()
    else:
        reports_dir = args.reports_dir
    
    # If reports directory doesn't exist, that's OK (no artifacts yet)
    if not reports_dir.exists():
        print(f"OK: Reports directory {reports_dir} does not exist (no artifacts to scan)")
        sys.exit(0)
    
    print(f"Scanning {reports_dir} for leaked secrets...")
    
    if verify_redaction(reports_dir):
        sys.exit(0)
    else:
        sys.exit(1)


if __name__ == "__main__":
    main()
