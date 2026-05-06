#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2025-2026 SpectraSynq
"""
Interactive C-1 microphone-domain envelope capture.

By default Captain controls the sound source and confirms each regime before
capture begins. With an explicit --audio-manifest plus --armed, the tool owns
local afplay start/stop timing so audio playback and serial capture start
together. The script captures MabuTrace JSON windows or compact [C1] serial
samples and writes a summary for the C-1 calibration-debt row.
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import math
import re
import subprocess
import sys
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any

try:
    import serial
except ImportError:
    sys.stderr.write(
        "ERROR: pyserial not installed. Use PlatformIO's Python or install pyserial.\n"
    )
    sys.exit(1)


START_MARKER = "[TRACE] Flushing trace buffer..."
END_MARKER = "[TRACE] Done."
C1_LINE_RE = re.compile(
    r"^\[C1\]\s+"
    r"t_us=(?P<t_us>\d+)\s+"
    r"raw=(?P<raw>[0-9.+-]+)\s+"
    r"frame=(?P<frame>[0-9.+-]+)\s+"
    r"conf=(?P<conf>[0-9.+-]+)\s+"
    r"sil=(?P<sil>[0-9.+-]+)\s+"
    r"silent=(?P<silent>[01])\s+"
    r"peak=(?P<peak>[0-9.+-]+)\s+"
    r"peakLast=(?P<peak_last>[0-9.+-]+)"
)

COUNTERS = {
    "rawHopRms": ("br_raw_rms", 1_000_000.0),
    "frameRms": ("audio_rms_x1000", 1_000.0),
    "audioConfidence": ("audio_confidence", 1_000.0),
    "silentScale": ("audio_silence_scale", 1_000.0),
    "isSilent": ("audio_is_silent", 1.0),
    "waveformPeakScaled": ("audio_waveform_peak_scaled", 1_000.0),
    "waveformPeakScaledLast": ("audio_waveform_peak_scaled_last", 1_000.0),
}

DEFAULT_REGIMES = (
    ("idle_room", "Leave the room/device quiet. No intentional music playback."),
    ("quiet_music", "Start quiet music at the lowest useful listening level."),
    ("normal_music", "Raise to normal listening level."),
    ("loud_music", "Raise to loud-but-safe listening level."),
    ("stop_playback", "Stop playback immediately after pressing Enter."),
)


@dataclass(frozen=True)
class PlaybackRegime:
    name: str
    capture_seconds: float
    path: Path | None = None
    hard_stop_after_seconds: float | None = None
    instruction: str = ""


def normalise_playback_manifest(manifest: dict[str, Any]) -> list[PlaybackRegime]:
    tracks = manifest.get("tracks")
    if not isinstance(tracks, list) or not tracks:
        raise ValueError("audio manifest requires a non-empty 'tracks' list")

    regimes: list[PlaybackRegime] = []
    for index, item in enumerate(tracks):
        if not isinstance(item, dict):
            raise ValueError(f"track {index} must be an object")

        name = item.get("name")
        if not isinstance(name, str) or not name.strip():
            raise ValueError(f"track {index} requires a non-empty name")

        capture_seconds = float(item.get("captureSeconds", 0.0))
        if capture_seconds <= 0.0:
            raise ValueError(f"track {name} requires captureSeconds > 0")

        path_value = item.get("path")
        path = Path(path_value) if isinstance(path_value, str) and path_value else None

        hard_stop_value = item.get("hardStopAfterSeconds")
        hard_stop_after_seconds = (
            float(hard_stop_value)
            if hard_stop_value is not None
            else None
        )
        if hard_stop_after_seconds is not None:
            if path is None:
                raise ValueError(f"track {name} cannot hard-stop without a path")
            if hard_stop_after_seconds <= 0.0:
                raise ValueError(f"track {name} hardStopAfterSeconds must be > 0")
            if hard_stop_after_seconds >= capture_seconds:
                raise ValueError(
                    f"track {name} hardStopAfterSeconds must be inside the capture window"
                )

        instruction = str(item.get("instruction", ""))
        regimes.append(PlaybackRegime(
            name=name.strip(),
            capture_seconds=capture_seconds,
            path=path,
            hard_stop_after_seconds=hard_stop_after_seconds,
            instruction=instruction,
        ))

    return regimes


def load_playback_manifest(path: Path) -> list[PlaybackRegime]:
    return normalise_playback_manifest(json.loads(path.read_text()))


def print_playback_plan(regimes: list[PlaybackRegime]) -> None:
    print("C-1 playback/capture plan:")
    for regime in regimes:
        if regime.path is None:
            source = "silence/no playback"
            stop = "no playback"
        else:
            source = str(regime.path)
            stop = (
                "full playback"
                if regime.hard_stop_after_seconds is None
                else f"hard stop at {regime.hard_stop_after_seconds:.3f}s"
            )
        print(
            f"  - {regime.name}: capture {regime.capture_seconds:.3f}s, "
            f"{stop}, source={source}"
        )


def send_line(port: serial.Serial, line: str) -> None:
    port.write((line + "\n").encode("utf-8"))
    port.flush()


def drain(port: serial.Serial, seconds: float = 0.3) -> None:
    deadline = time.time() + seconds
    while time.time() < deadline:
        port.read(4096)
        time.sleep(0.02)


def read_until_trace_done(port: serial.Serial, timeout: float) -> list[str]:
    lines: list[str] = []
    buf = bytearray()
    deadline = time.time() + timeout
    while time.time() < deadline:
        chunk = port.read(4096)
        if not chunk:
            time.sleep(0.01)
            continue
        buf.extend(chunk)
        while True:
            newline = buf.find(b"\n")
            if newline < 0:
                break
            line = bytes(buf[:newline]).decode("utf-8", errors="replace").rstrip("\r")
            buf = buf[newline + 1 :]
            lines.append(line)
            if line.strip() == END_MARKER:
                return lines
    if buf:
        lines.append(buf.decode("utf-8", errors="replace").rstrip("\r"))
    raise TimeoutError(f"did not receive {END_MARKER!r} within {timeout:.1f}s")


def extract_trace_json(lines: list[str]) -> dict[str, Any]:
    start = next((i for i, line in enumerate(lines) if line.strip() == START_MARKER), -1)
    end = next((i for i, line in enumerate(lines) if line.strip() == END_MARKER), -1)
    if start < 0 or end < 0 or end <= start:
        raise ValueError("trace markers missing or out of order")

    body_lines: list[str] = []
    for line in lines[start + 1 : end]:
        stripped = line.lstrip()
        if not stripped:
            body_lines.append(line)
            continue
        if stripped[0] in '{}[]"':
            body_lines.append(line)
    if not body_lines:
        raise ValueError("empty trace body")
    return json.loads("\n".join(body_lines))


def capture_one(port: serial.Serial, timeout: float) -> dict[str, Any]:
    drain(port, 0.15)
    send_line(port, "trace")
    lines = read_until_trace_done(port, timeout)
    return extract_trace_json(lines)


def parse_c1_line(line: str) -> dict[str, float | int] | None:
    match = C1_LINE_RE.match(line.strip())
    if match is None:
        return None
    groups = match.groupdict()
    return {
        "t_us": int(groups["t_us"]),
        "rawHopRms": float(groups["raw"]),
        "frameRms": float(groups["frame"]),
        "audioConfidence": float(groups["conf"]),
        "silentScale": float(groups["sil"]),
        "isSilent": int(groups["silent"]),
        "waveformPeakScaled": float(groups["peak"]),
        "waveformPeakScaledLast": float(groups["peak_last"]),
    }


def start_playback(path: Path) -> subprocess.Popen:
    if not path.exists():
        raise FileNotFoundError(f"audio path does not exist: {path}")
    return subprocess.Popen(
        ["afplay", str(path)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def stop_playback(proc: subprocess.Popen | None) -> None:
    if proc is None or proc.poll() is not None:
        return
    proc.terminate()
    try:
        proc.wait(timeout=2.0)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=2.0)


def capture_regime(
    port: serial.Serial,
    out_dir: Path,
    name: str,
    capture_seconds: float,
    interval_seconds: float,
    timeout_seconds: float,
) -> tuple[list[str], list[dict[str, Any]], dict[str, Any]]:
    regime_events: list[dict[str, Any]] = []
    regime_files: list[str] = []
    failed_captures: list[dict[str, Any]] = []
    deadline = time.time() + capture_seconds
    capture_index = 0
    while time.time() < deadline:
        capture_index += 1
        remaining = max(0.0, deadline - time.time())
        print(f"  capture {capture_index} ({remaining:.1f}s remaining)")
        try:
            trace = capture_one(port, timeout_seconds)
        except (TimeoutError, ValueError, json.JSONDecodeError) as exc:
            failed_captures.append({
                "captureIndex": capture_index,
                "error": str(exc),
            })
            print(f"  WARN: capture {capture_index} failed: {exc}")
            trace = None

        if trace is not None:
            trace_path = out_dir / f"{name}_{capture_index:02d}.json"
            trace_path.write_text(json.dumps(trace, separators=(",", ":")))
            regime_files.append(str(trace_path))
            regime_events.extend(trace.get("traceEvents", []))

        sleep_for = min(interval_seconds, max(0.0, deadline - time.time()))
        if sleep_for > 0:
            time.sleep(sleep_for)

    return regime_files, failed_captures, summarise(regime_events)


def percentile(sorted_values: list[float], q: float) -> float | None:
    if not sorted_values:
        return None
    idx = max(0, math.ceil(q * len(sorted_values)) - 1)
    return sorted_values[idx]


def stats(values: list[float]) -> dict[str, float | int | None]:
    values = sorted(values)
    if not values:
        return {
            "count": 0,
            "min": None,
            "p50": None,
            "p95": None,
            "p99": None,
            "max": None,
            "mean": None,
        }
    return {
        "count": len(values),
        "min": values[0],
        "p50": percentile(values, 0.50),
        "p95": percentile(values, 0.95),
        "p99": percentile(values, 0.99),
        "max": values[-1],
        "mean": sum(values) / len(values),
    }


def summarise(events: list[dict[str, Any]]) -> dict[str, Any]:
    by_metric: dict[str, list[float]] = {metric: [] for metric in COUNTERS}
    samples_by_metric: dict[str, list[tuple[int, float]]] = {metric: [] for metric in COUNTERS}
    seen: set[tuple[str, int, float]] = set()

    for event in events:
        if event.get("ph") != "C":
            continue
        name = event.get("name")
        ts = int(event.get("ts", 0))
        args = event.get("args") or {}
        if "value" not in args:
            continue
        value = float(args["value"])
        key = (str(name), ts, value)
        if key in seen:
            continue
        seen.add(key)
        for metric, (counter_name, scale) in COUNTERS.items():
            if name == counter_name:
                scaled = value / scale
                by_metric[metric].append(scaled)
                samples_by_metric[metric].append((ts, scaled))
                break

    out = {metric: stats(values) for metric, values in by_metric.items()}
    out["sampleCounts"] = {metric: len(values) for metric, values in by_metric.items()}

    if samples_by_metric["isSilent"] or samples_by_metric["silentScale"]:
        all_ts = [
            ts
            for samples in samples_by_metric.values()
            for ts, _ in samples
        ]
        t0 = min(all_ts) if all_ts else 0
        silent_true = [
            (ts - t0) / 1_000_000.0
            for ts, value in samples_by_metric["isSilent"]
            if value >= 1.0
        ]
        scale_below_02 = [
            (ts - t0) / 1_000_000.0
            for ts, value in samples_by_metric["silentScale"]
            if value < 0.2
        ]
        out["firstIsSilentSeconds"] = min(silent_true) if silent_true else None
        out["firstSilentScaleBelow0p2Seconds"] = min(scale_below_02) if scale_below_02 else None

    return out


def summarise_c1_samples(
    samples: list[dict[str, float | int]],
    hard_stop_after_seconds: float | None = None,
) -> dict[str, Any]:
    out: dict[str, Any] = {}
    for metric in (
        "rawHopRms",
        "frameRms",
        "audioConfidence",
        "silentScale",
        "isSilent",
        "waveformPeakScaled",
        "waveformPeakScaledLast",
    ):
        out[metric] = stats([float(sample[metric]) for sample in samples])
    out["sampleCounts"] = {metric: out[metric]["count"] for metric in out if isinstance(out[metric], dict)}

    if samples:
        t0 = int(samples[0]["t_us"])
        silent_true = [
            (int(sample["t_us"]) - t0) / 1_000_000.0
            for sample in samples
            if int(sample["isSilent"]) >= 1
        ]
        scale_below_02 = [
            (int(sample["t_us"]) - t0) / 1_000_000.0
            for sample in samples
            if float(sample["silentScale"]) < 0.2
        ]
        out["firstIsSilentSeconds"] = min(silent_true) if silent_true else None
        out["firstSilentScaleBelow0p2Seconds"] = min(scale_below_02) if scale_below_02 else None

        if hard_stop_after_seconds is not None:
            post_stop_samples = [
                sample
                for sample in samples
                if ((int(sample["t_us"]) - t0) / 1_000_000.0) >= hard_stop_after_seconds
            ]
            post_stop_silent_true = [
                ((int(sample["t_us"]) - t0) / 1_000_000.0) - hard_stop_after_seconds
                for sample in post_stop_samples
                if int(sample["isSilent"]) >= 1
            ]
            post_stop_scale_below_02 = [
                ((int(sample["t_us"]) - t0) / 1_000_000.0) - hard_stop_after_seconds
                for sample in post_stop_samples
                if float(sample["silentScale"]) < 0.2
            ]
            out["postStopSampleCount"] = len(post_stop_samples)
            out["postStopFirstIsSilentSeconds"] = (
                min(post_stop_silent_true) if post_stop_silent_true else None
            )
            out["postStopFirstSilentScaleBelow0p2Seconds"] = (
                min(post_stop_scale_below_02) if post_stop_scale_below_02 else None
            )

    return out


def collect_c1_lines(port: serial.Serial, capture_seconds: float) -> tuple[list[str], list[dict[str, float | int]]]:
    raw_lines: list[str] = []
    samples: list[dict[str, float | int]] = []
    deadline = time.time() + capture_seconds
    buffer = bytearray()

    while time.time() < deadline:
        chunk = port.read(1024)
        if not chunk:
            time.sleep(0.01)
            continue
        buffer.extend(chunk)
        while True:
            newline = buffer.find(b"\n")
            if newline < 0:
                break
            line = bytes(buffer[:newline]).decode("utf-8", errors="replace").rstrip("\r")
            buffer = buffer[newline + 1 :]
            raw_lines.append(line)
            sample = parse_c1_line(line)
            if sample is not None:
                samples.append(sample)

    if buffer:
        line = buffer.decode("utf-8", errors="replace").rstrip("\r")
        raw_lines.append(line)
        sample = parse_c1_line(line)
        if sample is not None:
            samples.append(sample)

    return raw_lines, samples


def capture_c1_serial_regime(
    port: serial.Serial,
    out_dir: Path,
    name: str,
    capture_seconds: float,
    hard_stop_after_seconds: float | None,
) -> tuple[str, int, dict[str, Any]]:
    print(f"  collecting [C1] serial lines for {capture_seconds:.1f}s")
    raw_lines, samples = collect_c1_lines(port, capture_seconds)
    lines_path = out_dir / f"{name}_c1_lines.txt"
    samples_path = out_dir / f"{name}_c1_samples.json"
    lines_path.write_text("\n".join(raw_lines) + ("\n" if raw_lines else ""))
    samples_path.write_text(json.dumps(samples, indent=2, sort_keys=True) + "\n")
    return (
        str(samples_path),
        len(raw_lines),
        summarise_c1_samples(samples, hard_stop_after_seconds),
    )


def write_markdown(path: Path, payload: dict[str, Any]) -> None:
    lines = [
        "# C-1 Microphone Envelope Capture",
        "",
        f"- Captured at: `{payload['capturedAt']}`",
        f"- Port: `{payload['port']}`",
        f"- Effect: `{payload['effect']}`",
        f"- Audio source: {payload['audioSource']}",
        "",
        "| Regime | rawHopRms p50/p95/max | frameRms p50/p95/max | confidence p50 | silentScale p50 | isSilent p50 | waveformPeak p50/p95/max |",
        "|---|---:|---:|---:|---:|---:|---:|",
    ]
    for regime in payload["regimes"]:
        s = regime["summary"]
        def fmt(metric: str, field: str) -> str:
            value = s.get(metric, {}).get(field)
            return "n/a" if value is None else f"{value:.6g}"
        lines.append(
            "| {name} | {rr50}/{rr95}/{rrmax} | {fr50}/{fr95}/{frmax} | "
            "{conf50} | {ss50} | {sil50} | {wp50}/{wp95}/{wpmax} |".format(
                name=regime["name"],
                rr50=fmt("rawHopRms", "p50"),
                rr95=fmt("rawHopRms", "p95"),
                rrmax=fmt("rawHopRms", "max"),
                fr50=fmt("frameRms", "p50"),
                fr95=fmt("frameRms", "p95"),
                frmax=fmt("frameRms", "max"),
                conf50=fmt("audioConfidence", "p50"),
                ss50=fmt("silentScale", "p50"),
                sil50=fmt("isSilent", "p50"),
                wp50=fmt("waveformPeakScaled", "p50"),
                wp95=fmt("waveformPeakScaled", "p95"),
                wpmax=fmt("waveformPeakScaled", "max"),
            )
        )
        if regime.get("failedCaptures"):
            lines.append(
                f"<!-- {regime['name']} failed capture attempts: {len(regime['failedCaptures'])} -->"
            )
    lines.append("")
    lines.append("## Stop-Playback Recovery")
    for regime in payload["regimes"]:
        if regime.get("hardStopAfterSeconds") is None:
            continue
        summary = regime["summary"]
        lines.append(
            f"- `{regime['name']}` hard stop: `{regime.get('hardStopAfterSeconds')}` seconds from first captured sample."
        )
        lines.append(
            f"- First `isSilent=true` after hard stop: `{summary.get('postStopFirstIsSilentSeconds')}` seconds."
        )
        lines.append(
            "- First `silentScale<0.2` after hard stop: "
            f"`{summary.get('postStopFirstSilentScaleBelow0p2Seconds')}` seconds."
        )
    path.write_text("\n".join(lines) + "\n")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/cu.usbmodem2101")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--effect", default="0x2102")
    parser.add_argument("--duration", type=float, default=30.0)
    parser.add_argument("--interval", type=float, default=4.0)
    parser.add_argument("--timeout", type=float, default=15.0)
    parser.add_argument("--out-dir", default="")
    parser.add_argument("--skip-effect", action="store_true")
    parser.add_argument(
        "--audio-manifest",
        default="",
        help="JSON manifest for process-owned afplay start/stop and serial capture.",
    )
    parser.add_argument(
        "--armed",
        action="store_true",
        help="Required with --audio-manifest before the script opens serial or plays audio.",
    )
    parser.add_argument(
        "--serial-c1-lines",
        action="store_true",
        help="Collect compact [C1] serial telemetry instead of issuing MabuTrace dumps.",
    )
    args = parser.parse_args(argv)

    ts = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    out_dir = Path(args.out_dir) if args.out_dir else Path("firmware-v3/tools/baselines") / f"c1_envelope_{ts}"
    playback_regimes: list[PlaybackRegime] | None = None

    if args.audio_manifest:
        playback_regimes = load_playback_manifest(Path(args.audio_manifest))
        print_playback_plan(playback_regimes)
        if not args.armed:
            print("Plan only. Re-run with --armed to open serial, play audio, and capture traces.")
            return 0

    out_dir.mkdir(parents=True, exist_ok=True)

    if not Path(args.port).exists():
        print(f"ERROR: port {args.port} does not exist", file=sys.stderr)
        return 1

    payload: dict[str, Any] = {
        "capturedAt": dt.datetime.now(dt.timezone.utc).isoformat(),
        "port": args.port,
        "effect": args.effect,
        "durationSeconds": args.duration,
        "intervalSeconds": args.interval,
        "audioSource": (
            f"process-owned afplay playback from {args.audio_manifest}"
            if playback_regimes is not None
            else "Captain-controlled external playback; script did not play audio."
        ),
        "captureMode": "serial_c1_lines" if args.serial_c1_lines else "mabutrace_json",
        "regimes": [],
    }

    if playback_regimes is None:
        print("C-1 capture will not play audio. Captain controls the source.")
    else:
        print("C-1 capture is ARMED: this process will play/stop manifest audio with afplay.")
    print(f"Output: {out_dir}")

    try:
        port = serial.Serial(
            port=args.port,
            baudrate=args.baud,
            timeout=0.2,
            write_timeout=2.0,
            exclusive=True,
        )
    except serial.SerialException as exc:
        print(f"ERROR: cannot open {args.port}: {exc}", file=sys.stderr)
        return 1

    with port:
        drain(port, 0.5)
        if args.effect and not args.skip_effect:
            print(f"Setting effect {args.effect}")
            send_line(port, f"effect {args.effect}")
            time.sleep(0.3)
            drain(port, 0.3)

        if playback_regimes is None:
            for name, instruction in DEFAULT_REGIMES:
                print()
                print(f"Regime: {name}")
                print(instruction)
                input("Press Enter when ready to begin this regime...")

                regime_files, failed_captures, summary = capture_regime(
                    port,
                    out_dir,
                    name,
                    args.duration,
                    args.interval,
                    args.timeout,
                )
                payload["regimes"].append({
                    "name": name,
                    "instruction": instruction,
                    "traceFiles": regime_files,
                    "failedCaptures": failed_captures,
                    "summary": summary,
                })
        else:
            for regime in playback_regimes:
                print()
                print(f"Regime: {regime.name}")
                if regime.instruction:
                    print(regime.instruction)
                proc: subprocess.Popen | None = None
                timer: threading.Timer | None = None
                try:
                    drain(port, 0.2)
                    if regime.path is not None:
                        print(f"  starting audio: {regime.path}")
                        proc = start_playback(regime.path)
                        if regime.hard_stop_after_seconds is not None:
                            timer = threading.Timer(
                                regime.hard_stop_after_seconds,
                                stop_playback,
                                args=(proc,),
                            )
                            timer.start()

                    if args.serial_c1_lines:
                        sample_path, raw_line_count, summary = capture_c1_serial_regime(
                            port,
                            out_dir,
                            regime.name,
                            regime.capture_seconds,
                            regime.hard_stop_after_seconds,
                        )
                        regime_files = [sample_path]
                        failed_captures = []
                    else:
                        regime_files, failed_captures, summary = capture_regime(
                            port,
                            out_dir,
                            regime.name,
                            regime.capture_seconds,
                            args.interval,
                            args.timeout,
                        )
                        raw_line_count = None
                finally:
                    if timer is not None:
                        timer.cancel()
                    stop_playback(proc)

                payload["regimes"].append({
                    "name": regime.name,
                    "instruction": regime.instruction,
                    "audioPath": str(regime.path) if regime.path is not None else None,
                    "hardStopAfterSeconds": regime.hard_stop_after_seconds,
                    "captureSeconds": regime.capture_seconds,
                    "traceFiles": regime_files,
                    "rawLineCount": raw_line_count,
                    "failedCaptures": failed_captures,
                    "summary": summary,
                })

    summary_json = out_dir / "c1_envelope_summary.json"
    summary_md = out_dir / "c1_envelope_summary.md"
    summary_json.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n")
    write_markdown(summary_md, payload)
    print(f"Wrote {summary_json}")
    print(f"Wrote {summary_md}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
