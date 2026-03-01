#!/usr/bin/env python3
"""
Enrich harmonixset manifests with YouTube metadata via yt-dlp.

Outputs:
- harmonixset_youtube_cache.json
- harmonixset_preferred_enriched.csv
- harmonixset_test_candidates_enriched.csv
- harmonixset_enriched_summary.json
"""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import json
import subprocess
from collections import Counter
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from typing import Any


DEFAULT_OUT_DIR = Path(
    "/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/"
    "firmware-v3/test/music_corpus/harmonixset"
)
DEFAULT_PREFERRED = DEFAULT_OUT_DIR / "harmonixset_preferred.csv"
DEFAULT_CANDIDATES = DEFAULT_OUT_DIR / "harmonixset_test_candidates.csv"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Enrich harmonixset with YouTube metadata")
    parser.add_argument("--preferred-csv", type=Path, default=DEFAULT_PREFERRED)
    parser.add_argument("--candidates-csv", type=Path, default=DEFAULT_CANDIDATES)
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT_DIR)
    parser.add_argument("--workers", type=int, default=6)
    parser.add_argument("--timeout-seconds", type=float, default=25.0)
    parser.add_argument("--max-items", type=int, default=0, help="0 means no limit")
    parser.add_argument("--refresh", action="store_true", help="Ignore cache and re-fetch all")
    parser.add_argument("--yt-dlp-bin", type=str, default="yt-dlp")
    return parser.parse_args()


def now_utc() -> str:
    return dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def read_csv_rows(path: Path) -> list[dict[str, str]]:
    with path.open("r", newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def load_cache(path: Path) -> dict[str, dict[str, Any]]:
    if not path.exists():
        return {}
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
        if isinstance(payload, dict):
            return payload
    except json.JSONDecodeError:
        pass
    return {}


def save_cache(path: Path, cache: dict[str, dict[str, Any]]) -> None:
    ordered = {k: cache[k] for k in sorted(cache.keys())}
    path.write_text(json.dumps(ordered, indent=2, ensure_ascii=False), encoding="utf-8")


def normalise_date_yyyymmdd(value: str | None) -> str:
    if not value or len(value) != 8 or not value.isdigit():
        return ""
    return f"{value[0:4]}-{value[4:6]}-{value[6:8]}"


def encode_json_list(value: Any) -> str:
    if isinstance(value, list):
        return json.dumps(value, ensure_ascii=False, separators=(",", ":"))
    return "[]"


def extract_metadata(payload: dict[str, Any], fetched_at: str) -> dict[str, Any]:
    tags = payload.get("tags")
    categories = payload.get("categories")
    return {
        "yt_fetch_ok": True,
        "yt_fetch_error": "",
        "yt_title": str(payload.get("title") or ""),
        "yt_channel": str(payload.get("channel") or ""),
        "yt_channel_id": str(payload.get("channel_id") or ""),
        "yt_uploader": str(payload.get("uploader") or ""),
        "yt_uploader_id": str(payload.get("uploader_id") or ""),
        "yt_webpage_url": str(payload.get("webpage_url") or ""),
        "yt_duration_s": int(payload.get("duration") or 0),
        "yt_upload_date": normalise_date_yyyymmdd(payload.get("upload_date")),
        "yt_release_date": normalise_date_yyyymmdd(payload.get("release_date")),
        "yt_view_count": int(payload.get("view_count") or 0),
        "yt_like_count": int(payload.get("like_count") or 0),
        "yt_availability": str(payload.get("availability") or ""),
        "yt_live_status": str(payload.get("live_status") or ""),
        "yt_age_limit": int(payload.get("age_limit") or 0),
        "yt_was_live": bool(payload.get("was_live") or False),
        "yt_categories_json": encode_json_list(categories),
        "yt_tags_json": encode_json_list(tags),
        "yt_tags_count": len(tags) if isinstance(tags, list) else 0,
        "yt_fetched_at_utc": fetched_at,
    }


def fetch_one(source_id: str, yt_dlp_bin: str, timeout_seconds: float) -> tuple[str, dict[str, Any]]:
    url = f"https://www.youtube.com/watch?v={source_id}"
    cmd = [
        yt_dlp_bin,
        "--no-warnings",
        "--skip-download",
        "--no-playlist",
        "--dump-single-json",
        url,
    ]
    fetched_at = now_utc()
    try:
        proc = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout_seconds,
            check=False,
        )
    except subprocess.TimeoutExpired:
        return source_id, {
            "yt_fetch_ok": False,
            "yt_fetch_error": f"timeout>{timeout_seconds:.1f}s",
            "yt_title": "",
            "yt_channel": "",
            "yt_channel_id": "",
            "yt_uploader": "",
            "yt_uploader_id": "",
            "yt_webpage_url": url,
            "yt_duration_s": 0,
            "yt_upload_date": "",
            "yt_release_date": "",
            "yt_view_count": 0,
            "yt_like_count": 0,
            "yt_availability": "",
            "yt_live_status": "",
            "yt_age_limit": 0,
            "yt_was_live": False,
            "yt_categories_json": "[]",
            "yt_tags_json": "[]",
            "yt_tags_count": 0,
            "yt_fetched_at_utc": fetched_at,
        }

    if proc.returncode != 0 or not proc.stdout.strip():
        msg = (proc.stderr or "").strip().replace("\n", " ")
        msg = msg[:300] if msg else f"yt-dlp exit={proc.returncode}"
        return source_id, {
            "yt_fetch_ok": False,
            "yt_fetch_error": msg,
            "yt_title": "",
            "yt_channel": "",
            "yt_channel_id": "",
            "yt_uploader": "",
            "yt_uploader_id": "",
            "yt_webpage_url": url,
            "yt_duration_s": 0,
            "yt_upload_date": "",
            "yt_release_date": "",
            "yt_view_count": 0,
            "yt_like_count": 0,
            "yt_availability": "",
            "yt_live_status": "",
            "yt_age_limit": 0,
            "yt_was_live": False,
            "yt_categories_json": "[]",
            "yt_tags_json": "[]",
            "yt_tags_count": 0,
            "yt_fetched_at_utc": fetched_at,
        }

    try:
        payload = json.loads(proc.stdout)
    except json.JSONDecodeError:
        return source_id, {
            "yt_fetch_ok": False,
            "yt_fetch_error": "invalid_json",
            "yt_title": "",
            "yt_channel": "",
            "yt_channel_id": "",
            "yt_uploader": "",
            "yt_uploader_id": "",
            "yt_webpage_url": url,
            "yt_duration_s": 0,
            "yt_upload_date": "",
            "yt_release_date": "",
            "yt_view_count": 0,
            "yt_like_count": 0,
            "yt_availability": "",
            "yt_live_status": "",
            "yt_age_limit": 0,
            "yt_was_live": False,
            "yt_categories_json": "[]",
            "yt_tags_json": "[]",
            "yt_tags_count": 0,
            "yt_fetched_at_utc": fetched_at,
        }

    return source_id, extract_metadata(payload, fetched_at)


def merge_rows(rows: list[dict[str, str]], cache: dict[str, dict[str, Any]]) -> list[dict[str, Any]]:
    out: list[dict[str, Any]] = []
    for row in rows:
        merged: dict[str, Any] = dict(row)
        sid = row.get("source_id", "")
        meta = cache.get(sid, {})
        if "source_url" not in merged or not merged["source_url"]:
            merged["source_url"] = f"https://www.youtube.com/watch?v={sid}"
        for k, v in meta.items():
            merged[k] = v
        out.append(merged)
    return out


def parse_json_list(value: Any) -> list[str]:
    if isinstance(value, list):
        return [str(x) for x in value]
    if isinstance(value, str) and value:
        try:
            arr = json.loads(value)
            if isinstance(arr, list):
                return [str(x) for x in arr]
        except json.JSONDecodeError:
            return []
    return []


def write_csv(path: Path, rows: list[dict[str, Any]]) -> None:
    fields = sorted({k for row in rows for k in row.keys()})
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    args = parse_args()
    out_dir = args.out_dir.resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    preferred_rows = read_csv_rows(args.preferred_csv.resolve())
    candidates_rows = read_csv_rows(args.candidates_csv.resolve())
    cache_path = out_dir / "harmonixset_youtube_cache.json"
    cache = load_cache(cache_path)

    source_ids = sorted(
        {
            row.get("source_id", "").strip()
            for row in preferred_rows
            if row.get("source_id", "").strip()
        }
    )

    if args.max_items > 0:
        source_ids = source_ids[: args.max_items]

    if args.refresh:
        to_fetch = source_ids
    else:
        to_fetch = [sid for sid in source_ids if sid not in cache]
    cache_hits = len(source_ids) - len(to_fetch)

    print(
        "[enrich] source_ids="
        f"{len(source_ids)} cache_hits={cache_hits} fetch_pending={len(to_fetch)} workers={args.workers}"
    )

    fetched = 0
    ok_fetched = 0
    with ThreadPoolExecutor(max_workers=max(1, args.workers)) as ex:
        futures = [
            ex.submit(fetch_one, sid, args.yt_dlp_bin, args.timeout_seconds)
            for sid in to_fetch
        ]
        for fut in as_completed(futures):
            sid, meta = fut.result()
            cache[sid] = meta
            fetched += 1
            if meta.get("yt_fetch_ok"):
                ok_fetched += 1
            if fetched % 20 == 0 or fetched == len(to_fetch):
                print(f"[enrich] fetched {fetched}/{len(to_fetch)} ok={ok_fetched}")

    save_cache(cache_path, cache)

    preferred_enriched = merge_rows(preferred_rows, cache)
    candidates_enriched = merge_rows(candidates_rows, cache)

    preferred_out = out_dir / "harmonixset_preferred_enriched.csv"
    candidates_out = out_dir / "harmonixset_test_candidates_enriched.csv"
    write_csv(preferred_out, preferred_enriched)
    write_csv(candidates_out, candidates_enriched)

    ok_count = sum(1 for sid in source_ids if cache.get(sid, {}).get("yt_fetch_ok"))
    error_count = len(source_ids) - ok_count

    channel_counts: Counter[str] = Counter()
    category_counts: Counter[str] = Counter()
    upload_year_counts: Counter[str] = Counter()
    unavailable_ids: list[str] = []

    for sid in source_ids:
        meta = cache.get(sid, {})
        if not meta.get("yt_fetch_ok"):
            unavailable_ids.append(sid)
            continue
        channel = str(meta.get("yt_channel") or "").strip()
        if channel:
            channel_counts[channel] += 1

        for cat in parse_json_list(meta.get("yt_categories_json")):
            if cat.strip():
                category_counts[cat.strip()] += 1

        upload_date = str(meta.get("yt_upload_date") or "")
        if len(upload_date) >= 4 and upload_date[:4].isdigit():
            upload_year_counts[upload_date[:4]] += 1

    summary = {
        "generated_at_utc": now_utc(),
        "preferred_csv": str(args.preferred_csv.resolve()),
        "candidates_csv": str(args.candidates_csv.resolve()),
        "out_dir": str(out_dir),
        "total_source_ids": len(source_ids),
        "cache_hits": cache_hits,
        "fetched_this_run": len(to_fetch),
        "ok_count": ok_count,
        "error_count": error_count,
        "top_channels": channel_counts.most_common(20),
        "top_categories": category_counts.most_common(20),
        "upload_year_counts": dict(sorted(upload_year_counts.items())),
        "unavailable_source_ids": unavailable_ids,
        "outputs": {
            "cache_json": cache_path.name,
            "preferred_enriched_csv": preferred_out.name,
            "candidates_enriched_csv": candidates_out.name,
        },
    }
    summary_path = out_dir / "harmonixset_enriched_summary.json"
    summary_path.write_text(json.dumps(summary, indent=2, ensure_ascii=False), encoding="utf-8")

    print(f"[enrich] wrote: {cache_path}")
    print(f"[enrich] wrote: {preferred_out}")
    print(f"[enrich] wrote: {candidates_out}")
    print(f"[enrich] wrote: {summary_path}")
    print(f"[enrich] result: ok={ok_count} error={error_count}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
