#!/usr/bin/env python3
"""
Convert Codex CLI session JSONL logs into claude-mem import payloads
and optionally ingest via claude-mem worker /api/import.
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import re
import time
import urllib.error
import urllib.request
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

UTC = timezone.utc
KEYWORD_CONCEPTS = {
    "react",
    "typescript",
    "javascript",
    "python",
    "rust",
    "go",
    "api",
    "database",
    "sql",
    "graphql",
    "rest",
    "authentication",
    "authorization",
    "security",
    "testing",
    "docker",
    "kubernetes",
    "aws",
    "gcp",
    "azure",
    "git",
    "ci/cd",
    "deployment",
    "performance",
    "optimization",
    "caching",
    "validation",
    "logging",
}
FILE_PATTERNS = [
    re.compile(r"(?:^|\s)([\/~][\w\-\.\/]+\.[\w\d]{1,10})(?:\s|$|:|\)|,)"),
    re.compile(r"(?:^|\s)(\.{1,2}\/[^\s]+\.[\w\d]{1,10})(?:\s|$|:|\)|,)"),
    re.compile(r"(?:^|\s)([\w\-]+\.[a-z]{2,6})(?:\s|$|:|\)|,)"),
    re.compile(r"`([^`]+\.[a-z]{2,6})`"),
]


def parse_iso(ts: Optional[str]) -> Optional[int]:
    if not ts or not isinstance(ts, str):
        return None
    s = ts.strip()
    if not s:
        return None
    try:
        if s.endswith("Z"):
            dt = datetime.fromisoformat(s[:-1]).replace(tzinfo=UTC)
        else:
            dt = datetime.fromisoformat(s)
            if dt.tzinfo is None:
                dt = dt.replace(tzinfo=UTC)
        return int(dt.timestamp() * 1000)
    except Exception:
        return None


def epoch_to_iso(ms: int) -> str:
    return datetime.fromtimestamp(ms / 1000.0, tz=UTC).isoformat().replace("+00:00", "Z")


def infer_type(content: str, user_content: Optional[str]) -> str:
    content_lower = content.lower()
    user_lower = (user_content or "").lower()
    combined = content_lower + " " + user_lower
    if any(w in combined for w in ["fix", "bug", "error", "issue", "regression"]):
        return "bugfix"
    if any(w in combined for w in ["implement", "add ", "create ", "new feature"]):
        return "feature"
    if any(w in combined for w in ["refactor", "clean up", "reorganize", "restructure"]):
        return "refactor"
    if any(w in content_lower for w in ["found", "discovered", "learned", "investigated", "analysis"]):
        return "discovery"
    if any(w in content_lower for w in ["decided", "chose", "will use", "recommend", "should use"]):
        return "decision"
    return "change"


def extract_title(content: str) -> Optional[str]:
    for line in content.splitlines():
        t = line.strip()
        if not t or t.startswith("#") or t.startswith("```"):
            continue
        if len(t) < 5:
            continue
        return t[:100] if len(t) > 100 else t
    cleaned = " ".join(content.split())
    if not cleaned:
        return None
    return cleaned[:100] if len(cleaned) > 100 else cleaned


def extract_concepts(content: str) -> List[str]:
    lower = content.lower()
    found = {kw for kw in KEYWORD_CONCEPTS if kw in lower}
    if "function" in lower or "def " in lower:
        found.add("functions")
    if "class " in lower or "interface " in lower:
        found.add("oop")
    if "async" in lower or "await" in lower:
        found.add("async")
    return sorted(found)[:10]


def extract_files(content: str) -> List[str]:
    files: List[str] = []
    for pattern in FILE_PATTERNS:
        for match in pattern.finditer(content):
            candidate = match.group(1)
            if not candidate or "http" in candidate:
                continue
            files.append(candidate)
    dedup: List[str] = []
    for f in files:
        if f not in dedup:
            dedup.append(f)
    return dedup[:20]


def get_message_text(content: object) -> str:
    if not isinstance(content, list):
        return ""
    pieces: List[str] = []
    for item in content:
        if not isinstance(item, dict):
            continue
        t = item.get("type")
        if t in ("input_text", "output_text"):
            txt = item.get("text")
            if isinstance(txt, str) and txt.strip():
                pieces.append(txt)
        else:
            txt = item.get("text")
            if isinstance(txt, str) and txt.strip():
                pieces.append(txt)
    return "\n".join(pieces).strip()


def parse_codex_sessions(session_glob: str) -> Dict[str, Dict[str, object]]:
    sessions: Dict[str, Dict[str, object]] = {}
    files = sorted(glob.glob(os.path.expanduser(session_glob), recursive=True))

    for path in files:
        session_id: Optional[str] = None
        session_cwd: Optional[str] = None
        session_start_ms: Optional[int] = None

        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            for idx, line in enumerate(fh):
                line = line.strip()
                if not line:
                    continue
                try:
                    obj = json.loads(line)
                except Exception:
                    continue

                ts_ms = parse_iso(obj.get("timestamp"))
                otype = obj.get("type")
                payload = obj.get("payload") if isinstance(obj.get("payload"), dict) else {}

                if otype == "session_meta":
                    sid = payload.get("id")
                    if isinstance(sid, str) and sid:
                        session_id = sid
                    cwd = payload.get("cwd")
                    if isinstance(cwd, str) and cwd:
                        session_cwd = cwd
                    if ts_ms is not None and session_start_ms is None:
                        session_start_ms = ts_ms
                    continue

                if otype != "response_item":
                    continue
                if payload.get("type") != "message":
                    continue

                role = payload.get("role")
                if role not in ("user", "assistant"):
                    continue

                text = get_message_text(payload.get("content"))
                if not text:
                    continue

                if session_id is None:
                    session_id = f"file:{Path(path).stem}"

                sess = sessions.setdefault(
                    session_id,
                    {
                        "session_id": session_id,
                        "source_file": path,
                        "cwd": session_cwd,
                        "started_at_epoch": session_start_ms,
                        "messages": [],
                    },
                )
                if session_cwd and not sess.get("cwd"):
                    sess["cwd"] = session_cwd
                if session_start_ms and not sess.get("started_at_epoch"):
                    sess["started_at_epoch"] = session_start_ms

                sess["messages"].append(
                    {
                        "role": role,
                        "text": text,
                        "created_at_epoch": ts_ms,
                        "order": idx,
                    }
                )

    # Normalize timestamps/order for all sessions.
    now_ms = int(time.time() * 1000)
    for sess in sessions.values():
        msgs = sess.get("messages") or []
        last = sess.get("started_at_epoch") if isinstance(sess.get("started_at_epoch"), int) else None
        for i, msg in enumerate(msgs):
            ts = msg.get("created_at_epoch")
            if not isinstance(ts, int):
                if isinstance(last, int):
                    ts = last + 1000
                else:
                    ts = now_ms + i * 1000
                msg["created_at_epoch"] = ts
            last = ts
        msgs.sort(key=lambda m: (m["created_at_epoch"], m["order"]))
        if msgs:
            sess["started_at_epoch"] = msgs[0]["created_at_epoch"]
            sess["completed_at_epoch"] = msgs[-1]["created_at_epoch"]
        else:
            sess["started_at_epoch"] = sess.get("started_at_epoch") or now_ms
            sess["completed_at_epoch"] = sess["started_at_epoch"]

    return sessions


def normalize_to_import_payload(
    sessions: Dict[str, Dict[str, object]],
    project: str,
) -> Tuple[List[Dict[str, object]], Dict[str, List[Dict[str, object]]], Dict[str, List[Dict[str, object]]]]:
    out_sessions: List[Dict[str, object]] = []
    observations_by_session: Dict[str, List[Dict[str, object]]] = defaultdict(list)
    prompts_by_session: Dict[str, List[Dict[str, object]]] = defaultdict(list)

    for sid, sess in sorted(sessions.items()):
        msgs = sess.get("messages") or []
        if not msgs:
            continue
        content_session_id = f"codex-{sid}"
        memory_session_id = content_session_id

        first_user = next((m for m in msgs if m.get("role") == "user" and m.get("text")), None)
        start_ms = int(sess.get("started_at_epoch") or msgs[0]["created_at_epoch"])
        end_ms = int(sess.get("completed_at_epoch") or msgs[-1]["created_at_epoch"])

        out_sessions.append(
            {
                "content_session_id": content_session_id,
                "memory_session_id": memory_session_id,
                "project": project,
                "user_prompt": (first_user.get("text", "")[:500] if first_user else None),
                "started_at": epoch_to_iso(start_ms),
                "started_at_epoch": start_ms,
                "completed_at": epoch_to_iso(end_ms),
                "completed_at_epoch": end_ms,
                "status": "completed",
            }
        )

        prompt_number = 0
        last_user_text: Optional[str] = None

        for m in msgs:
            role = m.get("role")
            text = (m.get("text") or "").strip()
            if not text:
                continue
            ts = int(m["created_at_epoch"])
            iso = epoch_to_iso(ts)

            if role == "user":
                prompt_number += 1
                last_user_text = text
                prompts_by_session[content_session_id].append(
                    {
                        "content_session_id": content_session_id,
                        "prompt_number": prompt_number,
                        "prompt_text": text,
                        "created_at": iso,
                        "created_at_epoch": ts,
                    }
                )
                continue

            if role != "assistant":
                continue

            concepts = extract_concepts(text)
            files = extract_files(text)
            obs = {
                "memory_session_id": memory_session_id,
                "project": project,
                "text": text[:2000],
                "type": infer_type(text, last_user_text),
                "title": extract_title(text),
                "subtitle": (last_user_text[:200] if last_user_text else None),
                "facts": None,
                "narrative": text,
                "concepts": json.dumps(concepts) if concepts else None,
                "files_read": json.dumps(files) if files else None,
                "files_modified": None,
                "prompt_number": prompt_number or None,
                "discovery_tokens": max(1, int(len(text) / 4)),
                "created_at": iso,
                "created_at_epoch": ts,
            }
            observations_by_session[content_session_id].append(obs)

    return out_sessions, observations_by_session, prompts_by_session


def chunked(items: Sequence[str], size: int) -> Iterable[List[str]]:
    for i in range(0, len(items), size):
        yield list(items[i : i + size])


def post_payload(api_url: str, payload: Dict[str, object]) -> Dict[str, object]:
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(api_url, data=data, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=120) as resp:
        return json.loads(resp.read())


def main(argv: Optional[Sequence[str]] = None) -> int:
    ap = argparse.ArgumentParser(description="Import Codex sessions into claude-mem via /api/import")
    ap.add_argument(
        "--input-glob",
        default="~/.codex/sessions/**/*.jsonl",
        help="Glob for Codex session JSONL files",
    )
    ap.add_argument("--project", default="codex", help="Project label for imported sessions")
    ap.add_argument("--chunk-size", type=int, default=25, help="Sessions per /api/import request")
    ap.add_argument("--api-url", default="http://127.0.0.1:37777/api/import", help="claude-mem import endpoint")
    ap.add_argument("--write-json", default="", help="Optional combined payload output path")
    ap.add_argument("--post", action="store_true", help="POST to /api/import")
    args = ap.parse_args(list(argv) if argv is not None else None)

    sessions = parse_codex_sessions(args.input_glob)
    norm_sessions, obs_by_sid, prompts_by_sid = normalize_to_import_payload(sessions, args.project)
    session_ids = [s["content_session_id"] for s in norm_sessions]

    total_obs = sum(len(obs_by_sid[sid]) for sid in session_ids)
    total_prompts = sum(len(prompts_by_sid[sid]) for sid in session_ids)
    print(f"Prepared {len(session_ids)} sessions, {total_obs} observations, {total_prompts} prompts")

    if args.write_json:
        payload = {
            "sessions": norm_sessions,
            "summaries": [],
            "observations": [o for sid in session_ids for o in obs_by_sid[sid]],
            "prompts": [p for sid in session_ids for p in prompts_by_sid[sid]],
        }
        out = Path(os.path.expanduser(args.write_json))
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(payload, indent=2), encoding="utf-8")
        print(f"Wrote payload preview to {out}")

    if not args.post:
        return 0

    index = {s["content_session_id"]: s for s in norm_sessions}
    totals = {
        "sessionsImported": 0,
        "sessionsSkipped": 0,
        "observationsImported": 0,
        "observationsSkipped": 0,
        "promptsImported": 0,
        "promptsSkipped": 0,
    }

    for chunk in chunked(session_ids, args.chunk_size):
        payload = {
            "sessions": [index[sid] for sid in chunk],
            "summaries": [],
            "observations": [o for sid in chunk for o in obs_by_sid[sid]],
            "prompts": [p for sid in chunk for p in prompts_by_sid[sid]],
        }
        try:
            resp = post_payload(args.api_url, payload)
        except urllib.error.HTTPError as exc:
            print(f"HTTP error during import: {exc.read().decode()}")
            return 3
        except Exception as exc:
            print(f"Failed to import chunk ({len(chunk)} sessions): {exc}")
            return 3

        stats = resp.get("stats") or {}
        for k in totals:
            totals[k] += int(stats.get(k, 0))
        print(f"Imported chunk ({len(chunk)} sessions) -> {stats}")

    print("Import complete:", totals)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
