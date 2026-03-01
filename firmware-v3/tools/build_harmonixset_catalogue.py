#!/usr/bin/env python3
"""
Build a sorted metadata catalogue for the harmonixset audio corpus.

Outputs:
- harmonixset_catalogue.csv
- harmonixset_catalogue.json
- harmonixset_preferred.csv
- harmonixset_summary.json
"""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import json
import subprocess
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any


DEFAULT_INPUT = Path(
    "/Users/spectrasynq/Workspace_Management/Software/K1.reinvented/"
    "Implementation.plans/harmonixset-main/audio"
)
DEFAULT_OUTPUT = Path(
    "/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/"
    "firmware-v3/test/music_corpus/harmonixset"
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build harmonixset audio catalogue")
    parser.add_argument("--input", type=Path, default=DEFAULT_INPUT, help="Input audio directory")
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUTPUT, help="Output directory")
    return parser.parse_args()


def safe_float(v: Any) -> float:
    try:
        return float(v)
    except (TypeError, ValueError):
        return 0.0


def safe_int(v: Any) -> int:
    try:
        return int(v)
    except (TypeError, ValueError):
        return 0


def duration_bucket(seconds: float) -> str:
    if seconds < 15.0:
        return "too_short"
    if seconds < 60.0:
        return "micro_loop"
    if seconds < 180.0:
        return "short"
    if seconds < 420.0:
        return "medium"
    return "long"


def parse_source_id(file_name: str) -> tuple[str, str]:
    lower = file_name.lower()
    if lower.endswith(".mp3.wav"):
        return file_name[:-8], "wav_from_mp3"
    if lower.endswith(".mp3"):
        return file_name[:-4], "mp3"
    if lower.endswith(".wav"):
        return file_name[:-4], "wav"
    return file_name, "other"


def ffprobe_audio(path: Path) -> dict[str, Any]:
    cmd = [
        "ffprobe",
        "-v",
        "error",
        "-select_streams",
        "a:0",
        "-show_entries",
        "stream=codec_name,sample_rate,channels,bit_rate",
        "-show_entries",
        "format=duration,size,bit_rate",
        "-of",
        "json",
        str(path),
    ]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        payload = json.loads(result.stdout or "{}")
    except (subprocess.CalledProcessError, json.JSONDecodeError):
        payload = {}

    streams = payload.get("streams") or [{}]
    stream = streams[0] if streams else {}
    fmt = payload.get("format") or {}

    return {
        "codec": stream.get("codec_name", ""),
        "sample_rate_hz": safe_int(stream.get("sample_rate")),
        "channels": safe_int(stream.get("channels")),
        "stream_bit_rate_bps": safe_int(stream.get("bit_rate")),
        "duration_s": round(safe_float(fmt.get("duration")), 3),
        "size_bytes": safe_int(fmt.get("size")),
        "format_bit_rate_bps": safe_int(fmt.get("bit_rate")),
    }


def preferred_rank(row: dict[str, Any]) -> tuple[int, int, int, int]:
    variant = row["variant"]
    duration = row["duration_s"]
    sample_rate = row["sample_rate_hz"]
    fmt_br = row["format_bit_rate_bps"]

    is_wav = 0 if variant in {"wav", "wav_from_mp3"} else 1
    is_test_length = 0 if 30.0 <= duration <= 360.0 else 1
    # Negative for descending sort on quality fields.
    return (is_wav, is_test_length, -sample_rate, -fmt_br)


def read_downloaded_ids(audio_dir: Path) -> set[str]:
    downloaded = audio_dir / ".downloaded.txt"
    if not downloaded.exists():
        return set()

    ids: set[str] = set()
    for line in downloaded.read_text(encoding="utf-8", errors="ignore").splitlines():
        parts = line.strip().split()
        if len(parts) == 2 and parts[0] == "youtube":
            ids.add(parts[1])
    return ids


def main() -> int:
    args = parse_args()
    input_dir = args.input.resolve()
    out_dir = args.out_dir.resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    if not input_dir.exists():
        raise FileNotFoundError(f"Input directory not found: {input_dir}")

    audio_files = sorted(
        [
            p
            for p in input_dir.iterdir()
            if p.is_file()
            and p.suffix.lower() in {".mp3", ".wav"}
            and not p.name.lower().endswith(".part")
        ]
    )

    downloaded_ids = read_downloaded_ids(input_dir)

    rows: list[dict[str, Any]] = []
    for idx, path in enumerate(audio_files, start=1):
        if idx % 50 == 0:
            print(f"[catalogue] probed {idx}/{len(audio_files)} files")

        source_id, variant = parse_source_id(path.name)
        meta = ffprobe_audio(path)

        row: dict[str, Any] = {
            "source_id": source_id,
            "source_url": f"https://www.youtube.com/watch?v={source_id}",
            "file_name": path.name,
            "abs_path": str(path),
            "ext": path.suffix.lower().lstrip("."),
            "variant": variant,
            "duration_s": meta["duration_s"],
            "duration_bucket": duration_bucket(meta["duration_s"]),
            "sample_rate_hz": meta["sample_rate_hz"],
            "channels": meta["channels"],
            "codec": meta["codec"],
            "stream_bit_rate_bps": meta["stream_bit_rate_bps"],
            "format_bit_rate_bps": meta["format_bit_rate_bps"],
            "size_bytes": meta["size_bytes"],
            "in_download_list": source_id in downloaded_ids,
        }
        rows.append(row)

    mp3_by_id: defaultdict[str, int] = defaultdict(int)
    wav_by_id: defaultdict[str, int] = defaultdict(int)
    for r in rows:
        if r["ext"] == "mp3":
            mp3_by_id[r["source_id"]] += 1
        if r["ext"] == "wav":
            wav_by_id[r["source_id"]] += 1

    for r in rows:
        sid = r["source_id"]
        r["has_mp3_pair"] = mp3_by_id[sid] > 0
        r["has_wav_pair"] = wav_by_id[sid] > 0

    rows_sorted = sorted(rows, key=lambda r: (r["source_id"], r["variant"], r["file_name"]))

    preferred_by_id: dict[str, dict[str, Any]] = {}
    by_source: defaultdict[str, list[dict[str, Any]]] = defaultdict(list)
    for r in rows_sorted:
        by_source[r["source_id"]].append(r)
    for sid, variants in by_source.items():
        preferred_by_id[sid] = sorted(variants, key=preferred_rank)[0]

    preferred_rows = sorted(preferred_by_id.values(), key=lambda r: (r["duration_s"], r["source_id"]))
    candidate_rows = [r for r in preferred_rows if 30.0 <= r["duration_s"] <= 360.0]

    ext_counts = Counter(r["ext"] for r in rows_sorted)
    bucket_counts = Counter(r["duration_bucket"] for r in rows_sorted)
    sample_rate_counts = Counter(r["sample_rate_hz"] for r in rows_sorted)

    summary = {
        "generated_at_utc": dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z"),
        "input_dir": str(input_dir),
        "total_files": len(rows_sorted),
        "unique_source_ids": len(by_source),
        "ext_counts": dict(sorted(ext_counts.items())),
        "duration_bucket_counts": dict(sorted(bucket_counts.items())),
        "sample_rate_counts": dict(sorted(sample_rate_counts.items())),
        "pairs": {
            "source_ids_with_mp3": sum(1 for k in by_source if mp3_by_id[k] > 0),
            "source_ids_with_wav": sum(1 for k in by_source if wav_by_id[k] > 0),
            "source_ids_with_both": sum(1 for k in by_source if mp3_by_id[k] > 0 and wav_by_id[k] > 0),
        },
        "preferred_rows": len(preferred_rows),
        "test_candidate_rows": len(candidate_rows),
    }

    csv_fields = [
        "source_id",
        "source_url",
        "file_name",
        "abs_path",
        "ext",
        "variant",
        "duration_s",
        "duration_bucket",
        "sample_rate_hz",
        "channels",
        "codec",
        "stream_bit_rate_bps",
        "format_bit_rate_bps",
        "size_bytes",
        "has_mp3_pair",
        "has_wav_pair",
        "in_download_list",
    ]

    catalogue_csv = out_dir / "harmonixset_catalogue.csv"
    with catalogue_csv.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=csv_fields)
        writer.writeheader()
        writer.writerows(rows_sorted)

    preferred_csv = out_dir / "harmonixset_preferred.csv"
    with preferred_csv.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=csv_fields)
        writer.writeheader()
        writer.writerows(preferred_rows)

    candidates_csv = out_dir / "harmonixset_test_candidates.csv"
    with candidates_csv.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=csv_fields)
        writer.writeheader()
        writer.writerows(candidate_rows)

    catalogue_json = out_dir / "harmonixset_catalogue.json"
    catalogue_json.write_text(json.dumps(rows_sorted, indent=2), encoding="utf-8")

    summary_json = out_dir / "harmonixset_summary.json"
    summary_json.write_text(json.dumps(summary, indent=2), encoding="utf-8")

    print(f"[catalogue] wrote: {catalogue_csv}")
    print(f"[catalogue] wrote: {preferred_csv}")
    print(f"[catalogue] wrote: {candidates_csv}")
    print(f"[catalogue] wrote: {catalogue_json}")
    print(f"[catalogue] wrote: {summary_json}")
    print(
        "[catalogue] totals: "
        f"files={summary['total_files']} "
        f"unique_ids={summary['unique_source_ids']} "
        f"both_mp3_wav={summary['pairs']['source_ids_with_both']}"
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
