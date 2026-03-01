#!/usr/bin/env python3
"""
Extract practical tempo-test metrics from harmonixset audio files.

Outputs:
- harmonixset_metrics.csv
- harmonixset_metrics_summary.json
- harmonixset_balanced_pack_120.csv
- harmonixset_challenge_pack_120.csv
- harmonixset_reference_pack_120.csv
"""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import json
import math
import os
import subprocess
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path
from typing import Any

import numpy as np


DEFAULT_INPUT_CSV = Path(
    "/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/"
    "firmware-v3/test/music_corpus/harmonixset/harmonixset_test_candidates.csv"
)
DEFAULT_OUT_DIR = Path(
    "/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/"
    "firmware-v3/test/music_corpus/harmonixset"
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Analyse harmonixset metrics")
    parser.add_argument("--input-csv", type=Path, default=DEFAULT_INPUT_CSV)
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT_DIR)
    parser.add_argument("--sample-rate", type=int, default=11025)
    parser.add_argument("--max-seconds", type=float, default=90.0)
    parser.add_argument("--workers", type=int, default=max(1, (os.cpu_count() or 8) // 2))
    return parser.parse_args()


def safe_float(x: Any, default: float = 0.0) -> float:
    try:
        return float(x)
    except (TypeError, ValueError):
        return default


def safe_int(x: Any, default: int = 0) -> int:
    try:
        return int(x)
    except (TypeError, ValueError):
        return default


def frame_signal(x: np.ndarray, frame: int, hop: int) -> np.ndarray:
    if x.size < frame:
        return np.empty((0, frame), dtype=np.float32)
    n = 1 + (x.size - frame) // hop
    shape = (n, frame)
    strides = (x.strides[0] * hop, x.strides[0])
    return np.lib.stride_tricks.as_strided(x, shape=shape, strides=strides).copy()


def decode_audio_mono(path: str, sample_rate: int, max_seconds: float) -> np.ndarray:
    cmd = [
        "ffmpeg",
        "-v",
        "error",
        "-nostdin",
        "-i",
        path,
        "-ac",
        "1",
        "-ar",
        str(sample_rate),
        "-t",
        f"{max_seconds:.3f}",
        "-f",
        "f32le",
        "-",
    ]
    proc = subprocess.run(cmd, capture_output=True)
    if proc.returncode != 0 or not proc.stdout:
        raise RuntimeError(proc.stderr.decode("utf-8", errors="ignore"))
    return np.frombuffer(proc.stdout, dtype=np.float32)


def lag_score(ac: np.ndarray, lag: int) -> float:
    if lag < 1 or lag >= ac.size:
        return 0.0
    lo = max(1, lag - 1)
    hi = min(ac.size - 1, lag + 1)
    return float(np.max(ac[lo : hi + 1]))


def bpm_bucket(bpm: float) -> str:
    if bpm < 80:
        return "060-080"
    if bpm < 100:
        return "080-100"
    if bpm < 120:
        return "100-120"
    if bpm < 140:
        return "120-140"
    if bpm < 170:
        return "140-170"
    return "170-220"


def analyse_waveform(x: np.ndarray, sr: int) -> dict[str, float]:
    eps = 1e-9
    if x.size < 4096:
        return {
            "duration_used_s": float(x.size) / float(sr),
            "rms_dbfs": -120.0,
            "peak_dbfs": -120.0,
            "crest_db": 0.0,
            "silence_ratio": 1.0,
            "clip_ratio": 0.0,
            "zcr": 0.0,
            "spectral_centroid_hz": 0.0,
            "spectral_rolloff85_hz": 0.0,
            "spectral_flatness": 0.0,
            "bass_energy_ratio": 0.0,
            "treble_energy_ratio": 0.0,
            "onset_rate_hz": 0.0,
            "onset_interval_cv": 1.0,
            "onset_strength_mean": 0.0,
            "onset_strength_std": 0.0,
            "bass_flux_ratio": 0.0,
            "bpm_raw": 0.0,
            "bpm_est": 0.0,
            "bpm_confidence": 0.0,
            "bpm_peak_ratio": 0.0,
            "bpm_half_score": 0.0,
            "bpm_double_score": 0.0,
            "octave_ambiguity": 1.0,
            "tempo_regularity": 0.0,
            "dynamic_range_db": 0.0,
        }

    x = np.nan_to_num(x, nan=0.0, posinf=0.0, neginf=0.0).astype(np.float32)
    duration_s = float(x.size) / float(sr)

    rms = float(np.sqrt(np.mean(x * x) + eps))
    peak = float(np.max(np.abs(x)) + eps)
    rms_dbfs = 20.0 * math.log10(max(rms, eps))
    peak_dbfs = 20.0 * math.log10(max(peak, eps))
    crest_db = peak_dbfs - rms_dbfs
    clip_ratio = float(np.mean(np.abs(x) >= 0.99))
    zcr = float(np.mean((x[1:] * x[:-1]) < 0.0))

    frame = 1024
    hop = 256
    frames = frame_signal(x, frame, hop)
    win = np.hanning(frame).astype(np.float32)
    fw = frames * win

    rms_frames = np.sqrt(np.mean(fw * fw, axis=1) + eps)
    rms_db = 20.0 * np.log10(np.maximum(rms_frames, eps))
    silence_ratio = float(np.mean(rms_db < -45.0))
    dynamic_range_db = float(np.percentile(rms_db, 95.0) - np.percentile(rms_db, 10.0))

    spec = np.fft.rfft(fw, axis=1)
    mag = np.abs(spec).astype(np.float32)
    power = mag * mag
    freqs = np.fft.rfftfreq(frame, d=1.0 / float(sr)).astype(np.float32)

    pow_sum = np.sum(power, axis=1) + eps
    centroid = np.sum(power * freqs[None, :], axis=1) / pow_sum
    spectral_centroid_hz = float(np.mean(centroid))

    csum = np.cumsum(power, axis=1)
    thresh = 0.85 * csum[:, -1]
    rolloff_idx = np.argmax(csum >= thresh[:, None], axis=1)
    spectral_rolloff85_hz = float(np.mean(freqs[rolloff_idx]))

    gmean = np.exp(np.mean(np.log(power + eps), axis=1))
    amean = np.mean(power + eps, axis=1)
    spectral_flatness = float(np.mean(gmean / amean))

    bass_idx = freqs <= 250.0
    treble_idx = freqs >= 4000.0
    bass_energy_ratio = float(np.mean(np.sum(power[:, bass_idx], axis=1) / (pow_sum + eps)))
    treble_energy_ratio = float(np.mean(np.sum(power[:, treble_idx], axis=1) / (pow_sum + eps)))

    log_mag = np.log1p(mag)
    diff = np.diff(log_mag, axis=0)
    flux = np.sum(np.maximum(diff, 0.0), axis=1)
    low_flux = np.sum(np.maximum(diff[:, bass_idx], 0.0), axis=1)
    flux_mean = float(np.mean(flux) + eps)
    flux_std = float(np.std(flux))
    bass_flux_ratio = float(np.mean(low_flux) / flux_mean)

    # Peak picking on onset envelope
    env = flux.astype(np.float32)
    peak_th = float(np.mean(env) + 0.8 * np.std(env))
    if env.size >= 3:
        p = (env[1:-1] > env[:-2]) & (env[1:-1] >= env[2:]) & (env[1:-1] > peak_th)
        peak_idx = np.where(p)[0] + 1
    else:
        peak_idx = np.array([], dtype=np.int64)

    env_rate = float(sr) / float(hop)
    onset_rate_hz = float(peak_idx.size / max(duration_s, eps))
    if peak_idx.size >= 2:
        intervals = np.diff(peak_idx) / env_rate
        onset_interval_cv = float(np.std(intervals) / (np.mean(intervals) + eps))
    else:
        onset_interval_cv = 1.0

    # BPM via onset autocorrelation
    env0 = env - np.mean(env)
    env0 = np.maximum(env0, 0.0)
    if np.sum(env0) < eps:
        bpm_raw = 0.0
        bpm_est = 0.0
        bpm_conf = 0.0
        peak_ratio = 0.0
        half_score = 0.0
        double_score = 0.0
        octave_ambiguity = 1.0
        tempo_regulariry = 0.0
    else:
        n = env0.size
        nfft = 1 << ((2 * n - 1).bit_length())
        f = np.fft.rfft(env0, n=nfft)
        ac = np.fft.irfft(f * np.conj(f), n=nfft)[:n]
        ac = ac / (ac[0] + eps)

        bpm_min = 60.0
        bpm_max = 220.0
        lag_min = max(1, int(round(env_rate * 60.0 / bpm_max)))
        lag_max = min(n - 1, int(round(env_rate * 60.0 / bpm_min)))

        seg = ac[lag_min : lag_max + 1]
        rel = int(np.argmax(seg))
        top_lag = lag_min + rel
        top_score = float(seg[rel])

        seg2 = seg.copy()
        lo = max(0, rel - 2)
        hi = min(seg2.size, rel + 3)
        seg2[lo:hi] = -np.inf
        second_score = float(np.max(seg2[np.isfinite(seg2)])) if np.isfinite(seg2).any() else 0.0

        bpm_raw = float(60.0 * env_rate / float(top_lag))
        raw_score = lag_score(ac, top_lag)

        half_score = 0.0
        double_score = 0.0
        candidates: list[tuple[float, float]] = [(bpm_raw, raw_score)]

        bpm_half = bpm_raw * 0.5
        if bpm_half >= bpm_min:
            lag_half = int(round(60.0 * env_rate / bpm_half))
            half_score = lag_score(ac, lag_half)
            w_half = half_score * (1.10 if 90.0 <= bpm_half <= 150.0 else 1.0)
            candidates.append((bpm_half, w_half))

        bpm_double = bpm_raw * 2.0
        if bpm_double <= bpm_max:
            lag_double = int(round(60.0 * env_rate / bpm_double))
            double_score = lag_score(ac, lag_double)
            w_double = double_score * (1.10 if 90.0 <= bpm_double <= 150.0 else 1.0)
            if bpm_raw < 80.0 and raw_score > 0.05:
                w_double *= 1.35
            candidates.append((bpm_double, w_double))

        bpm_est = float(max(candidates, key=lambda t: t[1])[0])
        bpm_conf = float(top_score / (top_score + second_score + eps))
        peak_ratio = float(top_score / (second_score + 1e-6))
        octave_ambiguity = float(
            1.0 - abs(double_score - half_score) / (max(double_score, half_score) + eps)
        )
        tempo_regulariry = float(1.0 / (1.0 + onset_interval_cv))

    return {
        "duration_used_s": duration_s,
        "rms_dbfs": rms_dbfs,
        "peak_dbfs": peak_dbfs,
        "crest_db": crest_db,
        "silence_ratio": silence_ratio,
        "clip_ratio": clip_ratio,
        "zcr": zcr,
        "spectral_centroid_hz": spectral_centroid_hz,
        "spectral_rolloff85_hz": spectral_rolloff85_hz,
        "spectral_flatness": spectral_flatness,
        "bass_energy_ratio": bass_energy_ratio,
        "treble_energy_ratio": treble_energy_ratio,
        "onset_rate_hz": onset_rate_hz,
        "onset_interval_cv": onset_interval_cv,
        "onset_strength_mean": flux_mean,
        "onset_strength_std": flux_std,
        "bass_flux_ratio": bass_flux_ratio,
        "bpm_raw": bpm_raw,
        "bpm_est": bpm_est,
        "bpm_confidence": bpm_conf,
        "bpm_peak_ratio": peak_ratio,
        "bpm_half_score": half_score,
        "bpm_double_score": double_score,
        "octave_ambiguity": octave_ambiguity,
        "tempo_regularity": tempo_regulariry,
        "dynamic_range_db": dynamic_range_db,
    }


def analyse_row(row: dict[str, str], sample_rate: int, max_seconds: float) -> dict[str, Any]:
    out: dict[str, Any] = dict(row)
    out["analysis_sr_hz"] = sample_rate
    out["analysis_max_seconds"] = max_seconds
    out["source_url"] = f"https://www.youtube.com/watch?v={row.get('source_id','')}"
    try:
        x = decode_audio_mono(row["abs_path"], sample_rate, max_seconds)
        metrics = analyse_waveform(x, sample_rate)
        out.update(metrics)
        out["bpm_bucket"] = bpm_bucket(metrics["bpm_est"]) if metrics["bpm_est"] > 0 else "unknown"
        challenge = (
            (1.0 - metrics["bpm_confidence"]) * 0.45
            + metrics["octave_ambiguity"] * 0.30
            + min(1.0, metrics["onset_interval_cv"]) * 0.15
            + min(1.0, metrics["silence_ratio"]) * 0.10
        )
        reference = (
            metrics["bpm_confidence"] * 0.45
            + metrics["tempo_regularity"] * 0.25
            + (1.0 - min(1.0, metrics["silence_ratio"])) * 0.15
            + min(1.0, metrics["onset_strength_mean"] / 4.0) * 0.15
        )
        out["challenge_score"] = float(challenge)
        out["reference_score"] = float(reference)
        out["analysis_error"] = ""
    except Exception as exc:  # noqa: BLE001
        out["analysis_error"] = str(exc)[:400]
        out["bpm_est"] = 0.0
        out["bpm_confidence"] = 0.0
        out["bpm_bucket"] = "error"
        out["challenge_score"] = 1.0
        out["reference_score"] = 0.0
    return out


def write_csv(path: Path, rows: list[dict[str, Any]], fields: list[str]) -> None:
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def pick_balanced_pack(rows: list[dict[str, Any]], per_bucket: int = 20) -> list[dict[str, Any]]:
    buckets = ["060-080", "080-100", "100-120", "120-140", "140-170", "170-220"]
    out: list[dict[str, Any]] = []
    for b in buckets:
        candidates = [
            r
            for r in rows
            if r.get("bpm_bucket") == b
            and r.get("analysis_error", "") == ""
            and safe_float(r.get("silence_ratio", 1.0)) < 0.5
        ]
        candidates.sort(key=lambda r: safe_float(r.get("reference_score")), reverse=True)
        out.extend(candidates[:per_bucket])
    return out


def main() -> int:
    args = parse_args()
    out_dir = args.out_dir.resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    with args.input_csv.open("r", newline="", encoding="utf-8") as f:
        in_rows = list(csv.DictReader(f))

    total = len(in_rows)
    print(f"[metrics] analysing {total} files with workers={args.workers}")

    results: list[dict[str, Any]] = []
    done = 0
    with ProcessPoolExecutor(max_workers=args.workers) as ex:
        futures = [
            ex.submit(analyse_row, row, args.sample_rate, args.max_seconds)
            for row in in_rows
        ]
        for fut in as_completed(futures):
            results.append(fut.result())
            done += 1
            if done % 25 == 0 or done == total:
                print(f"[metrics] processed {done}/{total}")

    results.sort(key=lambda r: (r.get("bpm_bucket", "zzz"), safe_float(r.get("bpm_est")), r.get("source_id", "")))

    fields = sorted({k for r in results for k in r.keys()})
    metrics_csv = out_dir / "harmonixset_metrics.csv"
    write_csv(metrics_csv, results, fields)

    valid = [r for r in results if not r.get("analysis_error")]
    challenge_pack = sorted(valid, key=lambda r: safe_float(r.get("challenge_score")), reverse=True)[:120]
    reference_pack = sorted(valid, key=lambda r: safe_float(r.get("reference_score")), reverse=True)[:120]
    balanced_pack = pick_balanced_pack(valid, per_bucket=20)

    write_csv(out_dir / "harmonixset_challenge_pack_120.csv", challenge_pack, fields)
    write_csv(out_dir / "harmonixset_reference_pack_120.csv", reference_pack, fields)
    write_csv(out_dir / "harmonixset_balanced_pack_120.csv", balanced_pack, fields)

    bpm_values = np.array([safe_float(r.get("bpm_est")) for r in valid if safe_float(r.get("bpm_est")) > 0.0])
    conf_values = np.array([safe_float(r.get("bpm_confidence")) for r in valid])
    bucket_counts: dict[str, int] = {}
    for r in valid:
        b = r.get("bpm_bucket", "unknown")
        bucket_counts[b] = bucket_counts.get(b, 0) + 1

    summary = {
        "generated_at_utc": dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z"),
        "input_csv": str(args.input_csv.resolve()),
        "total_rows": total,
        "valid_rows": len(valid),
        "error_rows": total - len(valid),
        "analysis_sample_rate_hz": args.sample_rate,
        "analysis_max_seconds": args.max_seconds,
        "bpm_bucket_counts": bucket_counts,
        "bpm_stats": {
            "min": float(np.min(bpm_values)) if bpm_values.size else 0.0,
            "p25": float(np.percentile(bpm_values, 25)) if bpm_values.size else 0.0,
            "p50": float(np.percentile(bpm_values, 50)) if bpm_values.size else 0.0,
            "p75": float(np.percentile(bpm_values, 75)) if bpm_values.size else 0.0,
            "max": float(np.max(bpm_values)) if bpm_values.size else 0.0,
        },
        "confidence_stats": {
            "p25": float(np.percentile(conf_values, 25)) if conf_values.size else 0.0,
            "p50": float(np.percentile(conf_values, 50)) if conf_values.size else 0.0,
            "p75": float(np.percentile(conf_values, 75)) if conf_values.size else 0.0,
            "mean": float(np.mean(conf_values)) if conf_values.size else 0.0,
        },
        "output_files": {
            "metrics_csv": "harmonixset_metrics.csv",
            "challenge_pack": "harmonixset_challenge_pack_120.csv",
            "reference_pack": "harmonixset_reference_pack_120.csv",
            "balanced_pack": "harmonixset_balanced_pack_120.csv",
        },
    }

    summary_json = out_dir / "harmonixset_metrics_summary.json"
    summary_json.write_text(json.dumps(summary, indent=2), encoding="utf-8")

    print(f"[metrics] wrote: {metrics_csv}")
    print(f"[metrics] wrote: {out_dir / 'harmonixset_challenge_pack_120.csv'}")
    print(f"[metrics] wrote: {out_dir / 'harmonixset_reference_pack_120.csv'}")
    print(f"[metrics] wrote: {out_dir / 'harmonixset_balanced_pack_120.csv'}")
    print(f"[metrics] wrote: {summary_json}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
