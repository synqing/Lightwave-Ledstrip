#!/usr/bin/env python3
"""
Onset-focused hardware capture runner for K1 devices.

Runs fixed audio fixtures against the existing serial capture pipeline,
applies transport gates plus onset-specific gates, and saves session
artefacts for later inspection.
"""

from __future__ import annotations

import argparse
import math
import shlex
import shutil
import subprocess
import sys
import threading
import time
import wave
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Optional

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
from analyze_beats import (
    analyze_correlation,
    capture_with_metadata,
    format_gate_report,
    gate_has_failures,
    render_dashboard,
    run_regression_gate,
    save_session,
)


ROOT = Path(__file__).resolve().parents[2]
BENCH_32K = ROOT / "firmware-v3/test/music_corpus/harmonixset/esv11_benchmark/audio_32k"


@dataclass(frozen=True)
class OnsetThresholds:
    onset_rate_min_hz: Optional[float] = None
    onset_rate_max_hz: Optional[float] = None
    onset_active_max: Optional[float] = None
    flux_p95_min: Optional[float] = None
    flux_p95_max: Optional[float] = None
    flux_p99_min: Optional[float] = None


@dataclass(frozen=True)
class FixtureSpec:
    name: str
    label: str
    description: str
    duration_s: float
    source: str
    thresholds: OnsetThresholds
    path: Optional[Path] = None
    bpm: Optional[float] = None


@dataclass(frozen=True)
class GateCheck:
    name: str
    status: str
    actual_display: str
    threshold_display: str


@dataclass(frozen=True)
class FixtureRun:
    spec: FixtureSpec
    clip_path: Optional[Path]
    capture_seconds: float
    metrics: dict
    transport_gates: list
    onset_gates: list[GateCheck]


FIXTURES: dict[str, FixtureSpec] = {
    "silence": FixtureSpec(
        name="silence",
        label="Silence",
        description="False-trigger floor check with generated silence.",
        duration_s=10.0,
        source="generated_silence",
        thresholds=OnsetThresholds(
            onset_rate_max_hz=0.10,
            onset_active_max=0.05,
            flux_p95_max=0.010,
        ),
    ),
    "click120": FixtureSpec(
        name="click120",
        label="Click 120 BPM",
        description="Generated broadband click track for clean onset timing.",
        duration_s=20.0,
        source="generated_click",
        bpm=120.0,
        thresholds=OnsetThresholds(
            onset_rate_min_hz=0.15,
            onset_rate_max_hz=6.0,
            onset_active_max=0.18,
            flux_p95_min=0.008,
            flux_p99_min=0.015,
        ),
    ),
    "four4_128": FixtureSpec(
        name="four4_128",
        label="Four-on-floor 128 BPM",
        description="Generated kick-led pulse train for steady 4/4 response.",
        duration_s=20.0,
        source="generated_four4",
        bpm=128.0,
        thresholds=OnsetThresholds(
            onset_rate_min_hz=0.10,
            onset_rate_max_hz=5.0,
            onset_active_max=0.18,
            flux_p95_min=0.008,
            flux_p99_min=0.015,
        ),
    ),
    "sparse_ballad": FixtureSpec(
        name="sparse_ballad",
        label="Sparse Ballad",
        description="Committed harmonixset clip with a slow, sparse onset profile.",
        duration_s=25.0,
        source="repo_wav",
        path=BENCH_32K / "r9DBFTZTKPI_32k.wav",
        thresholds=OnsetThresholds(
            onset_rate_min_hz=0.02,
            onset_rate_max_hz=3.0,
            onset_active_max=0.10,
            flux_p95_min=0.005,
        ),
    ),
    "dense_music": FixtureSpec(
        name="dense_music",
        label="Dense Music",
        description="Committed harmonixset clip with dense transient activity.",
        duration_s=25.0,
        source="repo_wav",
        path=BENCH_32K / "T-sxSd1uwoU_32k.wav",
        thresholds=OnsetThresholds(
            onset_rate_min_hz=0.12,
            onset_rate_max_hz=8.0,
            onset_active_max=0.22,
            flux_p95_min=0.008,
            flux_p99_min=0.015,
        ),
    ),
}


def _default_playback_cmd() -> Optional[str]:
    if sys.platform == "darwin" and shutil.which("afplay"):
        return 'afplay {clip}'
    return None


def _write_wav(path: Path, samples: np.ndarray, sample_rate: int = 32000) -> Path:
    clipped = np.clip(samples, -0.99, 0.99)
    pcm = (clipped * 32767.0).astype(np.int16)
    with wave.open(str(path), "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        wf.writeframes(pcm.tobytes())
    return path


def _synth_silence(duration_s: float, sample_rate: int = 32000) -> np.ndarray:
    n = max(1, int(duration_s * sample_rate))
    return np.zeros(n, dtype=np.float32)


def _synth_click_track(duration_s: float, bpm: float, sample_rate: int = 32000) -> np.ndarray:
    n = max(1, int(duration_s * sample_rate))
    out = np.zeros(n, dtype=np.float32)
    beat_period = 60.0 / bpm
    click_len = int(0.018 * sample_rate)
    rng = np.random.default_rng(120)

    for beat in np.arange(0.0, duration_s, beat_period):
        start = int(beat * sample_rate)
        end = min(n, start + click_len)
        if end <= start:
            continue
        env = np.exp(-np.linspace(0.0, 7.0, end - start, dtype=np.float32))
        noise = rng.standard_normal(end - start).astype(np.float32)
        out[start:end] += 0.70 * env * noise

    return np.tanh(out)


def _synth_four_on_floor(duration_s: float, bpm: float, sample_rate: int = 32000) -> np.ndarray:
    n = max(1, int(duration_s * sample_rate))
    out = np.zeros(n, dtype=np.float32)
    beat_period = 60.0 / bpm
    kick_len = int(0.14 * sample_rate)

    for beat in np.arange(0.0, duration_s, beat_period):
        start = int(beat * sample_rate)
        end = min(n, start + kick_len)
        if end <= start:
            continue
        t = np.arange(end - start, dtype=np.float32) / float(sample_rate)
        env = np.exp(-18.0 * t)
        pitch = 84.0 - 30.0 * np.minimum(1.0, t / 0.06)
        phase = 2.0 * np.pi * np.cumsum(pitch) / float(sample_rate)
        kick = 0.85 * np.sin(phase) * env
        click_env = np.exp(-np.linspace(0.0, 9.0, end - start, dtype=np.float32))
        out[start:end] += kick + 0.12 * click_env

    return np.tanh(out)


def _resolve_fixture_audio(spec: FixtureSpec, fixtures_dir: Path) -> Optional[Path]:
    if spec.source == "generated_silence":
        path = fixtures_dir / f"{spec.name}.wav"
        return _write_wav(path, _synth_silence(spec.duration_s))
    if spec.source == "generated_click":
        path = fixtures_dir / f"{spec.name}.wav"
        return _write_wav(path, _synth_click_track(spec.duration_s, spec.bpm or 120.0))
    if spec.source == "generated_four4":
        path = fixtures_dir / f"{spec.name}.wav"
        return _write_wav(path, _synth_four_on_floor(spec.duration_s, spec.bpm or 128.0))
    if spec.source == "repo_wav":
        if spec.path is None or not spec.path.exists():
            raise FileNotFoundError(f"Fixture missing: {spec.path}")
        return spec.path
    raise ValueError(f"Unknown fixture source: {spec.source}")


def _start_playback_thread(cmd_template: Optional[str], clip_path: Optional[Path],
                           delay_s: float) -> Optional[threading.Thread]:
    if clip_path is None or cmd_template is None:
        return None

    def worker() -> None:
        time.sleep(max(0.0, delay_s))
        cmd = cmd_template.format(clip=shlex.quote(str(clip_path)))
        subprocess.run(cmd, shell=True, check=False)

    thread = threading.Thread(target=worker, daemon=True)
    thread.start()
    return thread


def _capture_seconds(spec: FixtureSpec, playback_cmd: Optional[str],
                     playback_delay_s: float, tail_s: float) -> float:
    if spec.name == "silence" or playback_cmd is None:
        return spec.duration_s
    return playback_delay_s + spec.duration_s + tail_s


def _safe_percentile(values: np.ndarray, q: float) -> float:
    if values.size == 0:
        return 0.0
    return float(np.percentile(values, q))


def _build_onset_metrics(metrics: dict) -> dict[str, float]:
    onset = np.asarray(metrics.get("_onset", np.array([])), dtype=bool)
    flux = np.asarray(metrics.get("_flux", np.array([])), dtype=np.float64)
    duration = float(metrics.get("duration", 0.0))

    onset_rate = float(onset.sum()) / max(0.1, duration)
    onset_active = float(onset.mean()) if onset.size > 0 else 0.0

    return {
        "onset_rate_hz": onset_rate,
        "onset_active_rate": onset_active,
        "flux_mean": float(flux.mean()) if flux.size > 0 else 0.0,
        "flux_p95": _safe_percentile(flux, 95.0),
        "flux_p99": _safe_percentile(flux, 99.0),
    }


def _gate_line(name: str, ok: bool, actual: str, threshold: str) -> GateCheck:
    return GateCheck(
        name=name,
        status="PASS" if ok else "FAIL",
        actual_display=actual,
        threshold_display=threshold,
    )


def _run_onset_gates(spec: FixtureSpec, onset_metrics: dict[str, float]) -> list[GateCheck]:
    checks: list[GateCheck] = []
    th = spec.thresholds

    if th.onset_rate_min_hz is not None:
        val = onset_metrics["onset_rate_hz"]
        checks.append(_gate_line(
            "onset_rate_min",
            val >= th.onset_rate_min_hz,
            f"{val:.3f} Hz",
            f">= {th.onset_rate_min_hz:.3f} Hz",
        ))

    if th.onset_rate_max_hz is not None:
        val = onset_metrics["onset_rate_hz"]
        checks.append(_gate_line(
            "onset_rate_max",
            val <= th.onset_rate_max_hz,
            f"{val:.3f} Hz",
            f"<= {th.onset_rate_max_hz:.3f} Hz",
        ))

    if th.onset_active_max is not None:
        val = onset_metrics["onset_active_rate"]
        checks.append(_gate_line(
            "onset_active_max",
            val <= th.onset_active_max,
            f"{val * 100.0:.2f}%",
            f"<= {th.onset_active_max * 100.0:.2f}%",
        ))

    if th.flux_p95_min is not None:
        val = onset_metrics["flux_p95"]
        checks.append(_gate_line(
            "flux_p95_min",
            val >= th.flux_p95_min,
            f"{val:.4f}",
            f">= {th.flux_p95_min:.4f}",
        ))

    if th.flux_p95_max is not None:
        val = onset_metrics["flux_p95"]
        checks.append(_gate_line(
            "flux_p95_max",
            val <= th.flux_p95_max,
            f"{val:.4f}",
            f"<= {th.flux_p95_max:.4f}",
        ))

    if th.flux_p99_min is not None:
        val = onset_metrics["flux_p99"]
        checks.append(_gate_line(
            "flux_p99_min",
            val >= th.flux_p99_min,
            f"{val:.4f}",
            f">= {th.flux_p99_min:.4f}",
        ))

    return checks


def _fixture_failed(run: FixtureRun) -> bool:
    return gate_has_failures(run.transport_gates) or any(c.status == "FAIL" for c in run.onset_gates)


def _format_onset_gate_report(checks: list[GateCheck], label: str) -> str:
    lines = ["", f"  Onset Gate: {label}", "  " + ("\u2500" * 50)]
    n_pass = 0
    n_fail = 0

    for check in checks:
        if check.status == "PASS":
            n_pass += 1
        else:
            n_fail += 1
        pretty = check.name.replace("_", " ")
        lines.append(
            f"  [{check.status}] {pretty:<22s} {check.actual_display:<12s} {check.threshold_display}"
        )

    lines.append("  " + ("\u2500" * 50))
    verdict = f"FAIL ({n_fail} failed)" if n_fail > 0 else f"PASS ({n_pass}/{max(1, n_pass)})"
    lines.append(f"  RESULT: {verdict}")
    lines.append("")
    return "\n".join(lines)


def _save_fixture_outputs(output_dir: Path, run: FixtureRun, frames, timestamps, metadata) -> None:
    slug = run.spec.name
    session_path = output_dir / f"{slug}.npz"
    dashboard_path = output_dir / f"{slug}.png"

    save_session(
        str(session_path),
        frames,
        timestamps,
        metadata,
        label_a=run.spec.label,
    )

    dashboard = render_dashboard(run.metrics)
    if dashboard is not None:
        dashboard.save(dashboard_path, "PNG")


def _run_fixture(spec: FixtureSpec, args, output_dir: Path, fixtures_dir: Path) -> FixtureRun:
    clip_path = _resolve_fixture_audio(spec, fixtures_dir)
    playback_cmd = None if spec.name == "silence" or args.no_playback else args.playback_cmd
    capture_seconds = _capture_seconds(spec, playback_cmd, args.playback_delay, args.tail)

    print(f"\n{'=' * 72}")
    print(f"{spec.label} — {spec.description}")
    print(f"fixture={spec.name} capture={capture_seconds:.1f}s clip={clip_path if clip_path else 'none'}")
    print(f"{'=' * 72}")

    playback_thread = _start_playback_thread(playback_cmd, clip_path, args.playback_delay)
    result = capture_with_metadata(
        args.serial,
        capture_seconds,
        fps=args.fps,
        tap=args.tap,
        fmt=args.format,
    )
    if playback_thread is not None:
        playback_thread.join(timeout=max(1.0, spec.duration_s + args.playback_delay + args.tail + 1.0))

    if result is None:
        raise RuntimeError(f"Capture failed for fixture '{spec.name}'")

    frames, timestamps, metadata = result
    metrics = analyze_correlation(frames, timestamps, metadata, spec.label)
    onset_metrics = _build_onset_metrics(metrics)
    metrics.update(onset_metrics)
    transport_gates = run_regression_gate(metrics)
    onset_gates = _run_onset_gates(spec, onset_metrics)

    run = FixtureRun(
        spec=spec,
        clip_path=clip_path,
        capture_seconds=capture_seconds,
        metrics=metrics,
        transport_gates=transport_gates,
        onset_gates=onset_gates,
    )
    _save_fixture_outputs(output_dir, run, frames, timestamps, metadata)
    return run


def _write_summary(output_dir: Path, runs: list[FixtureRun]) -> Path:
    lines = []
    now = datetime.now().strftime("%Y-%m-%d %H:%M")
    lines.append(f"Onset Capture Suite — {now}")
    lines.append("=" * 72)

    overall_fail = False
    for run in runs:
        m = run.metrics
        lines.append("")
        lines.append(f"{run.spec.label} [{run.spec.name}]")
        lines.append(f"  clip: {run.clip_path if run.clip_path else 'none'}")
        lines.append(
            f"  capture: {m.get('duration', 0.0):.1f}s, {m.get('actual_fps', 0.0):.1f} FPS, "
            f"onset_rate={m.get('onset_rate_hz', 0.0):.3f} Hz, "
            f"onset_active={m.get('onset_active_rate', 0.0) * 100.0:.2f}%, "
            f"flux_p95={m.get('flux_p95', 0.0):.4f}, flux_p99={m.get('flux_p99', 0.0):.4f}"
        )

        transport_failed = gate_has_failures(run.transport_gates)
        onset_failed = any(c.status == "FAIL" for c in run.onset_gates)
        status = "FAIL" if transport_failed or onset_failed else "PASS"
        lines.append(f"  verdict: {status}")
        overall_fail = overall_fail or status == "FAIL"

        lines.append(format_gate_report(run.transport_gates, run.spec.label))
        lines.append(_format_onset_gate_report(run.onset_gates, run.spec.label))

    lines.append("=" * 72)
    lines.append(f"Overall: {'FAIL' if overall_fail else 'PASS'}")

    report_path = output_dir / "onset_suite_report.txt"
    report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return report_path


def _fixture_names(csv: str) -> list[str]:
    names = [part.strip() for part in csv.split(",") if part.strip()]
    for name in names:
        if name not in FIXTURES:
            raise ValueError(f"Unknown fixture '{name}'. Available: {', '.join(FIXTURES)}")
    return names


def _list_fixtures() -> int:
    for spec in FIXTURES.values():
        clip = spec.path if spec.path is not None else spec.source
        print(f"{spec.name:14s} {spec.duration_s:>5.1f}s  {clip}  {spec.description}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run onset-focused K1 hardware captures with fixed fixtures"
    )
    parser.add_argument("--serial", help="Serial port for the K1 device")
    parser.add_argument(
        "--fixtures",
        default="silence,click120,four4_128,sparse_ballad,dense_music",
        help="Comma-separated fixture names",
    )
    parser.add_argument("--tap", default="b", help="Capture tap (default: b)")
    parser.add_argument("--fps", type=int, default=30, help="Capture FPS (default: 30)")
    parser.add_argument("--format", default="v2", choices=["v1", "v2", "meta", "slim"],
                        help="Capture format passed to firmware (default: v2)")
    parser.add_argument("--output", default=None,
                        help="Output directory (default: /tmp/onset_capture_<timestamp>)")
    parser.add_argument("--playback-cmd", default=_default_playback_cmd(),
                        help="Playback command template, use {clip} for the WAV path")
    parser.add_argument("--no-playback", action="store_true",
                        help="Disable automatic playback and capture fixtures as-is")
    parser.add_argument("--playback-delay", type=float, default=4.0,
                        help="Delay before clip playback starts (default: 4s)")
    parser.add_argument("--tail", type=float, default=1.0,
                        help="Extra capture time after playback ends (default: 1s)")
    parser.add_argument("--settle", type=float, default=2.0,
                        help="Idle time between fixtures in seconds (default: 2s)")
    parser.add_argument("--list-fixtures", action="store_true",
                        help="List available fixtures and exit")
    args = parser.parse_args()

    if args.list_fixtures:
        return _list_fixtures()

    if not args.serial:
        parser.error("--serial is required unless --list-fixtures is used")

    if not args.no_playback and args.playback_cmd is None:
        parser.error("No playback command available. Use --playback-cmd or --no-playback.")

    fixture_names = _fixture_names(args.fixtures)

    if args.output:
        output_dir = Path(args.output)
    else:
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        output_dir = Path(f"/tmp/onset_capture_{ts}")
    output_dir.mkdir(parents=True, exist_ok=True)
    fixtures_dir = output_dir / "fixtures"
    fixtures_dir.mkdir(parents=True, exist_ok=True)

    runs: list[FixtureRun] = []
    for idx, name in enumerate(fixture_names):
        run = _run_fixture(FIXTURES[name], args, output_dir, fixtures_dir)
        runs.append(run)
        if idx < len(fixture_names) - 1 and args.settle > 0:
            time.sleep(args.settle)

    report_path = _write_summary(output_dir, runs)
    print(f"\nReport: {report_path}")

    if any(_fixture_failed(run) for run in runs):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
