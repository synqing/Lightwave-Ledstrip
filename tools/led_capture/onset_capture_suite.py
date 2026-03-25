#!/usr/bin/env python3
"""
Onset hardware capture suite for K1 devices.

Runs a fixed set of acoustic playback fixtures against the live firmware onset
detector, captures metadata-only serial frames, and applies both transport and
onset-specific gates.

Usage:
    python tools/led_capture/onset_capture_suite.py --serial /dev/cu.usbmodem1101
    python tools/led_capture/onset_capture_suite.py --serial /dev/cu.usbmodem1101 --fixtures silence,click120
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import subprocess
import sys
import tempfile
import threading
import time
import wave
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
from analyze_beats import (  # noqa: E402
    analyze_correlation,
    capture_with_metadata,
    format_gate_report,
    gate_has_failures,
    run_regression_gate,
    save_session,
)


REPO_ROOT = Path(__file__).resolve().parents[2]
MANIFEST_PATH = (
    REPO_ROOT
    / "firmware-v3/test/music_corpus/harmonixset/esv11_benchmark/manifest.tsv"
)
DEFAULT_OUTPUT_ROOT = REPO_ROOT / "tools/led_capture/output"
SAMPLE_RATE = 32000
LEAD_IN_SEC = 0.30
ANALYSIS_WARMUP_SEC = 1.50
CLICK_AMPLITUDE = 0.85


@dataclass(frozen=True)
class FixtureDef:
    fixture_id: str
    label: str
    kind: str
    duration_sec: float
    source_id: str = ""
    offset_sec: float = 0.0
    bpm: float = 0.0


FIXTURES = {
    "silence": FixtureDef(
        fixture_id="silence",
        label="Silence Floor",
        kind="silence",
        duration_sec=8.0,
    ),
    "click120": FixtureDef(
        fixture_id="click120",
        label="Click Truth 120 BPM",
        kind="click",
        duration_sec=12.0,
        bpm=120.0,
    ),
    "steady_pop": FixtureDef(
        fixture_id="steady_pop",
        label="Steady Pop 129 BPM",
        kind="music",
        duration_sec=15.0,
        source_id="T-sxSd1uwoU",
    ),
    "dynamic_sparse": FixtureDef(
        fixture_id="dynamic_sparse",
        label="Dynamic Sparse 161 BPM",
        kind="music",
        duration_sec=25.0,
        source_id="r9DBFTZTKPI",
    ),
    "fast_dense": FixtureDef(
        fixture_id="fast_dense",
        label="Fast Dense 172 BPM",
        kind="music",
        duration_sec=15.0,
        source_id="c7tOAGY59uQ",
    ),
}


def load_manifest_map(path: Path) -> dict[str, Path]:
    if not path.exists():
        raise FileNotFoundError(f"Manifest not found: {path}")
    mapping: dict[str, Path] = {}
    with path.open("r", encoding="utf-8") as f:
        reader = csv.DictReader(f, delimiter="\t")
        for row in reader:
            source_id = row.get("source_id", "").strip()
            audio_path = row.get("audio_32k_wav", "").strip()
            if source_id and audio_path:
                mapping[source_id] = Path(audio_path)
    return mapping


def write_pcm16_wav(path: Path, samples: np.ndarray, sample_rate: int = SAMPLE_RATE) -> None:
    samples = np.asarray(samples, dtype=np.int16)
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(sample_rate)
        wav.writeframes(samples.tobytes())


def generate_silence_wav(path: Path, duration_sec: float) -> None:
    n = int(round((duration_sec + LEAD_IN_SEC) * SAMPLE_RATE))
    write_pcm16_wav(path, np.zeros(n, dtype=np.int16))


def generate_click_wav(path: Path, duration_sec: float, bpm: float) -> None:
    total_sec = duration_sec + LEAD_IN_SEC
    total_samples = int(round(total_sec * SAMPLE_RATE))
    samples = np.zeros(total_samples, dtype=np.float32)

    click_len = int(0.020 * SAMPLE_RATE)
    env = np.exp(-np.linspace(0.0, 5.0, click_len)).astype(np.float32)
    tone = np.sin(2.0 * np.pi * 2200.0 * np.arange(click_len) / SAMPLE_RATE).astype(np.float32)
    click = CLICK_AMPLITUDE * env * tone

    beat_period = 60.0 / bpm
    t = LEAD_IN_SEC
    while t < total_sec:
        start = int(round(t * SAMPLE_RATE))
        end = min(total_samples, start + click_len)
        samples[start:end] += click[: end - start]
        t += beat_period

    samples = np.clip(samples, -1.0, 1.0)
    write_pcm16_wav(path, np.round(samples * 32767.0).astype(np.int16))


def trim_wav_with_lead_in(src: Path, dst: Path, duration_sec: float, offset_sec: float = 0.0) -> None:
    with wave.open(str(src), "rb") as wav:
        if wav.getnchannels() != 1 or wav.getsampwidth() != 2:
            raise ValueError(f"Expected 16-bit mono WAV: {src}")
        if wav.getframerate() != SAMPLE_RATE:
            raise ValueError(f"Expected {SAMPLE_RATE} Hz WAV: {src}")

        start_frame = int(round(offset_sec * SAMPLE_RATE))
        frame_count = int(round(duration_sec * SAMPLE_RATE))
        wav.setpos(min(start_frame, wav.getnframes()))
        raw = wav.readframes(frame_count)

    music = np.frombuffer(raw, dtype=np.int16)
    lead = np.zeros(int(round(LEAD_IN_SEC * SAMPLE_RATE)), dtype=np.int16)
    write_pcm16_wav(dst, np.concatenate([lead, music]))


def prepare_fixture_audio(fixture: FixtureDef, manifest_map: dict[str, Path], temp_dir: Path) -> Path:
    path = temp_dir / f"{fixture.fixture_id}.wav"
    if fixture.kind == "silence":
        generate_silence_wav(path, fixture.duration_sec)
        return path
    if fixture.kind == "click":
        generate_click_wav(path, fixture.duration_sec, fixture.bpm)
        return path

    src = manifest_map.get(fixture.source_id)
    if src is None:
        raise KeyError(f"Fixture source_id missing from manifest: {fixture.source_id}")
    trim_wav_with_lead_in(src, path, fixture.duration_sec, fixture.offset_sec)
    return path


def capture_duration_for_fixture(fixture: FixtureDef) -> float:
    return fixture.duration_sec + LEAD_IN_SEC


def compute_onset_metrics(metadata: list[dict], timestamps: np.ndarray) -> dict:
    n = len(metadata)
    duration = float(timestamps[-1] - timestamps[0]) if len(timestamps) >= 2 else 0.0
    onset = np.array([m.get("onset", False) for m in metadata], dtype=bool)
    onset_env = np.array([m.get("onset_env", 0.0) for m in metadata], dtype=np.float64)
    onset_event = np.array([m.get("onset_event", 0.0) for m in metadata], dtype=np.float64)
    kick = np.array([m.get("kick_trigger", False) for m in metadata], dtype=bool)
    snare = np.array([m.get("snare_trigger", False) for m in metadata], dtype=bool)
    hihat = np.array([m.get("hihat_trigger", False) for m in metadata], dtype=bool)
    rms = np.array([m.get("rms", 0.0) for m in metadata], dtype=np.float64)

    onset_count = int(onset.sum())
    onset_rate_hz = onset_count / max(0.1, duration)
    active_frac = float((onset_env > 0.0).mean()) if n > 0 else 0.0
    onset_env_positive = onset_env[onset_env > 0.0]
    onset_times = timestamps[np.where(onset)[0]] if onset_count > 0 else np.array([])
    if len(onset_times) >= 3:
        intervals = np.diff(onset_times)
        interval_cv = float(np.std(intervals) / max(1e-6, np.mean(intervals)))
    else:
        interval_cv = 0.0

    return {
        "n_frames": n,
        "duration": duration,
        "onset_count": onset_count,
        "onset_rate_hz": onset_rate_hz,
        "active_frac": active_frac,
        "max_onset_env": float(onset_env.max()) if len(onset_env) > 0 else 0.0,
        "mean_onset_env": float(onset_env.mean()) if len(onset_env) > 0 else 0.0,
        "mean_positive_onset_env": (
            float(onset_env_positive.mean()) if len(onset_env_positive) > 0 else 0.0
        ),
        "max_onset_event": float(onset_event.max()) if len(onset_event) > 0 else 0.0,
        "kick_count": int(kick.sum()),
        "snare_count": int(snare.sum()),
        "hihat_count": int(hihat.sum()),
        "interval_cv": interval_cv,
        "mean_rms": float(rms.mean()) if len(rms) > 0 else 0.0,
        "max_rms": float(rms.max()) if len(rms) > 0 else 0.0,
    }


def trim_capture_for_analysis(frames: np.ndarray, timestamps: np.ndarray,
                              metadata: list[dict], warmup_sec: float):
    if len(timestamps) < 2 or warmup_sec <= 0.0:
        return frames, timestamps, metadata
    cutoff = timestamps[0] + warmup_sec
    keep = np.where(timestamps >= cutoff)[0]
    if len(keep) < 10:
        return frames, timestamps, metadata
    start = int(keep[0])
    return frames[start:], timestamps[start:], metadata[start:]


def _gate_check(name: str, ok: bool, actual: str, expected: str) -> dict:
    return {
        "name": name,
        "status": "PASS" if ok else "FAIL",
        "actual": actual,
        "expected": expected,
    }


def evaluate_onset_gate(fixture: FixtureDef, metrics: dict) -> list[dict]:
    rate = metrics["onset_rate_hz"]
    active = metrics["active_frac"]
    env_max = metrics["max_onset_env"]
    kick = metrics["kick_count"]
    snare = metrics["snare_count"]
    hihat = metrics["hihat_count"]
    percussion = kick + snare + hihat

    if fixture.kind == "silence":
        return [
            _gate_check("false_trigger_rate", rate <= 0.20, f"{rate:.2f} Hz", "<= 0.20 Hz"),
            _gate_check("active_fraction", active <= 0.02, f"{active * 100:.2f}%", "<= 2.00%"),
            _gate_check("max_onset_env", env_max <= 0.02, f"{env_max:.3f}", "<= 0.020"),
            _gate_check("percussion_noise", percussion <= 2, str(percussion), "<= 2"),
        ]

    if fixture.kind == "click":
        return [
            _gate_check("event_rate", 0.80 <= rate <= 3.20, f"{rate:.2f} Hz", "0.80..3.20 Hz"),
            _gate_check("interval_cv", metrics["interval_cv"] <= 0.50,
                        f"{metrics['interval_cv']:.3f}", "<= 0.500"),
            _gate_check("max_onset_env", env_max >= 0.01, f"{env_max:.3f}", ">= 0.010"),
            _gate_check("active_fraction", active <= 0.20, f"{active * 100:.2f}%", "<= 20.00%"),
        ]

    checks = [
        _gate_check("event_rate", 0.40 <= rate <= 10.0, f"{rate:.2f} Hz", "0.40..10.00 Hz"),
        _gate_check("active_fraction", 0.005 <= active <= 0.20,
                    f"{active * 100:.2f}%", "0.50..20.00%"),
        _gate_check("max_onset_env", env_max >= 0.01, f"{env_max:.3f}", ">= 0.010"),
    ]

    if fixture.fixture_id == "dynamic_sparse":
        checks.append(_gate_check("sparse_floor", rate <= 5.00, f"{rate:.2f} Hz", "<= 5.00 Hz"))
        checks.append(_gate_check("percussion_presence", percussion >= 3, str(percussion), ">= 3"))
    else:
        checks.append(_gate_check("percussion_presence", percussion >= 5, str(percussion), ">= 5"))

    return checks


def onset_gate_has_failures(results: list[dict]) -> bool:
    return any(r["status"] == "FAIL" for r in results)


def format_onset_gate_report(results: list[dict], label: str) -> str:
    lines = [f"  Onset Gate: {label}", "  " + ("-" * 50)]
    fails = 0
    for r in results:
        lines.append(
            f"  [{r['status']}] {r['name']:<20} {r['actual']:<12} {r['expected']}"
        )
        if r["status"] == "FAIL":
            fails += 1
    lines.append("  " + ("-" * 50))
    lines.append(f"  RESULT: {'FAIL' if fails else 'PASS'}")
    lines.append("")
    return "\n".join(lines)


def parse_fixture_list(raw: str) -> list[FixtureDef]:
    selected = []
    for name in [part.strip() for part in raw.split(",") if part.strip()]:
        if name not in FIXTURES:
            raise KeyError(f"Unknown fixture: {name}")
        selected.append(FIXTURES[name])
    return selected


def launch_afplay(path: Path) -> subprocess.Popen:
    return subprocess.Popen(
        ["afplay", str(path)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def ensure_stopped(proc: subprocess.Popen | None) -> None:
    if proc is None:
        return
    try:
        proc.wait(timeout=1.0)
    except subprocess.TimeoutExpired:
        proc.terminate()
        try:
            proc.wait(timeout=1.0)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=1.0)


def run_fixture(port: str, fixture: FixtureDef, audio_path: Path, output_dir: Path,
                fps: int, tap: str) -> dict:
    playback_proc: subprocess.Popen | None = None
    launch_lock = threading.Lock()

    def on_stream_started() -> None:
        nonlocal playback_proc
        with launch_lock:
            playback_proc = launch_afplay(audio_path)

    duration = capture_duration_for_fixture(fixture)
    capture = capture_with_metadata(
        port,
        duration,
        fps=fps,
        tap=tap,
        fmt="meta",
        on_stream_started=on_stream_started,
    )
    ensure_stopped(playback_proc)

    if capture is None:
        return {
            "fixture": fixture,
            "capture_failed": True,
            "health_gate": [],
            "onset_gate": [],
        }

    frames, timestamps, metadata = capture
    session_path = output_dir / f"{fixture.fixture_id}.npz"
    save_session(
        str(session_path),
        frames,
        timestamps,
        metadata,
        label_a=fixture.label,
    )

    frames_trim, timestamps_trim, metadata_trim = trim_capture_for_analysis(
        frames, timestamps, metadata, ANALYSIS_WARMUP_SEC
    )
    analysis = analyze_correlation(frames_trim, timestamps_trim, metadata_trim, fixture.label)
    onset_metrics = compute_onset_metrics(metadata_trim, timestamps_trim)
    health_gate = run_regression_gate(analysis)
    onset_gate = evaluate_onset_gate(fixture, onset_metrics)

    return {
        "fixture": fixture,
        "session_path": session_path,
        "analysis": analysis,
        "onset_metrics": onset_metrics,
        "health_gate": health_gate,
        "onset_gate": onset_gate,
        "capture_failed": False,
    }


def summarise_results(results: list[dict]) -> str:
    lines = [
        "",
        "Onset Hardware Capture Suite",
        "=" * 72,
        f" {'Fixture':<18} | {'Rate':>6} | {'Act%':>6} | {'MaxEnv':>6} | {'K/S/H':<13} | Verdict",
        f" {'-'*18}-+-{'-'*6}-+-{'-'*6}-+-{'-'*6}-+-{'-'*13}-+--------",
    ]
    overall_fail = False

    for r in results:
        fixture = r["fixture"]
        if r["capture_failed"]:
            lines.append(f" {fixture.fixture_id:<18} |   --   |   --   |   --   | {'--/--/--':<13} | FAIL")
            overall_fail = True
            continue

        onset = r["onset_metrics"]
        verdict = "PASS"
        if gate_has_failures(r["health_gate"]) or onset_gate_has_failures(r["onset_gate"]):
            verdict = "FAIL"
            overall_fail = True

        lines.append(
            f" {fixture.fixture_id:<18} | {onset['onset_rate_hz']:>6.2f} | "
            f"{onset['active_frac'] * 100:>5.2f}% | {onset['max_onset_env']:>6.3f} | "
            f"{onset['kick_count']:>2}/{onset['snare_count']:<2}/{onset['hihat_count']:<2}         | {verdict}"
        )

    lines.append("=" * 72)
    lines.append(f"Overall: {'FAIL' if overall_fail else 'PASS'}")
    lines.append("")
    return "\n".join(lines)


def json_safe_metrics(metrics: dict) -> dict:
    safe = {}
    for key, value in metrics.items():
        if key.startswith("_"):
            continue
        if isinstance(value, np.generic):
            safe[key] = value.item()
        else:
            safe[key] = value
    return safe


def json_safe_gate_list(results: list) -> list[dict]:
    safe = []
    for r in results:
        safe.append({
            "name": getattr(r, "name", ""),
            "status": getattr(r, "status", ""),
            "actual": getattr(r, "actual", 0),
            "threshold": getattr(r, "threshold", 0),
            "direction": getattr(r, "direction", ""),
            "actual_display": getattr(r, "actual_display", ""),
            "threshold_display": getattr(r, "threshold_display", ""),
        })
    return safe


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run the onset hardware capture suite against a fixed clip pack."
    )
    parser.add_argument("--serial", required=True, help="Serial port for the K1 device")
    parser.add_argument("--fixtures", default="silence,click120,steady_pop,dynamic_sparse,fast_dense",
                        help="Comma-separated fixture ids")
    parser.add_argument("--fps", type=int, default=30, help="Capture FPS (default: 30)")
    parser.add_argument("--tap", default="b", choices=["a", "b", "c"],
                        help="Capture tap (default: b)")
    parser.add_argument("--output", default="",
                        help="Output directory (default: tools/led_capture/output/onset_suite_<timestamp>)")
    args = parser.parse_args()

    manifest_map = load_manifest_map(MANIFEST_PATH)
    fixtures = parse_fixture_list(args.fixtures)

    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    output_dir = Path(args.output) if args.output else (DEFAULT_OUTPUT_ROOT / f"onset_suite_{stamp}")
    output_dir.mkdir(parents=True, exist_ok=True)

    summary_path = output_dir / "summary.json"
    report_path = output_dir / "report.txt"

    results = []
    with tempfile.TemporaryDirectory(prefix="lw_onset_suite_") as temp_root:
        temp_dir = Path(temp_root)
        for fixture in fixtures:
            print(f"\n[{fixture.fixture_id}] Preparing audio fixture...")
            audio_path = prepare_fixture_audio(fixture, manifest_map, temp_dir)
            print(f"[{fixture.fixture_id}] Capturing {capture_duration_for_fixture(fixture):.2f}s on {args.serial}...")

            result = run_fixture(args.serial, fixture, audio_path, output_dir, args.fps, args.tap)
            results.append(result)

            if result["capture_failed"]:
                print(f"[{fixture.fixture_id}] Capture failed.")
                continue

            print(format_gate_report(result["health_gate"], fixture.label))
            print(format_onset_gate_report(result["onset_gate"], fixture.label))

    serialisable = []
    for r in results:
        row = {
            "fixture_id": r["fixture"].fixture_id,
            "label": r["fixture"].label,
            "capture_failed": r["capture_failed"],
        }
        if not r["capture_failed"]:
            row["analysis"] = json_safe_metrics(r["analysis"])
            row["onset_metrics"] = r["onset_metrics"]
            row["health_gate"] = json_safe_gate_list(r["health_gate"])
            row["onset_gate"] = r["onset_gate"]
            row["session_path"] = str(r["session_path"])
        serialisable.append(row)

    summary_path.write_text(json.dumps(serialisable, indent=2, default=float) + "\n", encoding="utf-8")
    report = summarise_results(results)
    report_path.write_text(report, encoding="utf-8")
    print(report)
    print(f"[Save] Summary: {summary_path}")
    print(f"[Save] Report:  {report_path}")

    any_fail = False
    for r in results:
        if r["capture_failed"]:
            any_fail = True
            continue
        if gate_has_failures(r["health_gate"]) or onset_gate_has_failures(r["onset_gate"]):
            any_fail = True
    return 1 if any_fail else 0


if __name__ == "__main__":
    raise SystemExit(main())
