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

# Import anchor extractor (handles ImportError if module not found)
try:
    from anchor_extractor import extract_anchors, format_anchors_markdown
except ImportError:
    # Fallback if anchor_extractor not available
    extract_anchors = None
    format_anchors_markdown = None

# Import delta ledger (handles ImportError if module not found)
try:
    from delta_ledger import DeltaLedger
except ImportError:
    # Fallback if delta_ledger not available
    DeltaLedger = None

# Import fragment cache (handles ImportError if module not found)
try:
    from fragment_cache import FragmentCache
except ImportError:
    # Fallback if fragment_cache not available
    FragmentCache = None

# Import file priority manager (handles ImportError if module not found)
try:
    from file_priority import FilePriorityManager
except ImportError as e:
    # Fallback if file_priority not available
    FilePriorityManager = None
    print("Warning: file_priority module not available - token budget feature disabled", file=sys.stderr)
    print(f"  Import error: {e}", file=sys.stderr)

# Import lazy context loader (handles ImportError if module not found)
try:
    from lazy_context import LazyContextLoader
except ImportError as e:
    # Fallback if lazy_context not available
    LazyContextLoader = None
    print("Warning: lazy_context module not available - lazy loading feature disabled", file=sys.stderr)
    print(f"  Import error: {e}", file=sys.stderr)

# Import semantic compressor (handles ImportError if module not found)
try:
    from semantic_compress import SemanticCompressor
except ImportError as e:
    # Fallback if semantic_compress not available
    SemanticCompressor = None
    print("Warning: semantic_compress module not available - document compression feature disabled", file=sys.stderr)
    print(f"  Import error: {e}", file=sys.stderr)


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


def filter_diff_by_file_list(diff_content: str, include_files: List[str]) -> Tuple[str, List[str]]:
    """
    Filter diff content to include only files in the provided list.
    Returns (filtered_diff, list_of_excluded_files).
    """
    if not diff_content.strip():
        return diff_content, []
    
    include_set = set(include_files)
    excluded_files = []
    filtered_chunks = []
    current_chunk = []
    current_file = None
    
    for line in diff_content.split("\n"):
        # Detect new file in diff
        if line.startswith("diff --git"):
            # Save previous chunk if included
            if current_chunk and current_file:
                if current_file in include_set:
                    filtered_chunks.append("\n".join(current_chunk))
                else:
                    excluded_files.append(current_file)
            
            # Start new chunk
            current_chunk = [line]
            # Extract file path from "diff --git a/path b/path"
            match = re.search(r'diff --git a/(.+?) b/', line)
            current_file = match.group(1) if match else None
        else:
            current_chunk.append(line)
    
    # Handle last chunk
    if current_chunk and current_file:
        if current_file in include_set:
            filtered_chunks.append("\n".join(current_chunk))
        else:
            excluded_files.append(current_file)
    
    return "\n".join(filtered_chunks), sorted(set(excluded_files))


def sort_diff_chunks(diff_content: str, priority_ordered_files: Optional[List[str]] = None) -> str:
    """
    Sort diff chunks by file path or priority order.
    
    Args:
        diff_content: Diff content to sort
        priority_ordered_files: Optional list of file paths in priority order.
                                If provided, chunks are sorted by this order.
                                If None, chunks are sorted alphabetically.
    """
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
    
    # Sort by priority order or file path
    if priority_ordered_files:
        # Sort by priority order (CRITICAL -> HIGH -> MEDIUM -> LOW)
        priority_map = {path: idx for idx, path in enumerate(priority_ordered_files)}
        chunks.sort(key=lambda x: priority_map.get(x[0], 9999))  # Unmapped files go last
    else:
        # Sort alphabetically (default)
        chunks.sort(key=lambda x: x[0])
    
    return "\n".join(chunk[1] for chunk in chunks)


def split_diff_semantic(diff_content: str, chunk_size: int = DIFF_CHUNK_SIZE) -> List[Tuple[str, str]]:
    """
    Split diff into chunks using semantic boundaries (file + hunk clusters).
    Groups related hunks within files to preserve logical cohesion.
    Returns list of (filename, content) tuples.
    """
    if len(diff_content.encode("utf-8")) <= chunk_size:
        return [("diff.patch", diff_content)]
    
    # Parse diff into file-level structures with hunks
    file_structures = []
    current_file = None
    current_file_lines = []
    current_hunk = []
    in_hunk = False
    
    for line in diff_content.split("\n"):
        if line.startswith("diff --git"):
            # Save previous file if exists
            if current_file and current_file_lines:
                file_structures.append({
                    "path": current_file,
                    "content": "\n".join(current_file_lines),
                    "size": len("\n".join(current_file_lines).encode("utf-8"))
                })
            
            # Start new file
            match = re.search(r'diff --git a/(.+?) b/', line)
            current_file = match.group(1) if match else "unknown"
            current_file_lines = [line]
            current_hunk = []
            in_hunk = False
        elif line.startswith("@@") and not in_hunk:
            # Start of hunk
            in_hunk = True
            current_hunk = [line]
            current_file_lines.append(line)
        elif line.startswith("@@"):
            # New hunk within same file - semantic boundary
            current_hunk = [line]
            current_file_lines.append(line)
        else:
            if in_hunk:
                current_hunk.append(line)
            current_file_lines.append(line)
    
    # Save last file
    if current_file and current_file_lines:
        file_structures.append({
            "path": current_file,
            "content": "\n".join(current_file_lines),
            "size": len("\n".join(current_file_lines).encode("utf-8"))
        })
    
    # Group files by directory for better semantic cohesion
    # Files in same directory are more likely related
    file_groups = {}
    for file_struct in file_structures:
        path = file_struct["path"]
        dir_path = os.path.dirname(path) if os.path.dirname(path) else "root"
        if dir_path not in file_groups:
            file_groups[dir_path] = []
        file_groups[dir_path].append(file_struct)
    
    # Build chunks preserving semantic groups
    chunks = []
    current_chunk = []
    current_size = 0
    chunk_num = 1
    
    for dir_path in sorted(file_groups.keys()):  # Deterministic ordering
        dir_files = file_groups[dir_path]
        
        for file_struct in dir_files:
            file_bytes = file_struct["size"]
            
            # If this file alone exceeds chunk size, split it
            if file_bytes > chunk_size:
                # Save current chunk if it has content
                if current_chunk:
                    chunks.append((f"diff_part_{chunk_num:02d}.patch", "\n".join(current_chunk)))
                    chunk_num += 1
                    current_chunk = []
                    current_size = 0
                
                # Large file gets its own chunk
                chunks.append((f"diff_part_{chunk_num:02d}.patch", file_struct["content"]))
                chunk_num += 1
            elif current_size + file_bytes > chunk_size and current_chunk:
                # Current chunk is full, start new one
                chunks.append((f"diff_part_{chunk_num:02d}.patch", "\n".join(current_chunk)))
                chunk_num += 1
                current_chunk = [file_struct["content"]]
                current_size = file_bytes
            else:
                # Add to current chunk
                current_chunk.append(file_struct["content"])
                current_size += file_bytes
    
    # Save last chunk
    if current_chunk:
        chunks.append((f"diff_part_{chunk_num:02d}.patch", "\n".join(current_chunk)))
    
    return chunks


def split_diff(diff_content: str, chunk_size: int = DIFF_CHUNK_SIZE, strategy: str = "size") -> List[Tuple[str, str]]:
    """
    Split diff into chunks of approximately chunk_size bytes.
    
    Args:
        diff_content: Raw diff content
        chunk_size: Target chunk size in bytes
        strategy: Chunking strategy - "size" (file boundaries) or "semantic" (file + hunk clusters)
    
    Returns:
        List of (filename, content) tuples.
    """
    if strategy == "semantic":
        return split_diff_semantic(diff_content, chunk_size)
    
    # Original size-based strategy
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
    print("  [1/7] Checking template files...")
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
    print("  [2/7] Checking .contextpackignore...")
    if IGNORE_FILE.exists():
        patterns = load_ignore_patterns()
        print(f"    OK: {len(patterns)} patterns loaded")
    else:
        print(f"    INFO: Using {len(DEFAULT_EXCLUSIONS)} default exclusions")
    
    # Check 3: TOON CLI availability
    print("  [3/7] Checking TOON CLI...")
    if check_toon_cli_available():
        print("    OK: TOON CLI available")
    else:
        warnings.append("TOON CLI not available. Run 'npm --prefix tools install'")
    
    # Check 4: Check staged files for secrets
    print("  [4/7] Checking staged files for secrets...")
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
    print("  [5/7] Checking token savings guardrail...")
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
    
    # Check 6: Verify new token-saving outputs (if context pack exists)
    print("  [6/7] Checking token-saving outputs...")
    contextpack_dir = REPO_ROOT / "contextpack"
    
    if contextpack_dir.exists():
        # Check for summary ladder files
        summary_files = ["summary_1line.md", "summary_5line.md", "summary_1para.md"]
        existing_summaries = [f for f in summary_files if (contextpack_dir / f).exists()]
        
        if existing_summaries:
            print(f"    OK: Summary ladder files present ({len(existing_summaries)}/{len(summary_files)})")
        
        # Check for multi-part diff with index
        diff_parts = list(contextpack_dir.glob("diff_part_*.patch"))
        diff_index_path = contextpack_dir / "diff_index.md"
        
        if len(diff_parts) > 1:
            if diff_index_path.exists():
                print(f"    OK: Diff index present for {len(diff_parts)}-part diff")
            else:
                # Not an error - index only generated when hard limit exceeded
                print(f"    INFO: Multi-part diff ({len(diff_parts)} parts) - index generated when needed")
        
        # Check for minified prompt (heuristic: check blank line density)
        prompt_path = contextpack_dir / "prompt.md"
        if prompt_path.exists():
            try:
                content = prompt_path.read_text(encoding="utf-8")
                blank_line_ratio = content.count("\n\n\n") / max(content.count("\n"), 1)
                if blank_line_ratio < 0.01:  # Very few triple newlines suggests minification
                    print("    INFO: Prompt may be minified (low blank-line density)")
            except IOError:
                pass
    else:
        print("    INFO: Context pack directory not found (skipping output checks)")
    
    # Check 7: Validate semantic chunking (if multi-part diff exists)
    print("  [7/7] Checking chunking strategy...")
    if contextpack_dir.exists():
        diff_parts = list(contextpack_dir.glob("diff_part_*.patch"))
        if len(diff_parts) > 1:
            # Semantic chunking tends to group files by directory
            # This is a weak heuristic - actual validation requires diff content analysis
            print(f"    INFO: Multi-part diff detected ({len(diff_parts)} parts) - chunking strategy applied")
    else:
        print("    INFO: Context pack directory not found (skipping chunking check)")
    
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


def generate_prompt_md(out_dir: Path, packet_content: str, diff_size: int, fixtures_count: int, minify: bool = False, cache_mode: str = "openai") -> Tuple[Path, Dict]:
    """
    Generate a cache-friendly prompt.md with stable prefix first.
    
    Args:
        out_dir: Output directory
        packet_content: Content from packet.md
        diff_size: Size of diff in bytes
        fixtures_count: Number of fixture files
        minify: Whether to minify prompt content
        cache_mode: Cache mode ("openai" or "anthropic")
    
    Returns:
        Tuple of (prompt_path, stats_dict) with before/after sizes if minified.
    """
    prompt_path = out_dir / "prompt.md"
    
    # Provider-specific cache instructions
    if cache_mode == "openai":
        cache_hint = (
            "<!-- OpenAI Prompt Caching: "
            "Stable prefix (above) cached for ≥1024 tokens. "
            "Keep prefix unchanged for cache hits. -->"
        )
    elif cache_mode == "anthropic":
        cache_hint = (
            "<!-- Anthropic Prompt Caching: "
            "Use cache_control blocks in API for stable prefix. "
            "Mark cache boundaries explicitly. -->"
        )
    else:
        cache_hint = "<!-- CACHE-FRIENDLY STRUCTURE: Stable content first, volatile content last -->"
    
    lines = [
        "# Context Pack Prompt",
        "",
        cache_hint,
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
        "<!-- DO NOT EDIT ABOVE THIS LINE - CACHE BARRIER -->",
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
    
    content = "\n".join(lines)
    stats = {"before_size": len(content.encode("utf-8"))}
    
    # Apply minification if requested
    if minify:
        content = minify_prompt_content(content)
        stats["after_size"] = len(content.encode("utf-8"))
        stats["savings_percent"] = round((1 - stats["after_size"] / stats["before_size"]) * 100, 1) if stats["before_size"] > 0 else 0
    else:
        stats["after_size"] = stats["before_size"]
        stats["savings_percent"] = 0
    
    prompt_path.write_text(content, encoding="utf-8")
    return prompt_path, stats


def minify_prompt_content(content: str) -> str:
    """
    Minify prompt content by removing redundant whitespace and comment blocks.
    Only affects generated prompt artefacts, not source diffs.
    
    Args:
        content: Prompt content to minify
    
    Returns:
        Minified content
    """
    lines = content.split("\n")
    minified = []
    prev_empty = False
    
    for line in lines:
        # Strip trailing whitespace
        line = line.rstrip()
        
        # Skip HTML comment blocks (<!-- ... -->)
        if re.match(r'^\s*<!--.*?-->\s*$', line):
            continue
        if re.match(r'^\s*<!--', line):
            # Start of multi-line comment - skip until end
            continue
        if re.match(r'.*?-->\s*$', line):
            # End of multi-line comment
            continue
        
        # Normalise multiple blank lines to single blank
        if not line.strip():
            if not prev_empty:
                minified.append("")
                prev_empty = True
            continue
        
        minified.append(line)
        prev_empty = False
    
    # Remove trailing blank lines
    while minified and not minified[-1]:
        minified.pop()
    
    return "\n".join(minified)


def generate_summary_ladder(
    out_dir: Path,
    packet_content: str,
    diff_size: int,
    diff_files: int,
    diff_parts: int,
    fixtures_count: int,
) -> List[Path]:
    """
    Generate hierarchical summarisation ladder (1-line, 5-line, 1-para).
    
    Returns list of generated summary file paths.
    """
    summary_paths = []
    
    # Extract key info from packet (if filled)
    goal_match = re.search(r'##\s+Goal\s*\n\n(.+?)(?=\n##|\Z)', packet_content, re.DOTALL)
    symptom_match = re.search(r'##\s+Symptom\s*\n\n(.+?)(?=\n##|\Z)', packet_content, re.DOTALL)
    
    goal = goal_match.group(1).strip() if goal_match else "[Goal not specified]"
    symptom = symptom_match.group(1).strip() if symptom_match else "[Symptom not specified]"
    
    # Truncate for summaries
    goal_short = goal[:80] + "..." if len(goal) > 80 else goal
    symptom_short = symptom[:80] + "..." if len(symptom) > 80 else symptom
    
    # 1-line summary
    summary_1line = f"Context pack: {diff_files} file(s), {diff_size/1024:.1f} KB, {fixtures_count} fixture(s). Goal: {goal_short}"
    path_1line = out_dir / "summary_1line.md"
    path_1line.write_text(summary_1line, encoding="utf-8")
    summary_paths.append(path_1line)
    
    # 5-line summary
    summary_5line = f"""# Context Pack Summary

**Goal**: {goal}

**Symptom**: {symptom}

**Changes**: {diff_files} file(s) modified, {diff_size/1024:.1f} KB diff ({diff_parts} part(s))

**Fixtures**: {fixtures_count} file(s)"""
    
    path_5line = out_dir / "summary_5line.md"
    path_5line.write_text(summary_5line, encoding="utf-8")
    summary_paths.append(path_5line)
    
    # 1-paragraph summary
    summary_1para = f"""Context pack for delta-only LLM prompting. **Goal**: {goal}. **Symptom**: {symptom}. Contains changes to {diff_files} file(s) ({diff_size/1024:.1f} KB total, split into {diff_parts} part(s) for size management) and {fixtures_count} fixture file(s). See `diff.patch` (or `diff_part_*.patch`) for changes, `fixtures/` for data fixtures, and `packet.md` for full goal/symptom/acceptance details."""
    
    path_1para = out_dir / "summary_1para.md"
    path_1para.write_text(summary_1para, encoding="utf-8")
    summary_paths.append(path_1para)
    
    return summary_paths


def generate_cache_fingerprint(
    llm_context_path: Path,
    prompt_path: Path,
    stable_prefix_tokens: int = 1024
) -> Dict:
    """
    Generate cache fingerprint from stable prefixes.
    
    Args:
        llm_context_path: Path to LLM_CONTEXT.md
        prompt_path: Path to generated prompt.md
        stable_prefix_tokens: Target token count for stable prefix (default: 1024)
    
    Returns:
        Dict with llm_context_hash, prompt_prefix_hash, and token_estimate
    """
    fingerprint = {
        "llm_context_hash": None,
        "prompt_prefix_hash": None,
        "stable_prefix_tokens": stable_prefix_tokens,
        "cache_barrier_position": None,
    }
    
    # Hash first N bytes of LLM_CONTEXT.md (default: entire file if <100KB, else first 50KB)
    if llm_context_path.exists():
        content = llm_context_path.read_bytes()
        hash_size = min(len(content), 50 * 1024)  # First 50KB max
        fingerprint["llm_context_hash"] = hashlib.sha256(content[:hash_size]).hexdigest()[:16]
    
    # Hash first ~1024 tokens of prompt.md stable prefix (~4096 chars = 1024 tokens)
    if prompt_path.exists():
        content = prompt_path.read_text(encoding="utf-8")
        # Find cache barrier position (if exists)
        barrier_match = re.search(r'<!--\s*DO NOT EDIT ABOVE THIS LINE\s*.*?-->', content)
        if barrier_match:
            fingerprint["cache_barrier_position"] = barrier_match.start()
            stable_content = content[:barrier_match.start()]
        else:
            # Estimate stable prefix (before "## 3. Volatile" section)
            volatile_match = re.search(r'^##\s+3\.\s+Volatile', content, re.MULTILINE)
            stable_content = content[:volatile_match.start()] if volatile_match else content[:4096]
        
        # Hash stable prefix (up to 4096 chars = ~1024 tokens)
        stable_prefix = stable_content[:4096]
        fingerprint["prompt_prefix_hash"] = hashlib.sha256(stable_prefix.encode("utf-8")).hexdigest()[:16]
        fingerprint["stable_prefix_tokens"] = len(stable_prefix.encode("utf-8")) // 4  # ~4 chars/token
    
    return fingerprint


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


def load_policy() -> Dict:
    """Load policy.yaml and return parsed dict."""
    policy_path = TOOLS_DIR / "command_centre" / "policy.yaml"
    if not policy_path.exists():
        return {}
    
    try:
        import yaml
        with open(policy_path, "r", encoding="utf-8") as f:
            return yaml.safe_load(f)
    except ImportError:
        print("Warning: PyYAML not available - policy features disabled", file=sys.stderr)
        return {}
    except Exception as e:
        print(f"Warning: Failed to load policy.yaml: {e}", file=sys.stderr)
        return {}


def classify_files_by_domain(file_paths: List[str], policy: Dict) -> Dict[str, List[str]]:
    """Classify file paths by domain based on policy patterns."""
    if not policy or "domains" not in policy:
        return {"unknown": file_paths}
    
    classifications = {domain: [] for domain in policy["domains"].keys()}
    classifications["unknown"] = []
    
    for file_path in file_paths:
        matched = False
        for domain_name, domain_config in policy["domains"].items():
            for pattern in domain_config.get("patterns", []):
                if fnmatch.fnmatch(file_path, pattern) or fnmatch.fnmatch(file_path, f"**/{pattern}"):
                    classifications[domain_name].append(file_path)
                    matched = True
                    break
            if matched:
                break
        
        if not matched:
            classifications["unknown"].append(file_path)
    
    return classifications


def check_protected_files(file_paths: List[str], policy: Dict) -> Tuple[List[str], Dict]:
    """
    Check if protected files are in the change set.
    
    Returns:
        Tuple of (protected_files_found, protection_details)
    """
    protected = []
    details = {}
    
    if not policy or "protected_files" not in policy:
        return protected, details
    
    for protected_file in policy["protected_files"]:
        pattern = protected_file["pattern"]
        for file_path in file_paths:
            if fnmatch.fnmatch(file_path, pattern) or fnmatch.fnmatch(file_path, f"**/{pattern}"):
                protected.append(file_path)
                details[file_path] = {
                    "level": protected_file["level"],
                    "checks": protected_file.get("checks", []),
                }
    
    return protected, details


def inject_domain_constraints(packet_path: Path, file_paths: List[str], policy: Dict) -> None:
    """Inject domain constraints block into packet.md based on file classifications."""
    if not packet_path.exists() or not file_paths:
        return
    
    classifications = classify_files_by_domain(file_paths, policy)
    active_domains = [d for d, files in classifications.items() if files and d != "unknown"]
    
    if not active_domains:
        return
    
    # Build constraints block
    constraints_block = ["\n## Domain Constraints", ""]
    
    for domain_name in active_domains:
        domain_config = policy.get("domains", {}).get(domain_name, {})
        constraints = domain_config.get("constraints", [])
        if constraints:
            constraints_block.append(f"### {domain_name.replace('_', ' ').title()}")
            for constraint in constraints:
                constraints_block.append(f"- {constraint}")
            constraints_block.append("")
    
    # Append to packet.md
    content = packet_path.read_text(encoding="utf-8")
    content += "\n" + "\n".join(constraints_block)
    packet_path.write_text(content, encoding="utf-8")


def extract_file_paths_from_diff(diff_content: str) -> List[str]:
    """
    Extract file paths from diff output.
    
    Args:
        diff_content: Git diff output
    
    Returns:
        List of file paths (relative to repo root)
    """
    file_paths = []
    for line in diff_content.split("\n"):
        if line.startswith("diff --git"):
            match = re.search(r'diff --git a/(.+?) b/', line)
            if match:
                file_paths.append(match.group(1))
    return file_paths


def generate_context_pack(
    out_dir: Path,
    diff_command: str = "git diff",
    log_path: Path = None,
    log_lines: int = 100,
    toonify: bool = True,
    toon_stats: bool = True,
    toon_min_items: int = TOON_MIN_ITEMS_DEFAULT,
    generate_prompt: bool = True,
    chunk_strategy: str = "size",
    minify_prompt: bool = False,
    generate_summaries: bool = False,
    extract_anchors_flag: bool = False,
    session_id: Optional[str] = None,
    enable_cache: bool = False,
    enable_lazy: bool = False,
    token_budget: Optional[int] = None,
    compress_docs: Optional[str] = None,
    cache_mode: str = "openai",
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
    
    # 1a. Extract anchors if enabled (before diff processing to get file list)
    diff_files_list = []  # Will be populated from diff
    
    # 2. Generate diff.patch with filtering and size budgets
    print("  Processing diff...")
    
    # Load exclusion patterns
    patterns = load_ignore_patterns()
    
    # Initialize delta ledger if session_id provided
    ledger = None
    if session_id and DeltaLedger is not None:
        ledger_path = REPO_ROOT / ".contextpack_ledger.json"
        try:
            ledger = DeltaLedger(ledger_path)
            print(f"  Using delta ledger (session: {session_id})")
        except Exception as e:
            print(f"    Warning: Failed to initialize ledger: {e}", file=sys.stderr)
    
    # Initialize fragment cache if enabled
    cache = None
    if enable_cache and FragmentCache is not None:
        cache_dir = REPO_ROOT / ".contextpack_cache"
        try:
            cache = FragmentCache(cache_dir)
            print(f"  Using fragment cache ({cache_dir.relative_to(REPO_ROOT)})")
        except Exception as e:
            print(f"    Warning: Failed to initialize cache: {e}", file=sys.stderr)
    elif enable_cache and FragmentCache is None:
        print("    Warning: fragment_cache module not available", file=sys.stderr)
    
    # Get raw diff
    diff_output = get_git_diff(diff_command)
    
    if diff_output.strip():
        # Extract file list from diff for anchor extraction
        for line in diff_output.split("\n"):
            if line.startswith("diff --git"):
                match = re.search(r'diff --git a/(.+?) b/', line)
                if match:
                    diff_files_list.append(match.group(1))
        
        # Apply lazy loading: prioritize file ordering (CRITICAL → HIGH → MEDIUM → LOW)
        if enable_lazy and LazyContextLoader is not None:
            try:
                lazy_loader = LazyContextLoader(REPO_ROOT)
                diff_files_list = lazy_loader.get_priority_ordered_files(diff_files_list)
                print(f"    Using lazy loading (priority-ordered {len(diff_files_list)} files)")
            except Exception as e:
                print(f"    Warning: Lazy loading failed: {e}", file=sys.stderr)
        
        # Apply token budget: enforce token limit by excluding low-priority files
        excluded_files_budget = []
        if token_budget and FilePriorityManager is not None:
            try:
                priority_manager = FilePriorityManager(REPO_ROOT)
                included_files, excluded_files_budget, total_tokens = priority_manager.enforce_budget(
                    diff_files_list, token_budget, include_all_critical=True
                )
                
                if excluded_files_budget:
                    # Update file list to only include files within budget
                    diff_files_list = included_files
                    print(f"    Token budget: {token_budget} tokens, included {len(included_files)} files ({total_tokens} tokens), excluded {len(excluded_files_budget)} files")
            except Exception as e:
                print(f"    Warning: Token budget enforcement failed: {e}", file=sys.stderr)
        
        # Filter excluded files (by patterns)
        filtered_diff, excluded_files = filter_diff_content(diff_output, patterns)
        
        # Apply token budget filtering: if files were excluded by budget, filter diff by included files
        if excluded_files_budget:
            # Re-filter diff to only include files within budget
            filtered_diff, _ = filter_diff_by_file_list(filtered_diff, diff_files_list)
        
        # Apply delta ledger filtering if enabled
        ledger_skipped = []
        if ledger and session_id:
            # Extract file hashes from filtered diff
            file_hashes = {}
            current_file = None
            current_file_content = []
            
            for line in filtered_diff.split("\n"):
                if line.startswith("diff --git"):
                    if current_file and current_file_content:
                        # Hash previous file's diff content
                        file_content = "\n".join(current_file_content)
                        file_hash = ledger.hash_content(file_content)
                        file_hashes[current_file] = file_hash
                    # Start new file
                    match = re.search(r'diff --git a/(.+?) b/', line)
                    current_file = match.group(1) if match else None
                    current_file_content = [line]
                else:
                    if current_file_content is not None:
                        current_file_content.append(line)
            
            # Hash last file
            if current_file and current_file_content:
                file_content = "\n".join(current_file_content)
                file_hash = ledger.hash_content(file_content)
                file_hashes[current_file] = file_hash
            
            # Get unchanged files
            unchanged_files = ledger.get_unchanged_files(file_hashes, session_id)
            
            if unchanged_files:
                # Filter out unchanged files from diff
                filtered_chunks = []
                current_chunk = []
                current_file = None
                
                for line in filtered_diff.split("\n"):
                    if line.startswith("diff --git"):
                        # Save previous chunk if not unchanged
                        if current_chunk and current_file and current_file not in unchanged_files:
                            filtered_chunks.append("\n".join(current_chunk))
                        elif current_file and current_file in unchanged_files:
                            ledger_skipped.append(current_file)
                        
                        # Start new chunk
                        match = re.search(r'diff --git a/(.+?) b/', line)
                        current_file = match.group(1) if match else None
                        current_chunk = [line] if current_file not in unchanged_files else []
                    else:
                        if current_file not in unchanged_files:
                            current_chunk.append(line)
                
                # Save last chunk
                if current_chunk and current_file and current_file not in unchanged_files:
                    filtered_chunks.append("\n".join(current_chunk))
                elif current_file and current_file in unchanged_files:
                    ledger_skipped.append(current_file)
                
                filtered_diff = "\n".join(filtered_chunks)
                
                if ledger_skipped:
                    print(f"    Skipped {len(ledger_skipped)} files (unchanged since session {session_id})")
        
        summary["excluded_files"] = len(excluded_files)
        
        if excluded_files:
            print(f"    Excluded {len(excluded_files)} files by .contextpackignore")
        
        # Sort for deterministic output
        # If lazy loading is enabled, sort by priority order; otherwise sort alphabetically
        priority_list = diff_files_list if enable_lazy else None
        filtered_diff = sort_diff_chunks(filtered_diff, priority_ordered_files=priority_list)
        
        diff_size = len(filtered_diff.encode("utf-8"))
        summary["diff_size"] = diff_size
        summary["diff_files"] = filtered_diff.count("diff --git")
        
        # Check size budgets
        if diff_size > DIFF_SOFT_LIMIT:
            strategy_name = "semantic" if chunk_strategy == "semantic" else "size-based"
            print(f"    Diff size ({diff_size / 1024:.1f} KB) exceeds soft limit ({DIFF_SOFT_LIMIT / 1024:.0f} KB), splitting ({strategy_name})...")
            
            chunks = split_diff(filtered_diff, DIFF_CHUNK_SIZE, strategy=chunk_strategy)
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
            diff_filename = "diff_delta.patch" if ledger and session_id else "diff.patch"
            diff_dest = out_dir / diff_filename
            diff_dest.write_text(filtered_diff, encoding="utf-8")
            print(f"    Created: {diff_dest.name} ({diff_size / 1024:.1f} KB, {summary['diff_files']} files)")
            
            # Record file hashes in ledger if enabled
            if ledger and session_id:
                diff_hash = ledger.hash_content(filtered_diff)
                ledger.record(diff_hash, session_id, "diff", diff_filename)
    else:
        diff_filename = "diff_delta.patch" if ledger and session_id else "diff.patch"
        diff_dest = out_dir / diff_filename
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
    
    # 4. Extract and inject semantic anchors if enabled
    if extract_anchors_flag and extract_anchors is not None:
        print("  Extracting semantic anchors...")
        try:
            # Use cache if enabled (cache key based on diff files list)
            cache_key = "anchors_markdown"
            source_paths = [
                REPO_ROOT / "firmware" / "v2" / "src" / "effects" / "PatternRegistry.cpp",
                REPO_ROOT / "firmware" / "v2" / "src" / "plugins" / "api" / "IEffect.h",
                REPO_ROOT / "firmware" / "v2" / "src" / "plugins" / "api" / "EffectContext.h",
            ]
            
            def generate_anchors():
                anchors = extract_anchors(REPO_ROOT, diff_files_list)
                return format_anchors_markdown(anchors)
            
            if cache:
                # Check if cache has entry before generating
                cached_content = cache.get(cache_key, source_paths)
                if cached_content:
                    anchors_markdown = cached_content
                    print(f"    Used cached anchors (cache key: {cache_key})")
                else:
                    anchors_markdown = cache.get_or_generate(
                        cache_key, generate_anchors, source_paths=source_paths, extension=".md"
                    )
                    print(f"    Generated and cached anchors (cache key: {cache_key})")
            else:
                anchors_markdown = generate_anchors()
            
            # Append anchors to packet.md
            if packet_dest.exists():
                packet_content = packet_dest.read_text(encoding="utf-8")
                packet_content = packet_content.rstrip() + "\n\n" + anchors_markdown + "\n"
                packet_dest.write_text(packet_content, encoding="utf-8")
                print(f"    Injected semantic anchors into {packet_dest.name}")
        except Exception as e:
            print(f"    Warning: Anchor extraction failed: {e}", file=sys.stderr)
    elif extract_anchors_flag and extract_anchors is None:
        print("    Warning: anchor_extractor module not available", file=sys.stderr)
    
    # 4b. Inject domain constraints if policy available
    policy = load_policy()
    if policy and diff_files_list:
        try:
            inject_domain_constraints(packet_dest, diff_files_list, policy)
        except Exception as e:
            print(f"    Warning: Domain constraints injection failed: {e}", file=sys.stderr)
    
    # 4a. Compress packet.md if compression enabled
    if compress_docs and compress_docs != "none" and SemanticCompressor is not None:
        if packet_dest.exists():
            try:
                compressor = SemanticCompressor()
                packet_content = packet_dest.read_text(encoding="utf-8")
                original_size = len(packet_content.encode("utf-8"))
                compressed_content = compressor.compress(packet_content, compress_docs)
                packet_dest.write_text(compressed_content, encoding="utf-8")
                compressed_size = len(compressed_content.encode("utf-8"))
                savings = ((original_size - compressed_size) / original_size * 100) if original_size > 0 else 0
                print(f"    Compressed packet.md ({compress_docs}): {savings:.1f}% savings")
            except Exception as e:
                print(f"    Warning: Compression failed for packet.md: {e}", file=sys.stderr)
    
    # 5. Copy fixtures README
    fixtures_readme_dest = fixtures_dir / "README.md"
    copy_template(FIXTURES_README, fixtures_readme_dest)
    print(f"  Created: fixtures/{fixtures_readme_dest.name}")
    
    # 6. TOONify JSON fixtures (if enabled)
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
    
    # 7. Generate token report (if stats collected)
    if toon_stats and (stats_list or skipped_list):
        report_path = generate_token_report(out_dir, stats_list, skipped_list)
        if report_path:
            print(f"  Created: {report_path.name}")
    
    # 8. Generate prompt.md (cache-friendly structure)
    cache_fingerprint = None
    prompt_path = None
    if generate_prompt:
        packet_content = ""
        if packet_dest.exists():
            packet_content = packet_dest.read_text(encoding="utf-8")
        
        fixtures_count = len(list(fixtures_dir.glob("*.toon"))) + len(list(fixtures_dir.glob("*.json")))
        prompt_path, prompt_stats = generate_prompt_md(
            out_dir, packet_content, summary["diff_size"], fixtures_count, minify=minify_prompt, cache_mode=cache_mode
        )
        
        # Generate cache fingerprint
        cache_fingerprint = generate_cache_fingerprint(LLM_CONTEXT, prompt_path)
        fingerprint_path = out_dir / "cache_fingerprint.json"
        fingerprint_path.write_text(json.dumps(cache_fingerprint, indent=2), encoding="utf-8")
        print(f"  Created: {fingerprint_path.name}")
        
        if minify_prompt and prompt_stats["savings_percent"] > 0:
            print(f"  Created: {prompt_path.name} (minified: {prompt_stats['savings_percent']}% savings)")
        else:
            print(f"  Created: {prompt_path.name}")
        
        # Compress prompt.md if compression enabled
        if compress_docs and compress_docs != "none" and SemanticCompressor is not None:
            try:
                compressor = SemanticCompressor()
                prompt_content = prompt_path.read_text(encoding="utf-8")
                original_size = len(prompt_content.encode("utf-8"))
                compressed_content = compressor.compress(prompt_content, compress_docs)
                prompt_path.write_text(compressed_content, encoding="utf-8")
                compressed_size = len(compressed_content.encode("utf-8"))
                savings = ((original_size - compressed_size) / original_size * 100) if original_size > 0 else 0
                print(f"    Compressed prompt.md ({compress_docs}): {savings:.1f}% savings")
            except Exception as e:
                print(f"    Warning: Compression failed for prompt.md: {e}", file=sys.stderr)
        
        # Generate cache notes
        cache_notes_path = generate_cache_notes(out_dir)
        print(f"  Created: {cache_notes_path.name}")
    
    # 9. Generate hierarchical summaries (if enabled)
    if generate_summaries:
        packet_content = ""
        if packet_dest.exists():
            packet_content = packet_dest.read_text(encoding="utf-8")
        
        fixtures_count = len(list(fixtures_dir.glob("*.toon"))) + len(list(fixtures_dir.glob("*.json")))
        summary_paths = generate_summary_ladder(
            out_dir,
            packet_content,
            summary["diff_size"],
            summary["diff_files"],
            summary["diff_parts"],
            fixtures_count,
        )
        
        for path in summary_paths:
            print(f"  Created: {path.name}")
        
        # Compress summary files if compression enabled
        if compress_docs and compress_docs != "none" and SemanticCompressor is not None:
            try:
                compressor = SemanticCompressor()
                for summary_path in summary_paths:
                    summary_content = summary_path.read_text(encoding="utf-8")
                    compressed_content = compressor.compress(summary_content, compress_docs)
                    summary_path.write_text(compressed_content, encoding="utf-8")
            except Exception as e:
                print(f"    Warning: Compression failed for summaries: {e}", file=sys.stderr)
    
    # 10. Enrich summary with additional stats
    summary["timestamp"] = datetime.now().isoformat()
    
    # Add cache fingerprint if prompt was generated
    if cache_fingerprint:
        summary["cache_fingerprint"] = cache_fingerprint
        if prompt_path and prompt_path.exists():
            prompt_size = prompt_path.stat().st_size
            summary["estimated_prompt_tokens"] = prompt_size // 4  # ~4 chars/token
        else:
            summary["estimated_prompt_tokens"] = 0
    else:
        summary["cache_fingerprint"] = None
        summary["estimated_prompt_tokens"] = 0
    
    # Add domain classifications if policy available
    policy = load_policy()
    if policy and diff_files_list:
        classifications = classify_files_by_domain(diff_files_list, policy)
        protected_files, protection_details = check_protected_files(diff_files_list, policy)
        summary["domain_classifications"] = {k: len(v) for k, v in classifications.items() if v}
        summary["protected_files_detected"] = list(protection_details.keys())
    else:
        summary["domain_classifications"] = None
        summary["protected_files_detected"] = []
    
    # Add compression savings if enabled
    if compress_docs and compress_docs != "none":
        summary["compression_enabled"] = compress_docs
    else:
        summary["compression_enabled"] = None
    
    # Write pack_stats.json
    stats_path = out_dir / "pack_stats.json"
    stats_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(f"  Created: {stats_path.name}")
    
    # Also store in reports directory for trend tracking
    if "reports/contextpack" not in str(out_dir):
        date_str = datetime.now().strftime("%Y-%m-%d")
        reports_dir = REPO_ROOT / "reports" / "contextpack" / date_str
        reports_dir.mkdir(parents=True, exist_ok=True)
        time_str = datetime.now().strftime("%H%M%S")
        reports_stats_path = reports_dir / f"pack_stats_{time_str}.json"
        reports_stats_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")
    
    # Print summary
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
    
    parser.add_argument(
        "--chunk-strategy",
        type=str,
        choices=["size", "semantic"],
        default="size",
        help="Chunking strategy: 'size' (file boundaries) or 'semantic' (file + hunk clusters) (default: size)",
    )
    
    parser.add_argument(
        "--minify-prompt",
        action="store_true",
        help="Minify prompt.md by removing redundant whitespace and comment blocks",
    )
    
    parser.add_argument(
        "--summaries",
        action="store_true",
        help="Generate hierarchical summaries (1-line, 5-line, 1-paragraph)",
    )
    
    parser.add_argument(
        "--anchors",
        action="store_true",
        help="Extract and inject semantic anchors (effect IDs, interfaces, changed modules) into packet.md",
    )
    
    parser.add_argument(
        "--session",
        type=str,
        default=None,
        help="Session ID for delta ledger tracking (skips unchanged files from previous sessions)",
    )
    
    parser.add_argument(
        "--cache",
        action="store_true",
        help="Enable fragment caching (caches effect registry, metadata, anchors for reuse)",
    )
    
    parser.add_argument(
        "--lazy",
        action="store_true",
        help="Enable lazy context loading (loads files only when needed, priority-based ordering)",
    )
    
    parser.add_argument(
        "--token-budget",
        type=int,
        default=None,
        help="Token budget limit (truncates low-priority files when exceeded)",
    )
    
    parser.add_argument(
        "--compress-docs",
        type=str,
        choices=["none", "light", "aggressive"],
        default=None,
        help="Compression level for documentation (none, light, aggressive). Default: none",
    )
    
    parser.add_argument(
        "--cache-mode",
        type=str,
        choices=["openai", "anthropic"],
        default="openai",
        help="Cache mode for prompt generation (openai or anthropic). Default: openai",
    )
    
    parser.add_argument(
        "--policy-check",
        action="store_true",
        help="Check policy: classify diff files by domain and verify protected files",
    )
    
    parser.add_argument(
        "--ack-protected",
        action="store_true",
        help="Acknowledge protected file changes (required when protected files are touched)",
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
    
    # Policy check mode
    if args.policy_check:
        diff_output = get_git_diff(args.diff)
        file_paths = extract_file_paths_from_diff(diff_output)
        
        policy = load_policy()
        if not policy:
            print("Warning: policy.yaml not found or failed to load", file=sys.stderr)
        
        classifications = classify_files_by_domain(file_paths, policy)
        protected_files, protection_details = check_protected_files(file_paths, policy)
        
        # Print domain classifications
        print("\nDomain Classifications:")
        for domain, files in classifications.items():
            if files:
                print(f"  {domain}: {len(files)} files")
        
        # Check protected files
        if protected_files and not args.ack_protected:
            print(f"\nERROR: Protected files detected without --ack-protected flag:")
            for pf in protected_files:
                print(f"  {pf} ({protection_details[pf]['level']})")
            sys.exit(1)
        elif protected_files:
            print(f"\nProtected files detected (acknowledged):")
            for pf in protected_files:
                print(f"  {pf} ({protection_details[pf]['level']})")
        
        # Don't generate pack, just check (unless --out is also specified)
        if args.policy_check and not args.out:
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
        chunk_strategy=args.chunk_strategy,
        minify_prompt=args.minify_prompt,
        generate_summaries=args.summaries,
        extract_anchors_flag=args.anchors,
        session_id=args.session,
        enable_cache=args.cache,
        enable_lazy=args.lazy,
        token_budget=args.token_budget,
        compress_docs=args.compress_docs,
        cache_mode=args.cache_mode,
    )
    
    # Return summary as JSON for Claude-Flow integration
    # (can be captured by wrapper scripts)


if __name__ == "__main__":
    main()
