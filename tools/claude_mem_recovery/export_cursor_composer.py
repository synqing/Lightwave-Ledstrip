#!/usr/bin/env python3
"""
Export Cursor "Composer" conversations from Cursor's globalStorage SQLite DB.

Cursor stores a large amount of AI chat/composer state in:
  ~/Library/Application Support/Cursor/User/globalStorage/state.vscdb

The data of interest is typically in the `cursorDiskKV` table under keys like:
  - composerData:<composerId>
  - bubbleId:<composerId>:<bubbleId>

This script exports a *normalised message stream* as JSONL:
  { source, conversation_id, message_id, role, created_at, text, rich_text, metadata }

It intentionally avoids any dependency on Cursor internals beyond the observed key/value formats.
"""

from __future__ import annotations

import argparse
import json
import os
import sqlite3
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, Iterable, Iterator, List, Optional, Sequence, Tuple


DEFAULT_DB = os.path.expanduser(
    "~/Library/Application Support/Cursor/User/globalStorage/state.vscdb"
)


def _as_text(value: Any) -> str:
    if value is None:
        return ""
    if isinstance(value, str):
        return value
    if isinstance(value, (bytes, bytearray)):
        return value.decode("utf-8", errors="replace")
    return str(value)


def _epoch_ms_to_iso(epoch_ms: Any) -> Optional[str]:
    try:
        n = int(epoch_ms)
    except Exception:
        return None
    dt = datetime.fromtimestamp(n / 1000.0, tz=timezone.utc)
    return dt.isoformat().replace("+00:00", "Z")


def _extract_bubble_text(bubble: Dict[str, Any]) -> Tuple[str, Optional[str]]:
    """
    Cursor bubble records can store content in multiple fields.
    Prefer `text`, but keep `richText` if present for potential future parsing.
    """
    text = bubble.get("text")
    if isinstance(text, str) and text.strip():
        return text, bubble.get("richText") if isinstance(bubble.get("richText"), str) else None

    rich = bubble.get("richText")
    if isinstance(rich, str) and rich.strip():
        # richText often contains JSON (lexical editor state). Keep it as-is.
        return "", rich

    # Fallbacks: keep empty text rather than guessing incorrectly.
    return "", None


def _bubble_role_from_type(bubble_type: Any) -> str:
    # Observed: 1=user, 2=assistant (but treat as heuristic).
    if bubble_type == 1:
        return "user"
    if bubble_type == 2:
        return "assistant"
    return "unknown"


def _iter_composer_rows(
    conn: sqlite3.Connection, limit: Optional[int] = None
) -> Iterator[Tuple[str, str]]:
    cur = conn.cursor()
    sql = "SELECT key, value FROM cursorDiskKV WHERE key LIKE 'composerData:%' ORDER BY key"
    if limit is not None:
        sql += f" LIMIT {int(limit)}"
    for key, value in cur.execute(sql):
        yield key, _as_text(value)


def _get_bubble_ids_for_composer(
    conn: sqlite3.Connection, composer_id: str, composer_data: Dict[str, Any]
) -> List[str]:
    headers = composer_data.get("fullConversationHeadersOnly")
    bubble_ids: List[str] = []
    if isinstance(headers, list):
        for h in headers:
            if isinstance(h, dict):
                bid = h.get("bubbleId")
                if isinstance(bid, str) and bid:
                    bubble_ids.append(bid)

    if bubble_ids:
        return bubble_ids

    # Fallback: query all bubble keys for this composer.
    cur = conn.cursor()
    prefix = f"bubbleId:{composer_id}:"
    sql = "SELECT key FROM cursorDiskKV WHERE key LIKE ? ORDER BY key"
    for (key,) in cur.execute(sql, (prefix + "%",)):
        if isinstance(key, str) and key.startswith(prefix):
            bubble_ids.append(key[len(prefix) :])
    return bubble_ids


def _load_bubble(
    conn: sqlite3.Connection, composer_id: str, bubble_id: str
) -> Optional[Dict[str, Any]]:
    cur = conn.cursor()
    key = f"bubbleId:{composer_id}:{bubble_id}"
    row = cur.execute("SELECT value FROM cursorDiskKV WHERE key = ?", (key,)).fetchone()
    if not row:
        return None
    raw = _as_text(row[0])
    try:
        return json.loads(raw)
    except Exception:
        return {"_parse_error": True, "raw": raw, "bubbleId": bubble_id}


def export_cursor(
    db_path: str,
    output_path: str,
    limit_composers: Optional[int],
    include_composer_ids: Optional[Sequence[str]],
    emit_meta: bool,
) -> Dict[str, int]:
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row

    out = Path(output_path)
    out.parent.mkdir(parents=True, exist_ok=True)

    counts = {"composers": 0, "bubbles": 0, "bubbles_with_text": 0}

    with out.open("w", encoding="utf-8") as f:
        for key, raw in _iter_composer_rows(conn, limit=limit_composers):
            try:
                composer_data = json.loads(raw)
            except Exception:
                continue

            composer_id = composer_data.get("composerId")
            if not isinstance(composer_id, str) or not composer_id:
                continue

            if include_composer_ids and composer_id not in set(include_composer_ids):
                continue

            counts["composers"] += 1

            created_at_iso = _epoch_ms_to_iso(composer_data.get("createdAt"))
            if emit_meta:
                meta_record = {
                    "record_type": "cursor_composer_meta",
                    "source": "cursor",
                    "conversation_id": composer_id,
                    "created_at": created_at_iso,
                    "cursor_key": key,
                    "version": composer_data.get("_v"),
                    "status": composer_data.get("status"),
                    "has_unread_messages": composer_data.get("hasUnreadMessages"),
                }
                f.write(json.dumps(meta_record, ensure_ascii=False) + "\n")

            bubble_ids = _get_bubble_ids_for_composer(conn, composer_id, composer_data)
            for bubble_id in bubble_ids:
                bubble = _load_bubble(conn, composer_id, bubble_id)
                if not isinstance(bubble, dict):
                    continue

                role = _bubble_role_from_type(bubble.get("type"))
                text, rich = _extract_bubble_text(bubble)
                created_at = bubble.get("createdAt")

                record = {
                    "record_type": "cursor_bubble",
                    "source": "cursor",
                    "conversation_id": composer_id,
                    "message_id": bubble_id,
                    "role": role,
                    "created_at": created_at if isinstance(created_at, str) else None,
                    "text": text,
                    "rich_text": rich,
                    "metadata": {
                        "bubble_type": bubble.get("type"),
                        "token_count": bubble.get("tokenCount"),
                        "unified_mode": bubble.get("unifiedMode"),
                        "is_agentic": bubble.get("isAgentic"),
                    },
                }

                counts["bubbles"] += 1
                if text.strip():
                    counts["bubbles_with_text"] += 1
                f.write(json.dumps(record, ensure_ascii=False) + "\n")

    return counts


def main(argv: Optional[Sequence[str]] = None) -> int:
    ap = argparse.ArgumentParser(description="Export Cursor composer chats to JSONL.")
    ap.add_argument("--db", default=DEFAULT_DB, help=f"Path to Cursor state DB (default: {DEFAULT_DB})")
    ap.add_argument("--output", required=True, help="Output JSONL file path.")
    ap.add_argument("--limit-composers", type=int, default=None, help="Limit number of composer conversations scanned.")
    ap.add_argument(
        "--composer-id",
        action="append",
        default=None,
        help="Export only these composerId values (repeatable).",
    )
    ap.add_argument(
        "--emit-meta",
        action="store_true",
        help="Emit a cursor_composer_meta record per composerId.",
    )

    args = ap.parse_args(list(argv) if argv is not None else None)

    db_path = os.path.expanduser(args.db)
    if not Path(db_path).exists():
        print(f"DB not found: {db_path}", file=sys.stderr)
        return 2

    counts = export_cursor(
        db_path=db_path,
        output_path=args.output,
        limit_composers=args.limit_composers,
        include_composer_ids=args.composer_id,
        emit_meta=args.emit_meta,
    )

    print(
        f"Exported composers={counts['composers']} bubbles={counts['bubbles']} bubbles_with_text={counts['bubbles_with_text']} -> {args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

