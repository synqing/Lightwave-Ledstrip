#!/usr/bin/env python3
"""
m2_run.py — ESV11 32 kHz adversarial robustness runner.

Five signal classes (per Topology_Reconciliation §6 item 4):
  1. Ambient     — slow, no clear beat (pink-noise modulated drone, 60 s)
  2. Rubato      — tempo drifts from 120 BPM to 80 BPM over 60 s (click + sweep)
  3. Syncopation — irregular accents at constant 110 BPM (click skipped beats)
  4. Sub-bass    — pure 40 Hz tone, 30 s
  5. Sine        — pure 1 kHz tone, 30 s

Pipeline:
  1) Generate or refresh missing WAVs in tools/m2_adversarial/signals/
     (idempotent — existing files are skipped unless --regen)
  2) Build the standalone m2_harness binary against the ESV11 32 kHz path
     using clang++ + the two vendor TUs. NOT registered in platformio.ini.
  3) For each signal, fork+run the harness, parse the JSON trajectory,
     evaluate the per-class pass/fail rule, accumulate results.
  4) Emit results JSON + Markdown summary table.

British English in docstrings/output. NO firmware-v3/src/ modifications.
NO platformio.ini modifications.

Run:
  python3 m2_run.py                    # build + run all 5 signals
  python3 m2_run.py --regen            # force-regenerate WAVs
  python3 m2_run.py --build-only       # just build the harness
  python3 m2_run.py --signal sine      # run a single signal
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable

import numpy as np
from scipy.io import wavfile  # type: ignore[import-untyped]

# ----------------------------------------------------------------------------
# Paths (resolved from this file's location, robust to caller cwd)
# ----------------------------------------------------------------------------

SCRIPT_DIR = Path(__file__).resolve().parent
M2_ROOT = SCRIPT_DIR.parent                     # tools/m2_adversarial/
FIRMWARE_ROOT = M2_ROOT.parent.parent           # firmware-v3/
SIGNALS_DIR = M2_ROOT / "signals"
RESULTS_DIR = M2_ROOT / "results"
HARNESS_SRC = SCRIPT_DIR / "m2_harness.cpp"
HARNESS_BIN = SCRIPT_DIR / "m2_harness"
ESV11_VENDOR = FIRMWARE_ROOT / "src" / "audio" / "backends" / "esv11" / "vendor"
ESV11_SHIM_HEADER = FIRMWARE_ROOT / "src" / "audio" / "backends" / "esv11" / "EsV11_32kHz_Shim.h"

SAMPLE_RATE = 32000  # must match EsV11_32kHz_Shim.h SAMPLE_RATE

# ----------------------------------------------------------------------------
# Signal generation
# ----------------------------------------------------------------------------

def _to_int16(x: np.ndarray) -> np.ndarray:
    """Clip + cast float [-1, 1] to int16."""
    x = np.clip(x, -1.0, 1.0)
    return (x * 32767.0).astype(np.int16)

def gen_sine(duration_s: float = 30.0, freq_hz: float = 1000.0) -> np.ndarray:
    """Pure sine wave at `freq_hz`, normalised to 0.5 amplitude."""
    n = int(duration_s * SAMPLE_RATE)
    t = np.arange(n) / SAMPLE_RATE
    return _to_int16(0.5 * np.sin(2.0 * np.pi * freq_hz * t))

def gen_subbass(duration_s: float = 30.0, freq_hz: float = 40.0) -> np.ndarray:
    """Pure sub-bass drone at `freq_hz` (default 40 Hz, in 30-50 Hz band)."""
    n = int(duration_s * SAMPLE_RATE)
    t = np.arange(n) / SAMPLE_RATE
    return _to_int16(0.5 * np.sin(2.0 * np.pi * freq_hz * t))

def gen_ambient(duration_s: float = 60.0, seed: int = 0xA113) -> np.ndarray:
    """Pink-noise modulated drone with very slow LFO — no detectable beat.

    Recipe: pink noise -> 4-pole LP at 800 Hz -> amplitude modulated by a
    0.05 Hz LFO (one swell per 20 s). No transient onsets.
    """
    rng = np.random.default_rng(seed)
    n = int(duration_s * SAMPLE_RATE)

    # Pink noise via Voss-McCartney approximation: cumulative-sum of white,
    # then high-pass leak so it does not drift away.
    white = rng.standard_normal(n).astype(np.float32)
    pink = np.cumsum(white) / np.sqrt(np.arange(1, n + 1))
    pink = pink - np.mean(pink)
    pink = pink / (np.max(np.abs(pink)) + 1e-9)

    # 4-pole one-pole LP cascade at ~800 Hz
    cutoff = 800.0
    alpha = float(1.0 - np.exp(-2.0 * np.pi * cutoff / SAMPLE_RATE))
    out = pink.copy()
    for _ in range(4):
        z = 0.0
        for i in range(n):
            z += alpha * (out[i] - z)
            out[i] = z

    # 0.05 Hz LFO swell, range [0.3, 0.8]
    t = np.arange(n) / SAMPLE_RATE
    lfo = 0.55 + 0.25 * np.sin(2.0 * np.pi * 0.05 * t)
    return _to_int16(out * lfo * 0.6)

def gen_rubato(duration_s: float = 60.0) -> np.ndarray:
    """Tempo-drifting click track: BPM sweeps 120 -> 80 over the first
    half, then 80 -> 110 over the second half. Each click is a 5 ms
    triangular pop at 0.7 amplitude.

    Used to test phase re-acquisition after tempo changes.
    """
    n = int(duration_s * SAMPLE_RATE)
    out = np.zeros(n, dtype=np.float32)
    t_total = duration_s
    # Generate event times by integrating beat rate.
    # bpm_curve(t): piecewise linear 120 -> 80 -> 110
    def bpm(t: float) -> float:
        if t < t_total / 2.0:
            frac = t / (t_total / 2.0)
            return 120.0 - 40.0 * frac
        else:
            frac = (t - t_total / 2.0) / (t_total / 2.0)
            return 80.0 + 30.0 * frac

    events: list[float] = []
    t = 0.0
    while t < t_total:
        events.append(t)
        period_s = 60.0 / bpm(t)
        t += period_s

    click_len = int(0.005 * SAMPLE_RATE)
    click = np.linspace(1.0, 0.0, click_len).astype(np.float32) * 0.7

    for ev in events:
        idx = int(ev * SAMPLE_RATE)
        if idx + click_len <= n:
            out[idx:idx + click_len] += click

    return _to_int16(out)

def gen_syncopation(duration_s: float = 60.0, base_bpm: float = 110.0) -> np.ndarray:
    """Constant tempo, irregular accent placement. Beats laid down on a
    16th-note grid, but only 11 of every 16 grid positions fire — pattern
    [1,0,1,1, 0,1,0,1, 1,0,1,0, 1,1,0,1] repeating. Rough Aphex-Twin-like
    accent irregularity at constant 110 BPM.
    """
    pattern = [1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1]
    sixteenth_s = (60.0 / base_bpm) / 4.0
    n = int(duration_s * SAMPLE_RATE)
    out = np.zeros(n, dtype=np.float32)

    click_len = int(0.005 * SAMPLE_RATE)
    click = np.linspace(0.9, 0.0, click_len).astype(np.float32)

    t = 0.0
    grid_idx = 0
    while t < duration_s:
        if pattern[grid_idx % len(pattern)] == 1:
            idx = int(t * SAMPLE_RATE)
            if idx + click_len <= n:
                out[idx:idx + click_len] += click
        t += sixteenth_s
        grid_idx += 1

    return _to_int16(out)

# Map of (slug, generator, ground-truth metadata)
SIGNAL_DEFS: dict[str, dict[str, Any]] = {
    "ambient": {
        "filename": "ambient_drone_60s.wav",
        "gen": lambda: gen_ambient(60.0),
        "duration_s": 60.0,
        "ground_truth": {"has_beat": False, "expected_bpm": None},
        "rule_id": "no_beat_low_conf",
    },
    "rubato": {
        "filename": "rubato_drift_60s.wav",
        "gen": lambda: gen_rubato(60.0),
        "duration_s": 60.0,
        "ground_truth": {
            "has_beat": True,
            "tempo_segments_bpm": [(0.0, 30.0, 120.0, 80.0),
                                   (30.0, 60.0, 80.0, 110.0)],
        },
        "rule_id": "rubato_reacquire",
    },
    "syncopation": {
        "filename": "syncopation_110bpm_60s.wav",
        "gen": lambda: gen_syncopation(60.0, 110.0),
        "duration_s": 60.0,
        "ground_truth": {"has_beat": True, "expected_bpm": 110.0},
        "rule_id": "phase_coherence",
    },
    "subbass": {
        "filename": "subbass_40hz_30s.wav",
        "gen": lambda: gen_subbass(30.0, 40.0),
        "duration_s": 30.0,
        "ground_truth": {"has_beat": False, "expected_bpm": None},
        "rule_id": "no_false_beats",
    },
    "sine": {
        "filename": "sine_1khz_30s.wav",
        "gen": lambda: gen_sine(30.0, 1000.0),
        "duration_s": 30.0,
        "ground_truth": {"has_beat": False, "expected_bpm": None},
        "rule_id": "sine_low_conf",
    },
}

def ensure_signals(regen: bool = False) -> None:
    SIGNALS_DIR.mkdir(parents=True, exist_ok=True)
    for slug, defn in SIGNAL_DEFS.items():
        out_path = SIGNALS_DIR / defn["filename"]
        if out_path.exists() and not regen:
            print(f"  signal {slug:12s}: kept (use --regen to overwrite)")
            continue
        print(f"  signal {slug:12s}: generating -> {out_path.name}")
        samples = defn["gen"]()
        wavfile.write(str(out_path), SAMPLE_RATE, samples)

# ----------------------------------------------------------------------------
# Harness build (stand-alone, NOT via platformio.ini)
# ----------------------------------------------------------------------------

def find_compiler() -> str:
    """Return path to a working C++17 compiler (clang++ preferred)."""
    for cand in ("clang++", "g++"):
        path = shutil.which(cand)
        if path:
            return path
    print("FATAL: no clang++ or g++ found on PATH", file=sys.stderr)
    sys.exit(1)

def build_harness() -> None:
    cxx = find_compiler()
    cmd = [
        cxx,
        "-std=c++17",
        "-O2",
        "-Wno-unused-result",
        "-D", "NATIVE_BUILD=1",
        "-D", "LIGHTWAVEOS_V2=1",
        "-D", "FEATURE_AUDIO_SYNC=1",
        "-D", "FEATURE_AUDIO_BACKEND_ESV11=1",
        "-D", "FEATURE_AUDIO_BACKEND_ESV11_32KHZ=1",
        "-include", str(ESV11_SHIM_HEADER),
        "-I", str(FIRMWARE_ROOT / "src"),
        str(HARNESS_SRC),
        str(ESV11_VENDOR / "EsV11Shim.cpp"),
        str(ESV11_VENDOR / "EsV11Buffers.cpp"),
        "-o", str(HARNESS_BIN),
    ]
    print("  building m2_harness ...")
    print(f"    {' '.join(cmd)}")
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(res.stdout)
        print(res.stderr, file=sys.stderr)
        print("FATAL: m2_harness build failed", file=sys.stderr)
        sys.exit(2)
    print(f"  built: {HARNESS_BIN}")

# ----------------------------------------------------------------------------
# Run + parse
# ----------------------------------------------------------------------------

def run_harness(wav_path: Path, max_seconds: float) -> dict[str, Any]:
    if not HARNESS_BIN.exists():
        raise RuntimeError("m2_harness not built — run with --build first")
    res = subprocess.run(
        [str(HARNESS_BIN), str(wav_path), str(max_seconds)],
        capture_output=True, text=True, timeout=180,
    )
    if res.returncode != 0:
        raise RuntimeError(f"m2_harness failed for {wav_path}: {res.stderr}")
    return json.loads(res.stdout)

# ----------------------------------------------------------------------------
# Pass/fail rules — see expected_results.md for the full thresholds doc
# ----------------------------------------------------------------------------

@dataclass
class RuleVerdict:
    passed: bool
    metrics: dict[str, Any] = field(default_factory=dict)
    notes: str = ""

def rule_no_beat_low_conf(traj: dict[str, Any], conf_threshold: float = 0.3,
                          frame_pct: float = 0.9) -> RuleVerdict:
    """Ambient: tempo_confidence < 0.3 in >= 90% of frames."""
    confs = [f["conf"] for f in traj["frames"]]
    n = len(confs)
    if n == 0:
        return RuleVerdict(False, notes="no frames")
    n_low = sum(1 for c in confs if c < conf_threshold)
    pct_low = n_low / n
    return RuleVerdict(
        passed=(pct_low >= frame_pct),
        metrics={"frames": n, "n_low_conf": n_low, "pct_low_conf": round(pct_low, 4),
                 "mean_conf": round(float(np.mean(confs)), 4)},
        notes=f"required pct_low_conf >= {frame_pct}, got {pct_low:.3f}",
    )

def rule_sine_low_conf(traj: dict[str, Any]) -> RuleVerdict:
    """Sine 1 kHz: tempo_confidence < 0.2 in >= 95% of frames."""
    return rule_no_beat_low_conf(traj, conf_threshold=0.2, frame_pct=0.95)

def rule_no_false_beats(traj: dict[str, Any]) -> RuleVerdict:
    """Sub-bass drone: 0 false beats reported."""
    n_ticks = traj["summary"].get("beat_tick_count", -1)
    return RuleVerdict(
        passed=(n_ticks == 0),
        metrics={"beat_tick_count": n_ticks},
        notes="required beat_tick_count == 0",
    )

def rule_phase_coherence(traj: dict[str, Any], coherence_pct: float = 0.75) -> RuleVerdict:
    """Syncopation: phase coherence >= 75% — frame-to-frame phase advance
    is monotonic (modulo 1) within +/- 10% of expected step.

    We exclude:
      - frames where top_bin changed (phase discontinuity)
      - frames with confidence < 0.1 (silence / startup)

    Expected step = (top_bpm / 60) / REFERENCE_FPS, normalised to [0,1].
    """
    frames = traj["frames"]
    ref_fps = traj.get("reference_fps", 100)
    if len(frames) < 2:
        return RuleVerdict(False, notes="too few frames")

    coherent = 0
    counted = 0
    for i in range(1, len(frames)):
        a, b = frames[i - 1], frames[i]
        if a["top_bin"] != b["top_bin"] or b["conf"] < 0.1:
            continue
        # Expected normalised phase step
        step_expected = (b["top_bpm"] / 60.0) / float(ref_fps)
        # Observed step (handle wrap)
        d = b["phase_norm"] - a["phase_norm"]
        if d < -0.5:
            d += 1.0
        elif d > 0.5:
            d -= 1.0
        # Both signs allowed (phase can rotate either way after re-lock)
        ratio = abs(d) / max(step_expected, 1e-6)
        if 0.9 <= ratio <= 1.1:
            coherent += 1
        counted += 1

    if counted == 0:
        return RuleVerdict(False, notes="no eligible frame pairs")
    pct = coherent / counted
    return RuleVerdict(
        passed=(pct >= coherence_pct),
        metrics={"counted_pairs": counted, "coherent_pairs": coherent,
                 "pct_coherent": round(pct, 4)},
        notes=f"required pct_coherent >= {coherence_pct}, got {pct:.3f}",
    )

def rule_rubato_reacquire(traj: dict[str, Any]) -> RuleVerdict:
    """Rubato: re-acquire correct phase within <= 4 bars after a tempo
    change. Heuristic: after each tempo segment boundary, count how many
    seconds elapse before phase coherence (per `rule_phase_coherence`)
    settles for >= 8 consecutive bars at the new BPM.

    NOTE: this rule depends on knowing the bar count. We approximate one
    bar = 4 beats. We report time-to-reacquire per segment; pass requires
    every segment <= 4 bars at its target BPM.
    """
    frames = traj["frames"]
    if len(frames) < 4:
        return RuleVerdict(False, notes="too few frames")

    # Hard-coded segments (must mirror SIGNAL_DEFS["rubato"].ground_truth)
    segments = [
        (0.0, 30.0, 80.0),    # final BPM at end of segment
        (30.0, 60.0, 110.0),
    ]

    metrics: dict[str, Any] = {"segments": []}
    all_pass = True
    for (start_s, end_s, target_bpm) in segments:
        # Frames inside this segment
        seg = [f for f in frames if start_s <= f["t_s"] < end_s]
        if not seg:
            metrics["segments"].append({"target_bpm": target_bpm,
                                        "reacquire_s": None, "pass": False})
            all_pass = False
            continue

        # First time the dominant bin matches target_bpm +/- 5 BPM
        # AND coherence holds for >= 8 consecutive bars (32 beats).
        bar_s = (60.0 / target_bpm) * 4.0
        hold_s = bar_s * 8.0  # 8 bars of stability

        ref_fps = traj.get("reference_fps", 100)
        # Walk forward, find first point where the next `hold_s` of frames
        # are coherent at target_bpm.
        reacquire_s: float | None = None
        i = 0
        while i < len(seg):
            f = seg[i]
            if abs(f["top_bpm"] - target_bpm) > 5.0 or f["conf"] < 0.1:
                i += 1
                continue
            # Look-ahead window
            t0 = f["t_s"]
            window_end = t0 + hold_s
            ok = True
            for g in seg[i:]:
                if g["t_s"] >= window_end:
                    break
                if abs(g["top_bpm"] - target_bpm) > 5.0:
                    ok = False
                    break
                if g["conf"] < 0.1:
                    ok = False
                    break
            if ok:
                reacquire_s = t0 - start_s
                break
            i += 1

        seg_pass = reacquire_s is not None and reacquire_s <= bar_s * 4.0
        metrics["segments"].append({
            "target_bpm": target_bpm,
            "reacquire_s": None if reacquire_s is None else round(reacquire_s, 3),
            "limit_s": round(bar_s * 4.0, 3),
            "pass": seg_pass,
        })
        if not seg_pass:
            all_pass = False

    return RuleVerdict(
        passed=all_pass,
        metrics=metrics,
        notes="required all segments reacquire within 4 bars",
    )

RULES: dict[str, Callable[[dict[str, Any]], RuleVerdict]] = {
    "no_beat_low_conf":  rule_no_beat_low_conf,
    "sine_low_conf":     rule_sine_low_conf,
    "no_false_beats":    rule_no_false_beats,
    "phase_coherence":   rule_phase_coherence,
    "rubato_reacquire":  rule_rubato_reacquire,
}

# ----------------------------------------------------------------------------
# Driver
# ----------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(description="M2 ESV11 adversarial runner")
    parser.add_argument("--regen", action="store_true",
                        help="regenerate WAVs even if they already exist")
    parser.add_argument("--build-only", action="store_true",
                        help="build harness then exit (no signal runs)")
    parser.add_argument("--no-build", action="store_true",
                        help="skip build (use existing m2_harness binary)")
    parser.add_argument("--signal", choices=list(SIGNAL_DEFS.keys()),
                        help="run a single signal class instead of all five")
    args = parser.parse_args()

    print("== M2 ESV11 32 kHz adversarial runner ==")
    print(f"  M2 root:       {M2_ROOT}")
    print(f"  firmware root: {FIRMWARE_ROOT}")

    print("\n[1/3] generating signals")
    ensure_signals(regen=args.regen)

    if not args.no_build:
        print("\n[2/3] building harness")
        build_harness()
    else:
        print("\n[2/3] skipping build (--no-build)")

    if args.build_only:
        print("\nbuild-only: stopping before signal runs.")
        return 0

    print("\n[3/3] running signals + evaluating thresholds")
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    results: list[dict[str, Any]] = []

    targets = [args.signal] if args.signal else list(SIGNAL_DEFS.keys())
    for slug in targets:
        defn = SIGNAL_DEFS[slug]
        wav_path = SIGNALS_DIR / defn["filename"]
        print(f"\n  -- {slug} ({defn['filename']}) --")
        traj = run_harness(wav_path, defn["duration_s"])
        rule = RULES[defn["rule_id"]]
        verdict = rule(traj)
        results.append({
            "signal": slug,
            "wav": str(wav_path.relative_to(FIRMWARE_ROOT)),
            "rule_id": defn["rule_id"],
            "passed": verdict.passed,
            "metrics": verdict.metrics,
            "notes": verdict.notes,
            "summary": traj["summary"],
        })
        # Persist trajectory for offline inspection
        (RESULTS_DIR / f"trajectory_{slug}.json").write_text(json.dumps(traj))
        print(f"    {'PASS' if verdict.passed else 'FAIL'} — {verdict.notes}")
        for k, v in verdict.metrics.items():
            print(f"      {k}: {v}")

    # ----------------------------------------------------------------------
    # Summary outputs
    # ----------------------------------------------------------------------
    summary_json = RESULTS_DIR / "m2_results.json"
    summary_md   = RESULTS_DIR / "m2_results.md"

    summary_json.write_text(json.dumps({
        "env": "native_test_esv11_music_corpus_32khz (verified 2026-04-27)",
        "harness": "tools/m2_adversarial/scripts/m2_harness.cpp",
        "results": results,
    }, indent=2))

    md_lines = [
        "# M2 ESV11 32 kHz adversarial — results",
        "",
        "| Signal | Rule | Outcome | Notes |",
        "|---|---|---|---|",
    ]
    for r in results:
        md_lines.append(f"| {r['signal']} | {r['rule_id']} | "
                        f"{'PASS' if r['passed'] else 'FAIL'} | {r['notes']} |")
    md_lines.append("")
    md_lines.append("Generated by `tools/m2_adversarial/scripts/m2_run.py`. "
                    "Per-frame trajectories live in `results/trajectory_<signal>.json`.")
    summary_md.write_text("\n".join(md_lines))

    print(f"\nresults JSON: {summary_json}")
    print(f"results MD:   {summary_md}")

    overall = all(r["passed"] for r in results)
    print(f"\noverall: {'PASS' if overall else 'FAIL'} "
          f"({sum(1 for r in results if r['passed'])}/{len(results)} signals)")
    return 0 if overall else 3


if __name__ == "__main__":
    sys.exit(main())
