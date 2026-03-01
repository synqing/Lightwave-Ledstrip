#!/usr/bin/env python3
"""
Build a stratified ESV11 benchmark pack from harmonixset metrics + enrichment.

Outputs:
- test/music_corpus/harmonixset/esv11_benchmark/manifest.tsv
- test/music_corpus/harmonixset/esv11_benchmark/summary.json
- test/music_corpus/harmonixset/esv11_benchmark/audio_12k8/*.wav
- test/music_corpus/harmonixset/esv11_benchmark/audio_32k/*.wav
"""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import json
import math
import os
import subprocess
from collections import defaultdict
from pathlib import Path
from typing import Any


ROOT = Path(
    "/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3"
)
DEFAULT_METRICS = ROOT / "test/music_corpus/harmonixset/harmonixset_metrics.csv"
DEFAULT_ENRICHED = ROOT / "test/music_corpus/harmonixset/harmonixset_test_candidates_enriched.csv"
DEFAULT_OUT_DIR = ROOT / "test/music_corpus/harmonixset/esv11_benchmark"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build ESV11 benchmark pack")
    parser.add_argument("--metrics-csv", type=Path, default=DEFAULT_METRICS)
    parser.add_argument("--enriched-csv", type=Path, default=DEFAULT_ENRICHED)
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT_DIR)
    parser.add_argument("--size", type=int, default=36, help="Number of tracks in manifest")
    parser.add_argument("--clip-seconds", type=float, default=25.0, help="Duration per transcoded clip")
    parser.add_argument("--min-duration", type=float, default=45.0, help="Minimum source duration in seconds")
    parser.add_argument("--max-duration", type=float, default=320.0, help="Maximum source duration in seconds")
    parser.add_argument("--workers", type=int, default=max(1, (os.cpu_count() or 8) // 2))
    parser.add_argument("--refresh-audio", action="store_true", help="Force re-transcode WAV assets")
    return parser.parse_args()


def now_utc() -> str:
    return dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def safe_float(value: Any, default: float = 0.0) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def safe_int(value: Any, default: int = 0) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open("r", newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def clean_label(source_id: str, title: str) -> str:
    t = (title or "").strip()
    if not t:
        return source_id
    t = t.replace("\t", " ").replace("\n", " ").replace("\r", " ")
    return t[:120]


def select_tracks(rows: list[dict[str, Any]], total_size: int) -> list[dict[str, Any]]:
    buckets = ["060-080", "080-100", "100-120", "120-140", "140-170", "170-220"]
    by_bucket: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for row in rows:
        by_bucket[row["bpm_bucket"]].append(row)

    base = total_size // len(buckets)
    extra = total_size % len(buckets)
    quotas: dict[str, int] = {}
    for i, b in enumerate(buckets):
        quotas[b] = base + (1 if i < extra else 0)

    selected: list[dict[str, Any]] = []
    used_ids: set[str] = set()

    for bucket in buckets:
        candidates = by_bucket.get(bucket, [])
        if not candidates:
            continue
        quota = quotas[bucket]
        if quota <= 0:
            continue

        for row in candidates:
            row["mix_score"] = (
                0.40 * row["reference_score"]
                + 0.30 * row["challenge_score"]
                + 0.20 * row["octave_ambiguity"]
                + 0.10 * (1.0 - min(1.0, row["silence_ratio"]))
            )

        challenge_sorted = sorted(candidates, key=lambda r: r["challenge_score"], reverse=True)
        reference_sorted = sorted(candidates, key=lambda r: r["reference_score"], reverse=True)
        mixed_sorted = sorted(candidates, key=lambda r: r["mix_score"], reverse=True)

        # 1/3 challenge, 1/3 reference, 1/3 mixed.
        k_ch = max(1, quota // 3)
        k_ref = max(1, quota // 3)
        k_mix = quota - k_ch - k_ref
        if k_mix < 0:
            k_mix = 0

        bucket_pick: list[dict[str, Any]] = []
        for src in (challenge_sorted[: k_ch * 3], reference_sorted[: k_ref * 3], mixed_sorted[: max(1, k_mix * 3)]):
            for row in src:
                sid = row["source_id"]
                if sid in used_ids:
                    continue
                bucket_pick.append(row)
                used_ids.add(sid)
                if len(bucket_pick) >= quota:
                    break
            if len(bucket_pick) >= quota:
                break

        if len(bucket_pick) < quota:
            for row in mixed_sorted:
                sid = row["source_id"]
                if sid in used_ids:
                    continue
                bucket_pick.append(row)
                used_ids.add(sid)
                if len(bucket_pick) >= quota:
                    break

        selected.extend(bucket_pick)

    if len(selected) < total_size:
        remaining = [r for r in rows if r["source_id"] not in used_ids]
        remaining.sort(key=lambda r: r["mix_score"], reverse=True)
        for row in remaining:
            selected.append(row)
            used_ids.add(row["source_id"])
            if len(selected) >= total_size:
                break

    selected.sort(key=lambda r: (r["bpm_bucket"], r["source_id"]))
    return selected[:total_size]


def transcode_wav(src: Path, dst: Path, sample_rate: int, clip_seconds: float, refresh: bool) -> None:
    if dst.exists() and not refresh:
        return
    dst.parent.mkdir(parents=True, exist_ok=True)
    cmd = [
        "ffmpeg",
        "-v",
        "error",
        "-nostdin",
        "-y",
        "-i",
        str(src),
        "-ac",
        "1",
        "-ar",
        str(sample_rate),
        "-t",
        f"{clip_seconds:.3f}",
        "-c:a",
        "pcm_s16le",
        str(dst),
    ]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        msg = (proc.stderr or "").strip()
        raise RuntimeError(f"ffmpeg failed for {src.name}: {msg[:300]}")


def write_manifest(path: Path, rows: list[dict[str, Any]]) -> None:
    fields = [
        "track_id",
        "source_id",
        "label",
        "bpm_bucket",
        "bpm_est",
        "bpm_confidence",
        "octave_ambiguity",
        "challenge_score",
        "reference_score",
        "silence_ratio",
        "source_url",
        "yt_title",
        "yt_channel",
        "audio_12k8_wav",
        "audio_32k_wav",
    ]
    with path.open("w", encoding="utf-8", newline="") as f:
        f.write("\t".join(fields) + "\n")
        for row in rows:
            values = [str(row.get(k, "")) for k in fields]
            values = [v.replace("\t", " ").replace("\n", " ").replace("\r", " ") for v in values]
            f.write("\t".join(values) + "\n")


def main() -> int:
    args = parse_args()
    out_dir = args.out_dir.resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    audio_12_dir = out_dir / "audio_12k8"
    audio_32_dir = out_dir / "audio_32k"
    audio_12_dir.mkdir(parents=True, exist_ok=True)
    audio_32_dir.mkdir(parents=True, exist_ok=True)

    metrics_rows = read_rows(args.metrics_csv.resolve())
    enriched_rows = read_rows(args.enriched_csv.resolve())
    by_id_metrics = {r["source_id"]: r for r in metrics_rows if r.get("source_id")}

    joined: list[dict[str, Any]] = []
    for e in enriched_rows:
        sid = e.get("source_id", "").strip()
        if not sid:
            continue
        m = by_id_metrics.get(sid)
        if not m:
            continue
        if e.get("yt_fetch_ok", "").lower() not in {"true", "1"}:
            continue
        if m.get("analysis_error", ""):
            continue

        duration_s = safe_float(m.get("duration_s"))
        if duration_s < args.min_duration or duration_s > args.max_duration:
            continue

        bpm_bucket = m.get("bpm_bucket", "")
        if bpm_bucket not in {"060-080", "080-100", "100-120", "120-140", "140-170", "170-220"}:
            continue

        abs_path = Path(m.get("abs_path", ""))
        if not abs_path.exists():
            continue

        joined.append(
            {
                "source_id": sid,
                "abs_path": str(abs_path),
                "source_url": m.get("source_url", f"https://www.youtube.com/watch?v={sid}"),
                "bpm_bucket": bpm_bucket,
                "bpm_est": round(safe_float(m.get("bpm_est")), 3),
                "bpm_confidence": round(safe_float(m.get("bpm_confidence")), 6),
                "octave_ambiguity": round(safe_float(m.get("octave_ambiguity")), 6),
                "challenge_score": round(safe_float(m.get("challenge_score")), 6),
                "reference_score": round(safe_float(m.get("reference_score")), 6),
                "silence_ratio": round(safe_float(m.get("silence_ratio")), 6),
                "duration_s": round(duration_s, 3),
                "yt_title": e.get("yt_title", ""),
                "yt_channel": e.get("yt_channel", ""),
            }
        )

    if not joined:
        raise RuntimeError("No eligible tracks after filtering")

    selected = select_tracks(joined, args.size)
    if len(selected) < args.size:
        print(f"[pack] warning: requested {args.size} tracks, selected {len(selected)}")

    manifest_rows: list[dict[str, Any]] = []
    for i, row in enumerate(selected, start=1):
        sid = row["source_id"]
        src = Path(row["abs_path"])
        dst_12 = audio_12_dir / f"{sid}_12k8.wav"
        dst_32 = audio_32_dir / f"{sid}_32k.wav"
        transcode_wav(src, dst_12, 12800, args.clip_seconds, args.refresh_audio)
        transcode_wav(src, dst_32, 32000, args.clip_seconds, args.refresh_audio)

        label = clean_label(sid, row.get("yt_title", ""))
        manifest_rows.append(
            {
                "track_id": f"trk_{i:03d}_{sid}",
                "source_id": sid,
                "label": label,
                "bpm_bucket": row["bpm_bucket"],
                "bpm_est": row["bpm_est"],
                "bpm_confidence": row["bpm_confidence"],
                "octave_ambiguity": row["octave_ambiguity"],
                "challenge_score": row["challenge_score"],
                "reference_score": row["reference_score"],
                "silence_ratio": row["silence_ratio"],
                "source_url": row["source_url"],
                "yt_title": row.get("yt_title", ""),
                "yt_channel": row.get("yt_channel", ""),
                "audio_12k8_wav": str(dst_12),
                "audio_32k_wav": str(dst_32),
            }
        )

    manifest_path = out_dir / "manifest.tsv"
    write_manifest(manifest_path, manifest_rows)

    bucket_counts: dict[str, int] = defaultdict(int)
    for row in manifest_rows:
        bucket_counts[row["bpm_bucket"]] += 1

    summary = {
        "generated_at_utc": now_utc(),
        "metrics_csv": str(args.metrics_csv.resolve()),
        "enriched_csv": str(args.enriched_csv.resolve()),
        "out_dir": str(out_dir),
        "requested_size": args.size,
        "selected_size": len(manifest_rows),
        "clip_seconds": args.clip_seconds,
        "min_duration": args.min_duration,
        "max_duration": args.max_duration,
        "bucket_counts": dict(sorted(bucket_counts.items())),
        "manifest": str(manifest_path),
        "audio_12k8_dir": str(audio_12_dir),
        "audio_32k_dir": str(audio_32_dir),
    }
    summary_path = out_dir / "summary.json"
    summary_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")

    print(f"[pack] wrote: {manifest_path}")
    print(f"[pack] wrote: {summary_path}")
    print(f"[pack] selected: {len(manifest_rows)} tracks")
    print(f"[pack] buckets: {dict(sorted(bucket_counts.items()))}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
