#!/usr/bin/env python3
"""
Context Pack Generator for LightwaveOS

Generates a minimal prompt bundle for delta-only LLM prompting:
- packet.md (from template)
- diff.patch (git diff output, filtered and size-budgeted)
- logs.trimmed.txt (optional log slice)
- fixtures/ directory with auto-TOONified JSON fixtures
- token_report.md with token savings stats
- prompt.md (cache-friendly assembled prompt)

Features:
- Denylist/allowlist via .contextpackignore
- Deterministic output (stable ordering)
- Size budgets (100KB soft, 200KB hard) with auto-split
- Lint mode for pre-commit validation
- Prompt caching optimisation

Usage:
    python tools/contextpack.py
    python tools/contextpack.py --out /tmp/contextpack
    python tools/contextpack.py --logs build.log --lines 100
    python tools/contextpack.py --diff "git diff --cached"
    python tools/contextpack.py --no-toonify  # Skip TOON conversion
    python tools/contextpack.py --lint        # Validate config only

See docs/contextpack/README.md for the full Context Pack pipeline.
"""

import argparse
import fnmatch
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Tuple


# Template paths relative to repo root
REPO_ROOT = Path(__file__).parent.parent
TOOLS_DIR = Path(__file__).parent
PACKET_TEMPLATE = REPO_ROOT / "docs" / "contextpack" / "packet.md"
FIXTURES_README = REPO_ROOT / "docs" / "contextpack" / "fixtures" / "README.md"
LLM_CONTEXT = REPO_ROOT / "docs" / "LLM_CONTEXT.md"
IGNORE_FILE = REPO_ROOT / ".contextpackignore"

# TOON CLI configuration
TOON_MIN_ITEMS_DEFAULT = 5  # Minimum array items to consider for TOONification

# Size budgets (in bytes)
DIFF_SOFT_LIMIT = 100 * 1024  # 100 KB - triggers split
DIFF_HARD_LIMIT = 200 * 1024  # 200 KB - forces split + index
DIFF_CHUNK_SIZE = 80 * 1024   # ~80 KB per chunk

# Token savings guardrail
MIN_TOKEN_SAVINGS_PERCENT = 20.0  # Minimum savings % for TOON conversion to pass lint

# Default exclusion patterns (used if no .contextpackignore exists)
DEFAULT_EXCLUSIONS = [
    # Binaries
    "*.bin", "*.elf", "*.a", "*.o", "*.so", "*.dylib", "*.exe", "*.hex",
    # Media
    "*.png", "*.jpg", "*.jpeg", "*.gif", "*.mp4", "*.mp3", "*.wav", "*.pdf",
    # Archives
    "*.zip", "*.tar", "*.gz", "*.7z",
    # Build artifacts
    "build/", "dist/", ".pio/", "node_modules/", "__pycache__/", "*.pyc",
    # Debug
    "*.map", "*.dSYM/",
    # Secrets (CRITICAL)
    "*.env", "*.env.*", "*credentials*", "*secret*", "*token*", "*.pem", "*.key",
    "wifi_credentials.ini",
    # IDE
    ".idea/", ".vscode/", "*.swp", ".DS_Store",
    # Lock files
    "package-lock.json", "yarn.lock",
    # Generated context packs
    "contextpack/",
]

# Secret patterns for lint mode
SECRET_PATTERNS = [
    r'(?i)(api[_-]?key|apikey)\s*[=:]\s*["\']?[a-zA-Z0-9_\-]{20,}',
    r'(?i)(secret|password|passwd|pwd)\s*[=:]\s*["\']?[^\s"\']{8,}',
    r'(?i)(token|auth)\s*[=:]\s*["\']?[a-zA-Z0-9_\-]{20,}',
    r'(?i)bearer\s+[a-zA-Z0-9_\-\.]{20,}',
    r'-----BEGIN\s+(RSA\s+)?PRIVATE\s+KEY-----',
    r'(?i)aws[_-]?(access[_-]?key|secret)',
]


def load_ignore_patterns() -> List[str]:
    """Load exclusion patterns from .contextpackignore or use defaults."""
    if IGNORE_FILE.exists():
        patterns = []
        try:
            with open(IGNORE_FILE, "r", encoding="utf-8") as f:
                for line in f:
                    line = line.strip()
                    # Skip empty lines and comments
                    if line and not line.startswith("#"):
                        patterns.append(line)
            return patterns
        except IOError:
            pass
    return DEFAULT_EXCLUSIONS


def matches_pattern(path: str, patterns: List[str]) -> bool:
    """Check if a path matches any of the exclusion patterns."""
    path_lower = path.lower()
    for pattern in patterns:
        pattern_lower = pattern.lower()
        # Handle directory patterns (ending with /)
        if pattern_lower.endswith("/"):
            dir_pattern = pattern_lower.rstrip("/")
            if f"/{dir_pattern}/" in f"/{path_lower}/" or path_lower.startswith(f"{dir_pattern}/"):
                return True
        # Handle glob patterns
        elif fnmatch.fnmatch(path_lower, pattern_lower):
            return True
        elif fnmatch.fnmatch(os.path.basename(path_lower), pattern_lower):
            return True
    return False


def filter_diff_content(diff_content: str, patterns: List[str]) -> Tuple[str, List[str]]:
    """
    Filter diff content to exclude files matching patterns.
    Returns (filtered_diff, list_of_excluded_files).
    """
    if not diff_content.strip():
        return diff_content, []
    
    excluded_files = []
    filtered_chunks = []
    current_chunk = []
    current_file = None
    
    for line in diff_content.split("\n"):
        # Detect new file in diff
        if line.startswith("diff --git"):
            # Save previous chunk if not excluded
            if current_chunk and current_file and not matches_pattern(current_file, patterns):
                filtered_chunks.append("\n".join(current_chunk))
            elif current_file and matches_pattern(current_file, patterns):
                excluded_files.append(current_file)
            
            # Start new chunk
            current_chunk = [line]
            # Extract file path from "diff --git a/path b/path"
            match = re.search(r'diff --git a/(.+?) b/', line)
            current_file = match.group(1) if match else None
        else:
            current_chunk.append(line)
    
    # Handle last chunk
    if current_chunk and current_file and not matches_pattern(current_file, patterns):
        filtered_chunks.append("\n".join(current_chunk))
    elif current_file and matches_pattern(current_file, patterns):
        excluded_files.append(current_file)
    
    return "\n".join(filtered_chunks), sorted(set(excluded_files))


def sort_diff_chunks(diff_content: str) -> str:
    """Sort diff chunks by file path for deterministic output."""
    if not diff_content.strip():
        return diff_content
    
    chunks = []
    current_chunk = []
    current_file = ""
    
    for line in diff_content.split("\n"):
        if line.startswith("diff --git"):
            if current_chunk:
                chunks.append((current_file, "\n".join(current_chunk)))
            current_chunk = [line]
            match = re.search(r'diff --git a/(.+?) b/', line)
            current_file = match.group(1) if match else ""
        else:
            current_chunk.append(line)
    
    if current_chunk:
        chunks.append((current_file, "\n".join(current_chunk)))
    
    # Sort by file path
    chunks.sort(key=lambda x: x[0])
    
    return "\n".join(chunk[1] for chunk in chunks)


def split_diff(diff_content: str, chunk_size: int = DIFF_CHUNK_SIZE) -> List[Tuple[str, str]]:
    """
    Split diff into chunks of approximately chunk_size bytes.
    Returns list of (filename, content) tuples.
    """
    if len(diff_content.encode("utf-8")) <= chunk_size:
        return [("diff.patch", diff_content)]
    
    chunks = []
    current_chunk = []
    current_size = 0
    chunk_num = 1
    
    # Split by file boundaries
    file_chunks = []
    current_file_chunk = []
    
    for line in diff_content.split("\n"):
        if line.startswith("diff --git") and current_file_chunk:
            file_chunks.append("\n".join(current_file_chunk))
            current_file_chunk = [line]
        else:
            current_file_chunk.append(line)
    
    if current_file_chunk:
        file_chunks.append("\n".join(current_file_chunk))
    
    # Group file chunks into size-limited parts
    for file_chunk in file_chunks:
        chunk_bytes = len(file_chunk.encode("utf-8"))
        
        if current_size + chunk_bytes > chunk_size and current_chunk:
            chunks.append((f"diff_part_{chunk_num:02d}.patch", "\n".join(current_chunk)))
            chunk_num += 1
            current_chunk = [file_chunk]
            current_size = chunk_bytes
        else:
            current_chunk.append(file_chunk)
            current_size += chunk_bytes
    
    if current_chunk:
        chunks.append((f"diff_part_{chunk_num:02d}.patch", "\n".join(current_chunk)))
    
    return chunks


def generate_diff_index(chunks: List[Tuple[str, str]], excluded_files: List[str]) -> str:
    """Generate an index of diff parts and their contents."""
    lines = [
        "# Diff Index",
        "",
        f"**Generated**: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
        "",
        "## Parts",
        "",
        "| Part | Size | Files |",
        "|------|------|-------|",
    ]
    
    for filename, content in chunks:
        size_kb = len(content.encode("utf-8")) / 1024
        # Count files in this chunk
        file_count = content.count("diff --git")
        lines.append(f"| {filename} | {size_kb:.1f} KB | {file_count} files |")
    
    if excluded_files:
        lines.extend([
            "",
            "## Excluded Files",
            "",
            "The following files were excluded by `.contextpackignore`:",
            "",
        ])
        for f in excluded_files[:20]:  # Limit to first 20
            lines.append(f"- `{f}`")
        if len(excluded_files) > 20:
            lines.append(f"- ... and {len(excluded_files) - 20} more")
    
    lines.extend([
        "",
        "---",
        "",
        "*To request a specific part, ask for it by name (e.g., \"show me diff_part_02.patch\")*",
    ])
    
    return "\n".join(lines)


def get_git_diff(diff_command: str = "git diff") -> str:
    """Run git diff and return the output."""
    try:
        result = subprocess.run(
            diff_command,
            shell=True,
            capture_output=True,
            text=True,
            cwd=REPO_ROOT,
        )
        return result.stdout
    except subprocess.SubprocessError as e:
        print(f"Warning: Failed to run git diff: {e}", file=sys.stderr)
        return ""


def trim_logs(log_path: Path, lines: int = 100) -> str:
    """Read the last N lines from a log file."""
    if not log_path.exists():
        print(f"Warning: Log file not found: {log_path}", file=sys.stderr)
        return ""
    
    try:
        with open(log_path, "r", encoding="utf-8", errors="replace") as f:
            all_lines = f.readlines()
            trimmed = all_lines[-lines:] if len(all_lines) > lines else all_lines
            return "".join(trimmed)
    except IOError as e:
        print(f"Warning: Failed to read log file: {e}", file=sys.stderr)
        return ""


def copy_template(src: Path, dest: Path, replacements: dict = None) -> None:
    """Copy a template file, optionally replacing placeholders."""
    if not src.exists():
        print(f"Warning: Template not found: {src}", file=sys.stderr)
        return
    
    content = src.read_text(encoding="utf-8")
    
    if replacements:
        for key, value in replacements.items():
            content = content.replace(key, value)
    
    dest.write_text(content, encoding="utf-8")


def is_toon_eligible(data: any, min_items: int = TOON_MIN_ITEMS_DEFAULT) -> bool:
    """
    Check if JSON data is eligible for TOON conversion.
    
    Criteria (from TOON spec):
    - Top-level is a list/array
    - Each element is an object/dict
    - Keys are consistent across the list (same key set)
    - Array has at least min_items elements
    """
    if not isinstance(data, list):
        return False
    
    if len(data) < min_items:
        return False
    
    if not all(isinstance(item, dict) for item in data):
        return False
    
    # Check key consistency
    if not data:
        return False
    
    first_keys = set(data[0].keys())
    for item in data[1:]:
        if set(item.keys()) != first_keys:
            return False
    
    return True


def check_toon_cli_available() -> bool:
    """Check if the TOON CLI is available via npm exec."""
    try:
        result = subprocess.run(
            ["npm", "--prefix", str(TOOLS_DIR), "exec", "--", "toon", "--version"],
            capture_output=True,
            text=True,
            cwd=REPO_ROOT,
            timeout=30,
        )
        return result.returncode == 0
    except (subprocess.SubprocessError, FileNotFoundError):
        return False


def run_toon_convert(json_path: Path, toon_path: Path) -> bool:
    """Convert a JSON file to TOON format using the CLI."""
    try:
        result = subprocess.run(
            ["npm", "--prefix", str(TOOLS_DIR), "exec", "--", "toon", str(json_path), "-o", str(toon_path)],
            capture_output=True,
            text=True,
            cwd=REPO_ROOT,
            timeout=60,
        )
        return result.returncode == 0
    except (subprocess.SubprocessError, FileNotFoundError) as e:
        print(f"Warning: TOON conversion failed for {json_path.name}: {e}", file=sys.stderr)
        return False


def run_toon_stats(json_path: Path) -> Optional[Dict]:
    """
    Run TOON CLI with --stats to get token statistics.
    
    Returns dict with keys: json_tokens, toon_tokens, savings_percent
    """
    try:
        result = subprocess.run(
            ["npm", "--prefix", str(TOOLS_DIR), "exec", "--", "toon", str(json_path), "--stats"],
            capture_output=True,
            text=True,
            cwd=REPO_ROOT,
            timeout=60,
        )
        
        if result.returncode != 0:
            return None
        
        # Parse stats output from CLI
        # Format: "[info] Token estimates: ~336 (JSON) → ~62 (TOON)"
        #         "[success] Saved ~274 tokens (-81.5%)"
        output = result.stdout + result.stderr
        
        stats = {}
        
        # Look for token counts in output
        # Pattern: "~336 (JSON) → ~62 (TOON)"
        json_match = re.search(r'~(\d+)\s*\(JSON\)', output)
        toon_match = re.search(r'~(\d+)\s*\(TOON\)', output)
        # Pattern: "(-81.5%)" or "Saved ~274 tokens (-81.5%)"
        savings_match = re.search(r'\(-?(\d+(?:\.\d+)?)\s*%\)', output)
        
        if json_match:
            stats['json_tokens'] = int(json_match.group(1))
        if toon_match:
            stats['toon_tokens'] = int(toon_match.group(1))
        if savings_match:
            stats['savings_percent'] = float(savings_match.group(1))
        
        # If we got token counts but not savings, calculate it
        if 'json_tokens' in stats and 'toon_tokens' in stats and 'savings_percent' not in stats:
            if stats['json_tokens'] > 0:
                stats['savings_percent'] = round(
                    (1 - stats['toon_tokens'] / stats['json_tokens']) * 100, 1
                )
        
        return stats if stats else None
        
    except (subprocess.SubprocessError, FileNotFoundError) as e:
        print(f"Warning: TOON stats failed for {json_path.name}: {e}", file=sys.stderr)
        return None


def toonify_fixtures(
    fixtures_dir: Path,
    min_items: int = TOON_MIN_ITEMS_DEFAULT,
    collect_stats: bool = True,
) -> List[Dict]:
    """
    Convert eligible JSON fixtures to TOON format.
    
    Returns list of stats dicts for each converted file.
    """
    stats_list = []
    
    # Check if TOON CLI is available
    if not check_toon_cli_available():
        print("  Warning: TOON CLI not available. Run 'npm --prefix tools install' first.", file=sys.stderr)
        return stats_list
    
    # Find all JSON files in fixtures directory
    json_files = list(fixtures_dir.glob("*.json"))
    
    if not json_files:
        return stats_list
    
    print(f"\n  TOONifying fixtures ({len(json_files)} JSON files found):")
    
    for json_path in json_files:
        try:
            with open(json_path, "r", encoding="utf-8") as f:
                data = json.load(f)
        except (json.JSONDecodeError, IOError) as e:
            print(f"    Skipped {json_path.name}: Invalid JSON ({e})")
            continue
        
        # Check eligibility
        if not is_toon_eligible(data, min_items):
            item_count = len(data) if isinstance(data, list) else 0
            reason = "not a uniform array" if not isinstance(data, list) else f"only {item_count} items"
            print(f"    Skipped {json_path.name}: {reason}")
            continue
        
        # Convert to TOON
        toon_path = json_path.with_suffix(".toon")
        if run_toon_convert(json_path, toon_path):
            print(f"    Created {toon_path.name}")
            
            # Collect stats if requested
            if collect_stats:
                stats = run_toon_stats(json_path)
                if stats:
                    stats['file'] = json_path.name
                    stats['toon_file'] = toon_path.name
                    stats_list.append(stats)
        else:
            print(f"    Failed to convert {json_path.name}")
    
    return stats_list


def generate_token_report(out_dir: Path, stats_list: List[Dict], skipped_list: List[Dict] = None) -> Optional[Path]:
    """Generate a token_report.md with conversion statistics."""
    if not stats_list and not skipped_list:
        return None
    
    report_path = out_dir / "token_report.md"
    
    lines = [
        "# Token Report",
        "",
        f"**Generated**: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
        "",
    ]
    
    if stats_list:
        lines.extend([
            "## Conversion Summary",
            "",
            "| Source (JSON) | Output (TOON) | JSON Tokens | TOON Tokens | Savings |",
            "|---------------|---------------|-------------|-------------|---------|",
        ])
        
        total_json = 0
        total_toon = 0
        
        for stats in stats_list:
            json_tokens = stats.get('json_tokens', '?')
            toon_tokens = stats.get('toon_tokens', '?')
            savings = stats.get('savings_percent', '?')
            
            if isinstance(json_tokens, int):
                total_json += json_tokens
            if isinstance(toon_tokens, int):
                total_toon += toon_tokens
            
            savings_str = f"{savings}%" if isinstance(savings, (int, float)) else savings
            
            lines.append(
                f"| {stats.get('file', '?')} | {stats.get('toon_file', '?')} | "
                f"{json_tokens} | {toon_tokens} | {savings_str} |"
            )
        
        # Add totals
        if total_json > 0 and total_toon > 0:
            total_savings = round((1 - total_toon / total_json) * 100, 1)
            lines.extend([
                "",
                "## Totals",
                "",
                f"- **Total JSON tokens**: {total_json}",
                f"- **Total TOON tokens**: {total_toon}",
                f"- **Total savings**: {total_savings}% ({total_json - total_toon} tokens)",
            ])
    
    if skipped_list:
        lines.extend([
            "",
            "## Skipped Files",
            "",
            "| File | Reason |",
            "|------|--------|",
        ])
        for item in skipped_list:
            lines.append(f"| {item.get('file', '?')} | {item.get('reason', '?')} |")
    
    lines.extend([
        "",
        "---",
        "",
        "*Generated by `tools/contextpack.py` using `@toon-format/cli@2.1.0`*",
    ])
    
    report_path.write_text("\n".join(lines), encoding="utf-8")
    return report_path


def check_for_secrets(content: str) -> List[str]:
    """Check content for potential secrets. Returns list of matched patterns."""
    matches = []
    for pattern in SECRET_PATTERNS:
        if re.search(pattern, content):
            matches.append(pattern)
    return matches


def run_lint() -> bool:
    """
    Lint mode: validate configuration and templates.
    Returns True if all checks pass, False otherwise.
    """
    errors = []
    warnings = []
    
    print("Running Context Pack lint checks...")
    print()
    
    # Check 1: Template files exist
    print("  [1/5] Checking template files...")
    if not PACKET_TEMPLATE.exists():
        errors.append(f"Template not found: {PACKET_TEMPLATE}")
    else:
        print(f"    OK: {PACKET_TEMPLATE.relative_to(REPO_ROOT)}")
    
    if not FIXTURES_README.exists():
        warnings.append(f"Fixtures README not found: {FIXTURES_README}")
    else:
        print(f"    OK: {FIXTURES_README.relative_to(REPO_ROOT)}")
    
    if not LLM_CONTEXT.exists():
        warnings.append(f"LLM context file not found: {LLM_CONTEXT}")
    else:
        print(f"    OK: {LLM_CONTEXT.relative_to(REPO_ROOT)}")
    
    # Check 2: Ignore file syntax
    print("  [2/5] Checking .contextpackignore...")
    if IGNORE_FILE.exists():
        patterns = load_ignore_patterns()
        print(f"    OK: {len(patterns)} patterns loaded")
    else:
        print(f"    INFO: Using {len(DEFAULT_EXCLUSIONS)} default exclusions")
    
    # Check 3: TOON CLI availability
    print("  [3/5] Checking TOON CLI...")
    if check_toon_cli_available():
        print("    OK: TOON CLI available")
    else:
        warnings.append("TOON CLI not available. Run 'npm --prefix tools install'")
    
    # Check 4: Check staged files for secrets
    print("  [4/5] Checking staged files for secrets...")
    try:
        result = subprocess.run(
            ["git", "diff", "--cached", "--name-only"],
            capture_output=True,
            text=True,
            cwd=REPO_ROOT,
        )
        staged_files = [f for f in result.stdout.strip().split("\n") if f]
        
        secret_files = []
        for filepath in staged_files:
            full_path = REPO_ROOT / filepath
            if full_path.exists() and full_path.is_file():
                try:
                    content = full_path.read_text(encoding="utf-8", errors="ignore")
                    secrets = check_for_secrets(content)
                    if secrets:
                        secret_files.append(filepath)
                except Exception:
                    pass
        
        if secret_files:
            errors.append(f"Potential secrets detected in staged files: {', '.join(secret_files)}")
        else:
            print(f"    OK: {len(staged_files)} staged files checked, no secrets detected")
    except Exception as e:
        warnings.append(f"Could not check staged files: {e}")
    
    # Check 5: Token savings guardrail (if TOON CLI available and fixtures exist)
    print("  [5/5] Checking token savings guardrail...")
    if check_toon_cli_available():
        # Check for generated context pack with fixtures
        contextpack_dir = REPO_ROOT / "contextpack"
        fixtures_dir = contextpack_dir / "fixtures"
        token_report_path = contextpack_dir / "token_report.md"
        
        if fixtures_dir.exists():
            json_files = list(fixtures_dir.glob("*.json"))
            eligible_files = []
            
            for json_path in json_files:
                try:
                    with open(json_path, "r", encoding="utf-8") as f:
                        data = json.load(f)
                    if is_toon_eligible(data):
                        eligible_files.append(json_path)
                except (json.JSONDecodeError, IOError):
                    pass
            
            if eligible_files:
                # Check if token report exists
                if not token_report_path.exists():
                    errors.append(f"token_report.md missing: eligible fixtures exist in {fixtures_dir} but no token report generated")
                else:
                    # Parse token report to check savings
                    try:
                        report_content = token_report_path.read_text(encoding="utf-8")
                        # Extract savings percentages from report
                        savings_matches = re.findall(r'\|\s*\w+\.\w+\s*\|\s*\w+\.\w+\s*\|\s*(\d+)\s*\|\s*(\d+)\s*\|\s*([\d.]+)%', report_content)
                        
                        if savings_matches:
                            low_savings = []
                            for json_tok, toon_tok, savings_pct in savings_matches:
                                try:
                                    savings = float(savings_pct)
                                    if savings < MIN_TOKEN_SAVINGS_PERCENT:
                                        low_savings.append(f"{savings}% (below {MIN_TOKEN_SAVINGS_PERCENT}% threshold)")
                                except (ValueError, TypeError):
                                    pass
                            
                            if low_savings:
                                errors.append(f"TOON savings below {MIN_TOKEN_SAVINGS_PERCENT}% threshold: {', '.join(low_savings)}")
                            else:
                                avg_savings = sum(float(m[2]) for m in savings_matches) / len(savings_matches)
                                print(f"    OK: Average savings {avg_savings:.1f}% (threshold: {MIN_TOKEN_SAVINGS_PERCENT}%)")
                        else:
                            # Couldn't parse report - warn but don't fail
                            warnings.append("Could not parse token_report.md to validate savings")
                    except IOError:
                        warnings.append(f"Could not read token_report.md: {token_report_path}")
            else:
                print(f"    INFO: No eligible fixtures found in {fixtures_dir}")
        else:
            print(f"    INFO: No fixtures directory found (checked {fixtures_dir})")
    else:
        print(f"    INFO: TOON CLI not available (skipping savings guardrail)")
    
    # Print results
    print()
    if errors:
        print("ERRORS:")
        for e in errors:
            print(f"  - {e}")
    
    if warnings:
        print("WARNINGS:")
        for w in warnings:
            print(f"  - {w}")
    
    if not errors and not warnings:
        print("All checks passed!")
    
    return len(errors) == 0


def generate_prompt_md(out_dir: Path, packet_content: str, diff_size: int, fixtures_count: int) -> Path:
    """
    Generate a cache-friendly prompt.md with stable prefix first.
    """
    prompt_path = out_dir / "prompt.md"
    
    lines = [
        "# Context Pack Prompt",
        "",
        "<!-- CACHE-FRIENDLY STRUCTURE: Stable content first, volatile content last -->",
        "",
        "---",
        "",
        "## 1. Stable Context (Cache This)",
        "",
    ]
    
    # Include LLM_CONTEXT.md content if it exists
    if LLM_CONTEXT.exists():
        try:
            context_content = LLM_CONTEXT.read_text(encoding="utf-8")
            lines.append(context_content)
        except IOError:
            lines.append("*LLM context file not available*")
    else:
        lines.append("*LLM context file not found*")
    
    lines.extend([
        "",
        "---",
        "",
        "## 2. Semi-Stable: Packet Template",
        "",
        packet_content,
        "",
        "---",
        "",
        "## 3. Volatile: Diff + Fixtures",
        "",
        f"**Diff size**: {diff_size / 1024:.1f} KB",
        f"**Fixtures**: {fixtures_count} files",
        "",
        "See attached files:",
        "- `diff.patch` (or `diff_part_*.patch` if split)",
        "- `fixtures/*.toon` / `fixtures/*.json`",
        "- `token_report.md` (if fixtures were converted)",
        "",
        "---",
        "",
        "## 4. Your Ask",
        "",
        "<!-- Replace this with your specific request -->",
        "",
        "[Describe what you need help with]",
        "",
        "---",
        "",
        "*Generated by `tools/contextpack.py`*",
    ])
    
    prompt_path.write_text("\n".join(lines), encoding="utf-8")
    return prompt_path


def generate_cache_notes(out_dir: Path) -> Path:
    """Generate cache_notes.md explaining prompt caching strategy."""
    notes_path = out_dir / "cache_notes.md"
    
    content = """# Prompt Cache Notes

## Cache-Friendly Structure

The `prompt.md` file is structured for optimal prompt caching:

1. **Stable prefix** (Section 1): LLM context that rarely changes
2. **Semi-stable** (Section 2): Packet template with placeholders
3. **Volatile tail** (Section 3+): Diff, fixtures, and your specific ask

## Why This Matters

### OpenAI Prompt Caching
- Automatically caches repeated prompt prefixes (≥1024 tokens)
- Cached prefixes reduce cost and latency on subsequent calls
- Keep stable content at the START of your prompt

### Anthropic Prompt Caching
- Supports explicit cache control blocks via API
- Cache stable prefixes to avoid re-processing
- Particularly effective for long context windows

## Best Practices

1. **Don't modify Section 1** unless the project fundamentally changes
2. **Fill in Section 2** (packet) with your specific goal/symptom
3. **Attach Section 3** files as needed (diff, fixtures)
4. **Write Section 4** (your ask) last

## Maximising Cache Hits

- Use the same `docs/LLM_CONTEXT.md` across all prompts
- Keep packet template structure consistent
- Only the volatile tail should change between prompts

---

*See OpenAI and Anthropic documentation for API-specific caching features.*
"""
    
    notes_path.write_text(content, encoding="utf-8")
    return notes_path


def generate_context_pack(
    out_dir: Path,
    diff_command: str = "git diff",
    log_path: Path = None,
    log_lines: int = 100,
    toonify: bool = True,
    toon_stats: bool = True,
    toon_min_items: int = TOON_MIN_ITEMS_DEFAULT,
    generate_prompt: bool = True,
) -> Dict:
    """
    Generate a context pack in the specified directory.
    Returns summary dict with stats for Claude-Flow integration.
    """
    summary = {
        "output_dir": str(out_dir),
        "diff_size": 0,
        "diff_files": 0,
        "diff_parts": 1,
        "excluded_files": 0,
        "fixtures_converted": 0,
        "fixtures_skipped": 0,
        "token_savings_percent": 0,
    }
    
    # Create output directory
    out_dir.mkdir(parents=True, exist_ok=True)
    fixtures_dir = out_dir / "fixtures"
    fixtures_dir.mkdir(exist_ok=True)
    
    print(f"Generating context pack in: {out_dir}")
    
    # 1. Copy packet template with date replacement
    packet_dest = out_dir / "packet.md"
    today = datetime.now().strftime("%Y-%m-%d")
    copy_template(PACKET_TEMPLATE, packet_dest, {"YYYY-MM-DD": today})
    print(f"  Created: {packet_dest.name}")
    
    # 2. Generate diff.patch with filtering and size budgets
    print("  Processing diff...")
    
    # Load exclusion patterns
    patterns = load_ignore_patterns()
    
    # Get raw diff
    diff_output = get_git_diff(diff_command)
    
    if diff_output.strip():
        # Filter excluded files
        filtered_diff, excluded_files = filter_diff_content(diff_output, patterns)
        summary["excluded_files"] = len(excluded_files)
        
        if excluded_files:
            print(f"    Excluded {len(excluded_files)} files by .contextpackignore")
        
        # Sort for deterministic output
        filtered_diff = sort_diff_chunks(filtered_diff)
        
        diff_size = len(filtered_diff.encode("utf-8"))
        summary["diff_size"] = diff_size
        summary["diff_files"] = filtered_diff.count("diff --git")
        
        # Check size budgets
        if diff_size > DIFF_SOFT_LIMIT:
            print(f"    Diff size ({diff_size / 1024:.1f} KB) exceeds soft limit ({DIFF_SOFT_LIMIT / 1024:.0f} KB), splitting...")
            
            chunks = split_diff(filtered_diff, DIFF_CHUNK_SIZE)
            summary["diff_parts"] = len(chunks)
            
            for filename, content in chunks:
                chunk_path = out_dir / filename
                chunk_path.write_text(content, encoding="utf-8")
                chunk_size = len(content.encode("utf-8"))
                print(f"    Created: {filename} ({chunk_size / 1024:.1f} KB)")
            
            # Generate index if hard limit exceeded
            if diff_size > DIFF_HARD_LIMIT:
                index_content = generate_diff_index(chunks, excluded_files)
                index_path = out_dir / "diff_index.md"
                index_path.write_text(index_content, encoding="utf-8")
                print(f"    Created: diff_index.md")
        else:
            # Single diff file
            diff_dest = out_dir / "diff.patch"
            diff_dest.write_text(filtered_diff, encoding="utf-8")
            print(f"    Created: {diff_dest.name} ({diff_size / 1024:.1f} KB, {summary['diff_files']} files)")
    else:
        diff_dest = out_dir / "diff.patch"
        diff_dest.write_text("# No changes detected\n", encoding="utf-8")
        print(f"    Created: {diff_dest.name} (empty - no changes)")
    
    # 3. Generate logs.trimmed.txt (if log path provided)
    if log_path:
        logs_output = trim_logs(log_path, log_lines)
        logs_dest = out_dir / "logs.trimmed.txt"
        if logs_output.strip():
            logs_dest.write_text(logs_output, encoding="utf-8")
            print(f"  Created: {logs_dest.name} (last {log_lines} lines)")
        else:
            logs_dest.write_text("# No logs available\n", encoding="utf-8")
            print(f"  Created: {logs_dest.name} (empty)")
    
    # 4. Copy fixtures README
    fixtures_readme_dest = fixtures_dir / "README.md"
    copy_template(FIXTURES_README, fixtures_readme_dest)
    print(f"  Created: fixtures/{fixtures_readme_dest.name}")
    
    # 5. TOONify JSON fixtures (if enabled)
    stats_list = []
    skipped_list = []
    if toonify:
        stats_list, skipped_list = toonify_fixtures_with_report(
            fixtures_dir,
            min_items=toon_min_items,
            collect_stats=toon_stats,
        )
        summary["fixtures_converted"] = len(stats_list)
        summary["fixtures_skipped"] = len(skipped_list)
        
        # Calculate average savings
        if stats_list:
            total_savings = sum(s.get("savings_percent", 0) for s in stats_list)
            summary["token_savings_percent"] = round(total_savings / len(stats_list), 1)
    
    # 6. Generate token report (if stats collected)
    if toon_stats and (stats_list or skipped_list):
        report_path = generate_token_report(out_dir, stats_list, skipped_list)
        if report_path:
            print(f"  Created: {report_path.name}")
    
    # 7. Generate prompt.md (cache-friendly structure)
    if generate_prompt:
        packet_content = ""
        if packet_dest.exists():
            packet_content = packet_dest.read_text(encoding="utf-8")
        
        fixtures_count = len(list(fixtures_dir.glob("*.toon"))) + len(list(fixtures_dir.glob("*.json")))
        prompt_path = generate_prompt_md(out_dir, packet_content, summary["diff_size"], fixtures_count)
        print(f"  Created: {prompt_path.name}")
        
        # Generate cache notes
        cache_notes_path = generate_cache_notes(out_dir)
        print(f"  Created: {cache_notes_path.name}")
    
    # 8. Print summary
    if LLM_CONTEXT.exists():
        print(f"\nStable context file: {LLM_CONTEXT.relative_to(REPO_ROOT)}")
    
    print(f"\nContext pack ready. Fill in packet.md and add fixtures as needed.")
    
    # Print compact summary for Claude-Flow
    print(f"\n--- Summary ---")
    print(f"  Diff: {summary['diff_size'] / 1024:.1f} KB ({summary['diff_files']} files, {summary['diff_parts']} part(s))")
    if summary["excluded_files"]:
        print(f"  Excluded: {summary['excluded_files']} files")
    if summary["fixtures_converted"]:
        print(f"  Fixtures: {summary['fixtures_converted']} converted ({summary['token_savings_percent']}% avg savings)")
    print(f"  Output: {out_dir}")
    
    return summary


def toonify_fixtures_with_report(
    fixtures_dir: Path,
    min_items: int = TOON_MIN_ITEMS_DEFAULT,
    collect_stats: bool = True,
) -> Tuple[List[Dict], List[Dict]]:
    """
    Convert eligible JSON fixtures to TOON format.
    Returns (stats_list, skipped_list) for reporting.
    """
    stats_list = []
    skipped_list = []
    
    # Check if TOON CLI is available
    if not check_toon_cli_available():
        print("  Warning: TOON CLI not available. Run 'npm --prefix tools install' first.", file=sys.stderr)
        return stats_list, skipped_list
    
    # Find all JSON files in fixtures directory (sorted for determinism)
    json_files = sorted(fixtures_dir.glob("*.json"))
    
    if not json_files:
        return stats_list, skipped_list
    
    print(f"\n  TOONifying fixtures ({len(json_files)} JSON files found):")
    
    for json_path in json_files:
        try:
            with open(json_path, "r", encoding="utf-8") as f:
                data = json.load(f)
        except (json.JSONDecodeError, IOError) as e:
            reason = f"Invalid JSON: {e}"
            print(f"    Skipped {json_path.name}: {reason}")
            skipped_list.append({"file": json_path.name, "reason": reason})
            continue
        
        # Check eligibility
        if not is_toon_eligible(data, min_items):
            if not isinstance(data, list):
                reason = "Not an array"
            elif len(data) < min_items:
                reason = f"Only {len(data)} items (min: {min_items})"
            else:
                reason = "Non-uniform keys"
            print(f"    Skipped {json_path.name}: {reason}")
            skipped_list.append({"file": json_path.name, "reason": reason})
            continue
        
        # Convert to TOON
        toon_path = json_path.with_suffix(".toon")
        if run_toon_convert(json_path, toon_path):
            print(f"    Created {toon_path.name}")
            
            # Collect stats if requested
            if collect_stats:
                stats = run_toon_stats(json_path)
                if stats:
                    stats['file'] = json_path.name
                    stats['toon_file'] = toon_path.name
                    stats_list.append(stats)
        else:
            reason = "Conversion failed"
            print(f"    Skipped {json_path.name}: {reason}")
            skipped_list.append({"file": json_path.name, "reason": reason})
    
    return stats_list, skipped_list


def main():
    parser = argparse.ArgumentParser(
        description="Generate a Context Pack for delta-only LLM prompting.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python tools/contextpack.py
  python tools/contextpack.py --out /tmp/contextpack
  python tools/contextpack.py --logs build.log --lines 100
  python tools/contextpack.py --diff "git diff --cached"
  python tools/contextpack.py --no-toonify  # Skip TOON conversion
  python tools/contextpack.py --lint        # Validate config only

Features:
  - Denylist/allowlist via .contextpackignore
  - Deterministic output (stable ordering)
  - Size budgets (100KB soft, 200KB hard) with auto-split
  - Cache-friendly prompt.md generation

TOONify workflow:
  1. Add JSON fixtures to fixtures/ directory
  2. Run contextpack.py (auto-converts eligible JSON to TOON)
  3. Check token_report.md for savings stats

See docs/contextpack/README.md for the full Context Pack pipeline.
        """,
    )
    
    parser.add_argument(
        "--out",
        type=Path,
        default=REPO_ROOT / "contextpack",
        help="Output directory (default: ./contextpack)",
    )
    
    parser.add_argument(
        "--diff",
        type=str,
        default="git diff",
        help='Git diff command (default: "git diff")',
    )
    
    parser.add_argument(
        "--logs",
        type=Path,
        default=None,
        help="Path to log file to trim",
    )
    
    parser.add_argument(
        "--lines",
        type=int,
        default=100,
        help="Number of log lines to include (default: 100)",
    )
    
    parser.add_argument(
        "--context",
        action="store_true",
        help="Print path to stable context file (docs/LLM_CONTEXT.md)",
    )
    
    parser.add_argument(
        "--lint",
        action="store_true",
        help="Lint mode: validate config and templates without generating",
    )
    
    parser.add_argument(
        "--no-prompt",
        action="store_true",
        help="Skip generating prompt.md and cache_notes.md",
    )
    
    # TOON options
    toon_group = parser.add_argument_group("TOON conversion options")
    
    toon_group.add_argument(
        "--toonify",
        action="store_true",
        default=True,
        help="Enable TOON conversion for eligible JSON fixtures (default: on)",
    )
    
    toon_group.add_argument(
        "--no-toonify",
        action="store_true",
        help="Disable TOON conversion",
    )
    
    toon_group.add_argument(
        "--toon-stats",
        action="store_true",
        default=True,
        help="Generate token_report.md with savings stats (default: on)",
    )
    
    toon_group.add_argument(
        "--no-toon-stats",
        action="store_true",
        help="Disable token stats report",
    )
    
    toon_group.add_argument(
        "--toon-min-items",
        type=int,
        default=TOON_MIN_ITEMS_DEFAULT,
        help=f"Minimum array items for TOON conversion (default: {TOON_MIN_ITEMS_DEFAULT})",
    )
    
    args = parser.parse_args()
    
    # Lint mode
    if args.lint:
        success = run_lint()
        sys.exit(0 if success else 1)
    
    # Context mode
    if args.context:
        if LLM_CONTEXT.exists():
            print(LLM_CONTEXT)
        else:
            print(f"Context file not found: {LLM_CONTEXT}", file=sys.stderr)
            sys.exit(1)
        return
    
    # Handle --no-* flags
    toonify = args.toonify and not args.no_toonify
    toon_stats = args.toon_stats and not args.no_toon_stats
    generate_prompt = not args.no_prompt
    
    summary = generate_context_pack(
        out_dir=args.out,
        diff_command=args.diff,
        log_path=args.logs,
        log_lines=args.lines,
        toonify=toonify,
        toon_stats=toon_stats,
        toon_min_items=args.toon_min_items,
        generate_prompt=generate_prompt,
    )
    
    # Return summary as JSON for Claude-Flow integration
    # (can be captured by wrapper scripts)


if __name__ == "__main__":
    main()
