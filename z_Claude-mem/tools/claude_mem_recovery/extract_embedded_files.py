#!/usr/bin/env python3
"""
Extract embedded file contents from Claude Code JSONL chat logs.

Why this exists:
- Some historical sessions include tool "outcome" payloads that embed full file contents (e.g. create/update events).
- Those embedded payloads can be used to reconstruct missing scripts/modules that no longer exist on disk.

This script scans JSONL logs (typically ~/.claude/projects/**/*.jsonl), finds <outcome>...</outcome> blocks,
decodes the embedded JSON payloads, and writes recovered files to a chosen output directory.

Notes:
- Designed to be dependency-free (standard library only).
- Conservative by default: only extracts create/update-style file writes unless --include is widened.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, Iterable, Iterator, List, Optional, Sequence, Tuple


@dataclass(frozen=True)
class SourceRef:
    jsonl_path: str
    jsonl_line: int
    outcome_index: int


@dataclass
class RecoveredFile:
    original_path: str
    content: str
    kind: str
    sources: List[SourceRef]

    def sha256(self) -> str:
        return hashlib.sha256(self.content.encode("utf-8")).hexdigest()


OUTCOME_BLOCK_RE = re.compile(r"<outcome>(.*?)</outcome>", re.DOTALL)


def _iter_jsonl_files(inputs: Sequence[str]) -> Iterator[Path]:
    for raw in inputs:
        p = Path(os.path.expanduser(raw))
        if p.is_file() and p.suffix == ".jsonl":
            yield p
            continue
        if p.is_dir():
            yield from p.rglob("*.jsonl")


def _safe_json_loads(value: str) -> Optional[Any]:
    try:
        return json.loads(value)
    except Exception:
        return None


def _decode_embedded_json(maybe_json: str, max_depth: int = 3) -> Optional[Any]:
    """
    The embedded payload is often JSON-encoded multiple times (e.g. a JSON string containing JSON).
    Try a few decode layers until it stops changing.
    """
    current: Any = maybe_json
    for _ in range(max_depth):
        if not isinstance(current, str):
            return current
        decoded = _safe_json_loads(current)
        if decoded is None:
            return None
        current = decoded
    return current


def _extract_text_blobs(event: Any) -> Iterator[str]:
    if not isinstance(event, dict):
        return

    msg = event.get("message")
    if isinstance(msg, dict):
        content = msg.get("content")
        if isinstance(content, list):
            for part in content:
                if isinstance(part, dict) and part.get("type") == "text":
                    text = part.get("text")
                    if isinstance(text, str) and text:
                        yield text

    # Fallback: some events may have a direct "text" field
    text = event.get("text")
    if isinstance(text, str) and text:
        yield text


def _extract_recovered_files_from_outcome_payload(
    payload: Any, source: SourceRef
) -> List[RecoveredFile]:
    """
    Supported shapes seen in logs:
    - { "type": "create"|"update", "filePath": "...", "content": "..." }
    - { "type": "text", "file": { "filePath": "...", "content": "..." } }  (often from file-view tools)
    """
    recovered: List[RecoveredFile] = []

    if not isinstance(payload, dict):
        return recovered

    payload_type = payload.get("type")
    if isinstance(payload_type, str) and payload_type in {"create", "update", "edit"}:
        file_path = payload.get("filePath")
        content = payload.get("content")
        if isinstance(file_path, str) and isinstance(content, str):
            recovered.append(
                RecoveredFile(
                    original_path=file_path,
                    content=content,
                    kind=payload_type,
                    sources=[source],
                )
            )

    if isinstance(payload_type, str) and payload_type == "text":
        file_obj = payload.get("file")
        if isinstance(file_obj, dict):
            file_path = file_obj.get("filePath")
            content = file_obj.get("content")
            if isinstance(file_path, str) and isinstance(content, str):
                recovered.append(
                    RecoveredFile(
                        original_path=file_path,
                        content=content,
                        kind="file",
                        sources=[source],
                    )
                )

    return recovered


def _extract_recovered_files_from_tool_use_result(
    tool_use_result: Any, source: SourceRef
) -> List[RecoveredFile]:
    """
    Claude Code JSONL often includes a structured tool result payload.

    Common shapes:
    - { filePath, originalFile, oldString, newString } (string replacement edit)
    - { filePath, content } (create/update)
    """
    if not isinstance(tool_use_result, dict):
        return []

    file_path = tool_use_result.get("filePath")
    if not isinstance(file_path, str) or not file_path:
        return []

    # Prefer explicit final content if present.
    content = tool_use_result.get("content")
    if isinstance(content, str):
        return [
            RecoveredFile(
                original_path=file_path,
                content=content,
                kind="toolUseResult",
                sources=[source],
            )
        ]

    # Some edits include the entire original file + the exact replacement strings.
    original_file = tool_use_result.get("originalFile")
    if isinstance(original_file, str):
        old = tool_use_result.get("oldString")
        new = tool_use_result.get("newString")
        if isinstance(old, str) and isinstance(new, str) and old:
            updated = original_file.replace(old, new, 1)
            content = updated if updated != original_file else original_file
        else:
            content = original_file

        return [
            RecoveredFile(
                original_path=file_path,
                content=content,
                kind="toolUseResult",
                sources=[source],
            )
        ]

    return []


def _merge_recovered_files(files: List[RecoveredFile]) -> List[RecoveredFile]:
    """
    Merge duplicate extractions of the same (path, content) by aggregating sources.
    Keep distinct contents as separate entries (caller handles conflict naming on write).
    """
    merged: Dict[Tuple[str, str], RecoveredFile] = {}
    for f in files:
        key = (f.original_path, f.sha256())
        if key not in merged:
            merged[key] = RecoveredFile(
                original_path=f.original_path,
                content=f.content,
                kind=f.kind,
                sources=list(f.sources),
            )
        else:
            merged[key].sources.extend(f.sources)
    return list(merged.values())


def _compile_include_regexps(patterns: Sequence[str]) -> List[re.Pattern[str]]:
    compiled: List[re.Pattern[str]] = []
    for p in patterns:
        compiled.append(re.compile(p))
    return compiled


def _included(original_path: str, include_res: List[re.Pattern[str]]) -> bool:
    if not include_res:
        return True
    return any(r.search(original_path) for r in include_res)


def _derive_output_relpath(original_path: str) -> Path:
    """
    Aim for a portable, repo-like layout rather than absolute host paths.

    Examples:
    - .../.claude/plugins/marketplaces/thedotmack/src/services/import/Importer.ts
      -> thedotmack/src/services/import/Importer.ts
    - (fallback) /abs/path.txt -> abs/abs/path.txt
    """
    normalised = original_path.replace("\\", "/")

    marker = "/thedotmack/"
    idx = normalised.find(marker)
    if idx != -1:
        suffix = normalised[idx + len(marker) :]
        return Path("thedotmack") / suffix

    marker = "/claude-mem/"
    idx = normalised.find(marker)
    if idx != -1:
        suffix = normalised[idx + len(marker) :]
        return Path("claude-mem") / suffix

    # Fallback: keep a stable representation under abs/
    stripped = normalised.lstrip("/")
    return Path("abs") / stripped


def _ensure_parent_dir(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)


def _write_with_conflict_handling(dest: Path, content: str) -> Tuple[Path, str]:
    """
    Returns (written_path, status) where status is: created|identical|conflict
    """
    if dest.exists():
        existing = dest.read_text(encoding="utf-8", errors="replace")
        if existing == content:
            return dest, "identical"

        # Conflict: choose a new name that cannot overwrite.
        stem = dest.name
        sha8 = hashlib.sha256(content.encode("utf-8")).hexdigest()[:8]
        conflict = dest.with_name(f"{stem}.conflict-{sha8}")
        _ensure_parent_dir(conflict)
        conflict.write_text(content, encoding="utf-8")
        return conflict, "conflict"

    _ensure_parent_dir(dest)
    dest.write_text(content, encoding="utf-8")
    return dest, "created"


def _scan_jsonl(
    jsonl_paths: Iterable[Path],
    include_res: List[re.Pattern[str]],
    max_files: Optional[int],
    max_lines: Optional[int],
) -> List[RecoveredFile]:
    recovered: List[RecoveredFile] = []

    for fi, jsonl_path in enumerate(jsonl_paths, 1):
        if max_files is not None and fi > max_files:
            break

        try:
            with jsonl_path.open("r", encoding="utf-8") as f:
                for line_no, line in enumerate(f, 1):
                    if max_lines is not None and line_no > max_lines:
                        break

                    line = line.strip()
                    if not line:
                        continue

                    event = _safe_json_loads(line)
                    if event is None:
                        continue

                    # Direct tool results (not wrapped in <outcome> tags)
                    if isinstance(event, dict) and "toolUseResult" in event:
                        tr = event.get("toolUseResult")
                        src = SourceRef(str(jsonl_path), line_no, 0)
                        for rf in _extract_recovered_files_from_tool_use_result(tr, src):
                            if _included(rf.original_path, include_res):
                                recovered.append(rf)

                    for blob in _extract_text_blobs(event):
                        for oi, match in enumerate(OUTCOME_BLOCK_RE.finditer(blob), 1):
                            inner = match.group(1).strip()
                            payload = _decode_embedded_json(inner)
                            if payload is None:
                                continue
                            src = SourceRef(str(jsonl_path), line_no, oi)
                            for rf in _extract_recovered_files_from_outcome_payload(payload, src):
                                if _included(rf.original_path, include_res):
                                    recovered.append(rf)
        except (OSError, UnicodeDecodeError):
            continue

    return _merge_recovered_files(recovered)


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description="Recover embedded file contents from Claude Code JSONL logs."
    )
    parser.add_argument(
        "--input",
        action="append",
        required=True,
        help="Input JSONL file or directory to scan (repeatable). Example: ~/.claude/projects",
    )
    parser.add_argument(
        "--output",
        required=True,
        help="Output directory to write recovered files into.",
    )
    parser.add_argument(
        "--include",
        action="append",
        default=[],
        help=(
            "Regex filter applied to original file paths. "
            "Repeat to add more. If omitted, extracts everything it can decode."
        ),
    )
    parser.add_argument(
        "--max-files",
        type=int,
        default=None,
        help="Stop after scanning N JSONL files (useful for quick tests).",
    )
    parser.add_argument(
        "--max-lines",
        type=int,
        default=None,
        help="Stop after scanning N lines per JSONL file (useful for quick tests).",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Do not write files; only print what would be recovered.",
    )

    args = parser.parse_args(list(argv) if argv is not None else None)

    include_res = _compile_include_regexps(args.include)
    output_dir = Path(args.output)

    jsonl_paths = sorted(_iter_jsonl_files(args.input))
    recovered = _scan_jsonl(
        jsonl_paths=jsonl_paths,
        include_res=include_res,
        max_files=args.max_files,
        max_lines=args.max_lines,
    )

    if not recovered:
        print("No recoverable files found (with current filters).", file=sys.stderr)
        return 2

    manifest: Dict[str, Any] = {
        "inputs": [str(Path(os.path.expanduser(p))) for p in args.input],
        "output": str(output_dir),
        "include": args.include,
        "files": [],
    }

    created = identical = conflicts = 0
    for rf in sorted(recovered, key=lambda x: x.original_path):
        rel = _derive_output_relpath(rf.original_path)
        dest = output_dir / rel

        status = "dry-run"
        written = dest
        if not args.dry_run:
            written, status = _write_with_conflict_handling(dest, rf.content)
            if status == "created":
                created += 1
            elif status == "identical":
                identical += 1
            elif status == "conflict":
                conflicts += 1

        manifest["files"].append(
            {
                "original_path": rf.original_path,
                "output_path": str(written),
                "kind": rf.kind,
                "sha256": rf.sha256(),
                "bytes": len(rf.content.encode("utf-8")),
                "sources": [
                    {
                        "jsonl_path": s.jsonl_path,
                        "jsonl_line": s.jsonl_line,
                        "outcome_index": s.outcome_index,
                    }
                    for s in rf.sources
                ],
            }
        )

        print(f"{rf.kind:8} {status:9} {rf.original_path} -> {written}")

    if not args.dry_run:
        _ensure_parent_dir(output_dir / "manifest.json")
        (output_dir / "manifest.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True), encoding="utf-8"
        )

    print(
        f"\nRecovered: {len(recovered)} file(s) | created={created} identical={identical} conflicts={conflicts}"
    )
    if not args.dry_run:
        print(f"Manifest: {output_dir / 'manifest.json'}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
