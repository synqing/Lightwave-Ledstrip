#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2025-2026 SpectraSynq
"""
capture_trace.py — extract a MabuTrace JSON dump from a running K1 over serial,
strip the wrapper lines, validate the JSON, and (optionally) open it in
Perfetto UI.

Firmware must be built with FEATURE_MABUTRACE=1 (e.g. env
`esp32dev_audio_esv11_k1v2_32khz_trace`). In a non-trace build the `trace`
serial command does nothing — see firmware-v3/docs/debugging/MABUTRACE_GUIDE.md.

Wire protocol (verified from src/serial/SerialCLI.cpp:1174-1183):
    >> trace\n
    << [TRACE] Flushing trace buffer...
    << <raw Chrome Trace Format JSON>
    << [TRACE] Done.

This tool reads everything between the two `[TRACE]` markers, validates the
JSON, and writes it to a file ready for `https://ui.perfetto.dev` (or any
Chrome Trace Format viewer).

Usage:
    python3 firmware-v3/tools/capture_trace.py \
        --port /dev/tty.usbmodem2101 \
        --output /tmp/k1_trace.json \
        --effect 0x2102 --soak 10 --open

Options:
    --port      Serial port. Default: /dev/tty.usbmodem2101.
    --baud      Baud rate. Default: 115200.
    --output    Output JSON path. Default: /tmp/k1_trace_YYYYMMDD_HHMMSS.json.
    --effect    Switch to this effect ID before tracing (e.g. 0x2100).
    --soak      Seconds to let the effect run before sending `trace`. Default: 5.
    --timeout   Per-read timeout (seconds) while collecting trace. Default: 10.
    --open      After capture, open Perfetto UI in the default browser.
    --quiet     Suppress per-line capture status.

Exit codes:
    0  success — JSON saved
    1  serial port unavailable / contention
    2  trace command issued but no `[TRACE] Done.` received within timeout
    3  JSON parse failure (firmware emitted truncated or malformed output)
    4  firmware does not appear to be a *_trace build (no markers seen)
"""

from __future__ import annotations

import argparse
import datetime as _dt
import json
import os
import sys
import time
import webbrowser
from pathlib import Path

try:
    import serial  # pyserial
except ImportError:
    sys.stderr.write(
        "ERROR: pyserial not installed. Install with `pip install pyserial`,\n"
        "or use PlatformIO's bundled python: ~/.platformio/penv/bin/python3 -m pip install pyserial\n"
    )
    sys.exit(1)


# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

START_MARKER = "[TRACE] Flushing trace buffer..."
END_MARKER = "[TRACE] Done."
PERFETTO_URL = "https://ui.perfetto.dev"


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _ts_filename() -> str:
    return _dt.datetime.now().strftime("k1_trace_%Y%m%d_%H%M%S.json")


def _drain(port: serial.Serial, settle_seconds: float = 0.5) -> None:
    """Read and discard any pending bytes from the serial port."""
    deadline = time.time() + settle_seconds
    while time.time() < deadline:
        chunk = port.read(4096)
        if not chunk:
            time.sleep(0.05)


def _send(port: serial.Serial, line: str) -> None:
    port.write((line + "\n").encode("utf-8"))
    port.flush()


def _read_until(
    port: serial.Serial,
    end_marker: str,
    *,
    overall_timeout: float,
    quiet: bool,
) -> tuple[list[str], bool]:
    """Read whole lines until end_marker is seen or overall timeout elapses.

    Returns (lines, found_end_marker). Each line has no trailing newline.
    """
    lines: list[str] = []
    buf = bytearray()
    deadline = time.time() + overall_timeout
    while time.time() < deadline:
        chunk = port.read(4096)
        if not chunk:
            time.sleep(0.01)
            continue
        buf.extend(chunk)
        # Walk through whole lines.
        while True:
            nl = buf.find(b"\n")
            if nl < 0:
                break
            raw_line = bytes(buf[:nl]).decode("utf-8", errors="replace").rstrip("\r")
            buf = buf[nl + 1 :]
            lines.append(raw_line)
            if not quiet:
                preview = raw_line if len(raw_line) <= 120 else raw_line[:117] + "..."
                print(f"  rx | {preview}", file=sys.stderr)
            if raw_line.strip() == end_marker:
                return lines, True
    # Final flush of any partial line.
    if buf:
        lines.append(buf.decode("utf-8", errors="replace").rstrip("\r"))
    return lines, False


def _extract_json(lines: list[str]) -> tuple[str, str]:
    """Pull the JSON body out of a captured stream.

    Returns (json_text, error_or_empty). Looks for START_MARKER and END_MARKER
    and returns everything in between, joined with newlines and stripped.
    """
    start_idx = next(
        (i for i, ln in enumerate(lines) if ln.strip() == START_MARKER), -1
    )
    end_idx = next(
        (i for i, ln in enumerate(lines) if ln.strip() == END_MARKER), -1
    )

    if start_idx < 0 and end_idx < 0:
        return "", (
            "Neither start nor end MabuTrace marker found in serial output. "
            "The firmware may not be a *_trace build (FEATURE_MABUTRACE off), "
            "or the `trace` command was not received."
        )
    if start_idx < 0:
        return "", "Start marker '[TRACE] Flushing trace buffer...' not seen."
    if end_idx < 0:
        return "", (
            "End marker '[TRACE] Done.' not seen — output truncated. "
            "Increase --timeout, or the on-chip 64 KB ring overflowed mid-flush."
        )
    if end_idx <= start_idx:
        return "", "End marker preceded start marker (interleaved output?)."

    # Filter out log-subsystem lines that interleave with trace output
    # (e.g. `[901581][1;32m[INFO][0m[WiFi] AP Mode...`). Trace JSON lines
    # only ever start with whitespace, `{`, `[`, `]`, `}`, or `"`.
    raw = lines[start_idx + 1 : end_idx]
    cleaned: list[str] = []
    for ln in raw:
        stripped = ln.lstrip()
        if not stripped:
            cleaned.append(ln)
            continue
        first = stripped[0]
        if first in '{}[]"':
            cleaned.append(ln)
            continue
        # Drop lines that are clearly log output (timestamps, ANSI, [INFO]).
        if "\x1b[" in ln or "[INFO]" in ln or "[WARN]" in ln or "[ERROR]" in ln:
            continue
        # Anything else: also drop, but keep a record for diagnostics.
        # (silent drop — this branch only fires on unexpected noise)

    body = "\n".join(cleaned).strip()
    if not body:
        return "", "Trace body between markers was empty."
    return body, ""


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description="Capture a MabuTrace JSON dump from a running K1 over serial."
    )
    parser.add_argument("--port", default="/dev/tty.usbmodem2101")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument(
        "--output",
        default=str(Path("/tmp") / _ts_filename()),
        help="Output JSON file path.",
    )
    parser.add_argument(
        "--effect",
        default=None,
        help="Switch to this effect ID before tracing (e.g. 0x2102).",
    )
    parser.add_argument(
        "--soak",
        type=float,
        default=5.0,
        help="Seconds to let the effect run before sending `trace`.",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=10.0,
        help="Maximum seconds to wait for [TRACE] Done. after sending `trace`.",
    )
    parser.add_argument("--open", action="store_true", help="Open Perfetto UI.")
    parser.add_argument("--quiet", action="store_true", help="Suppress live capture log.")
    args = parser.parse_args(argv)

    if not Path(args.port).exists():
        print(f"ERROR: port {args.port} does not exist.", file=sys.stderr)
        return 1

    print(f"-- Opening {args.port} @ {args.baud} baud", file=sys.stderr)
    try:
        port = serial.Serial(
            port=args.port,
            baudrate=args.baud,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.2,
            write_timeout=2.0,
            exclusive=True,
        )
    except serial.SerialException as exc:
        # Identify the holder so the user can close exactly the right thing.
        holder_hint = ""
        try:
            import subprocess

            out = subprocess.run(
                ["lsof", args.port],
                capture_output=True,
                text=True,
                timeout=2,
            ).stdout.strip().splitlines()
            if len(out) >= 2:
                # lsof first row is the header; subsequent rows are holders.
                cmd = out[1].split(None, 1)[0]
                holder_hint = (
                    f"\n  Port is currently held by `{cmd}`. "
                    f"Close any open serial monitor in {cmd} "
                    f"(or run `lsof {args.port}` for full details), then retry."
                )
        except Exception:  # noqa: BLE001
            pass
        print(
            f"ERROR: cannot open {args.port}: {exc}{holder_hint}\n"
            "  Two readers cannot coexist on a single TTY; this script needs "
            "exclusive access.",
            file=sys.stderr,
        )
        return 1

    with port:
        # Discard whatever is in the rx buffer (boot banner, prior output).
        _drain(port, settle_seconds=0.5)

        # Optional: switch effect first.
        if args.effect:
            cmd = f"effect {args.effect}"
            print(f"-- {cmd}", file=sys.stderr)
            _send(port, cmd)
            # Echo + ack tends to land within ~50 ms; don't gate on it.
            time.sleep(0.2)

        # Soak: let the effect run so the trace ring captures meaningful data.
        if args.soak > 0:
            print(f"-- Soaking {args.soak:.1f}s before trace dump...", file=sys.stderr)
            # Drain output during soak so the rx buffer doesn't fill.
            soak_deadline = time.time() + args.soak
            while time.time() < soak_deadline:
                port.read(4096)
                time.sleep(0.05)

        # Drain anything that arrived during soak's last read window.
        _drain(port, settle_seconds=0.2)

        # Issue the trace dump command.
        print("-- Sending `trace`...", file=sys.stderr)
        _send(port, "trace")

        lines, ok = _read_until(
            port,
            END_MARKER,
            overall_timeout=args.timeout,
            quiet=args.quiet,
        )
        if not ok:
            print(
                "ERROR: did not see '[TRACE] Done.' within "
                f"{args.timeout:.1f}s. Captured {len(lines)} lines.",
                file=sys.stderr,
            )
            # Still try to extract — maybe the marker is just missing newline.
            body, err = _extract_json(lines)
            if not body:
                print(f"ERROR: {err}", file=sys.stderr)
                return 2

    # Extract + validate.
    body, err = _extract_json(lines)
    if not body:
        print(f"ERROR: {err}", file=sys.stderr)
        return 4 if "FEATURE_MABUTRACE" in err else 3

    try:
        parsed = json.loads(body)
    except json.JSONDecodeError as exc:
        print(
            f"ERROR: captured trace body is not valid JSON: {exc}\n"
            "First 200 chars: " + body[:200],
            file=sys.stderr,
        )
        # Save the raw body anyway for forensic inspection.
        raw_path = Path(args.output).with_suffix(".raw.txt")
        raw_path.write_text(body)
        print(f"Saved raw body to {raw_path}", file=sys.stderr)
        return 3

    # Save canonical JSON (re-serialise to normalise whitespace).
    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(parsed, separators=(",", ":")))
    n_events = len(parsed.get("traceEvents", []))
    size_kb = out_path.stat().st_size / 1024.0
    print(
        f"-- Saved {n_events} trace events to {out_path} ({size_kb:.1f} KB)",
        file=sys.stderr,
    )

    if args.open:
        # Perfetto UI is a static web app — open it; user drags JSON in.
        # Future: use Perfetto's open_trace_in_ui helper if pyperfetto is present.
        print(f"-- Opening {PERFETTO_URL} (drop the JSON onto the page)", file=sys.stderr)
        try:
            webbrowser.open(PERFETTO_URL)
        except Exception as exc:  # noqa: BLE001
            print(f"WARN: webbrowser.open failed: {exc}", file=sys.stderr)

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
