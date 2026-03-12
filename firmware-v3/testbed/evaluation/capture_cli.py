#!/usr/bin/env python3
"""Serial capture CLI for K1 evaluation pipeline.

Connects to the K1 device over USB serial, sends 'capture stream' to start
binary frame streaming, saves raw bytes to a .bin file, then runs the
L0–L3 evaluation pipeline on the captured data.

Usage:
    # Capture 10 seconds at 15 FPS from tap B, then evaluate:
    python -m testbed.evaluation.capture_cli --duration 10 --fps 15 --tap b

    # Capture to a specific file without evaluation:
    python -m testbed.evaluation.capture_cli -d 30 -o captures/session01.bin --no-eval

    # Evaluate an existing capture file:
    python -m testbed.evaluation.capture_cli --evaluate captures/session01.bin

    # List available serial ports:
    python -m testbed.evaluation.capture_cli --list-ports

Requires:
    pip install pyserial numpy
"""

from __future__ import annotations

import argparse
import signal
import struct
import sys
import time
from datetime import datetime
from pathlib import Path
from typing import Optional

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("pyserial is required: pip install pyserial", file=sys.stderr)
    sys.exit(1)

# Support both direct script execution and module import.
# When run as `python capture_cli.py`, __package__ is None and relative
# imports fail.  Fix the path so absolute imports work either way.
import os as _os

_eval_dir = _os.path.dirname(_os.path.abspath(__file__))
_testbed_dir = _os.path.dirname(_eval_dir)
if _testbed_dir not in sys.path:
    sys.path.insert(0, _testbed_dir)

from evaluation.frame_parser import SYNC_BYTE, V2_FRAME_SIZE, V4_FRAME_SIZE, load_capture
from evaluation.pipeline import evaluate_sequence


# --- Constants ---

DEFAULT_PORT = "/dev/cu.usbmodem21401"
DEFAULT_BAUD = 115200
DEFAULT_FPS = 15
DEFAULT_DURATION = 10.0
DEFAULT_TAP = "b"
CAPTURES_DIR = Path("testbed/evaluation/captures")


def list_ports() -> None:
    """Print available serial ports."""
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("No serial ports found.")
        return
    for p in ports:
        print(f"  {p.device}  [{p.description}]")


def _drain_text(ser: serial.Serial, timeout: float = 0.5) -> list[str]:
    """Read text lines from serial until quiet for `timeout` seconds."""
    lines = []
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if ser.in_waiting:
            try:
                raw = ser.readline()
                line = raw.decode("utf-8", errors="replace").strip()
                if line:
                    lines.append(line)
                deadline = time.monotonic() + timeout  # reset on activity
            except Exception:
                break
        else:
            time.sleep(0.01)
    return lines


def capture(
    port: str = DEFAULT_PORT,
    baud: int = DEFAULT_BAUD,
    fps: int = DEFAULT_FPS,
    duration: float = DEFAULT_DURATION,
    tap: str = DEFAULT_TAP,
    output: Optional[Path] = None,
    version: int = 2,
) -> Path:
    """Capture binary frames from K1 and save to a .bin file.

    Args:
        port: Serial port path.
        baud: Baud rate.
        fps: Target frames per second (1–60).
        duration: Capture duration in seconds.
        tap: Capture tap point ('a', 'b', or 'c').
        output: Output file path. Auto-generated if None.
        version: Protocol version (2 or 4).

    Returns:
        Path to the saved .bin capture file.
    """
    if output is None:
        CAPTURES_DIR.mkdir(parents=True, exist_ok=True)
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        output = CAPTURES_DIR / f"capture_{ts}_tap{tap}_{fps}fps.bin"

    output.parent.mkdir(parents=True, exist_ok=True)
    frame_size = V2_FRAME_SIZE if version == 2 else V4_FRAME_SIZE

    # --- Signal handling for clean shutdown ---
    interrupted = [False]

    def _sigint(sig, frame):
        interrupted[0] = True

    old_handler = signal.signal(signal.SIGINT, _sigint)

    try:
        ser = serial.Serial(port, baud, timeout=0.1, write_timeout=1)
        ser.reset_input_buffer()
        ser.reset_output_buffer()
    except serial.SerialException as e:
        print(f"Cannot open {port}: {e}", file=sys.stderr)
        print("Available ports:", file=sys.stderr)
        list_ports()
        sys.exit(1)

    print(f"Connected to {port} @ {baud} baud", file=sys.stderr)

    # --- Flush any pending output ---
    _drain_text(ser, timeout=0.3)

    # --- Start capture stream ---
    cmd = f"capture stream {tap} {fps}\n"
    ser.write(cmd.encode("ascii"))
    print(f"Sent: {cmd.strip()}", file=sys.stderr)

    # Wait for firmware ack
    ack_lines = _drain_text(ser, timeout=1.0)
    for line in ack_lines:
        print(f"  K1> {line}", file=sys.stderr)

    # --- Capture binary frames ---
    print(
        f"Capturing tap {tap.upper()} @ {fps} FPS for {duration:.1f}s → {output}",
        file=sys.stderr,
    )

    start = time.monotonic()
    bytes_written = 0
    sync_count = 0
    last_report = start

    with open(output, "wb") as f:
        while not interrupted[0]:
            elapsed = time.monotonic() - start
            if elapsed >= duration:
                break

            # Read available bytes
            waiting = ser.in_waiting
            if waiting > 0:
                chunk = ser.read(min(waiting, 8192))
                f.write(chunk)
                bytes_written += len(chunk)

                # Count sync bytes for rough frame count
                sync_count += chunk.count(bytes([SYNC_BYTE]))
            else:
                time.sleep(0.001)

            # Progress report every 2 seconds
            now = time.monotonic()
            if now - last_report >= 2.0:
                elapsed_s = now - start
                rate = bytes_written / elapsed_s if elapsed_s > 0 else 0
                est_frames = bytes_written // frame_size
                print(
                    f"  {elapsed_s:.1f}s: {bytes_written:,} bytes "
                    f"(~{est_frames} frames, {rate/1024:.1f} KB/s)",
                    file=sys.stderr,
                )
                last_report = now

    capture_elapsed = time.monotonic() - start

    # --- Stop capture stream ---
    ser.write(b"capture stop\n")
    print("Sent: capture stop", file=sys.stderr)
    stop_lines = _drain_text(ser, timeout=1.0)
    for line in stop_lines:
        print(f"  K1> {line}", file=sys.stderr)

    ser.close()

    # --- Summary ---
    est_frames = bytes_written // frame_size
    actual_fps = est_frames / capture_elapsed if capture_elapsed > 0 else 0
    print(file=sys.stderr)
    print(f"Capture complete:", file=sys.stderr)
    print(f"  Duration:   {capture_elapsed:.1f}s", file=sys.stderr)
    print(f"  Bytes:      {bytes_written:,}", file=sys.stderr)
    print(f"  Est frames: {est_frames}", file=sys.stderr)
    print(f"  Actual FPS: {actual_fps:.1f}", file=sys.stderr)
    print(f"  Output:     {output}", file=sys.stderr)

    signal.signal(signal.SIGINT, old_handler)
    return output


def evaluate_file(path: Path, reference: Optional[Path] = None) -> None:
    """Load a capture file and run the full evaluation pipeline.

    Args:
        path: Path to .bin capture file.
        reference: Optional reference capture for L3 comparison.
    """
    print(f"Loading {path}...", file=sys.stderr)
    seq = load_capture(path)
    print(
        f"Loaded {seq.num_frames} frames "
        f"({seq.duration_s:.1f}s @ {seq.fps:.0f} FPS)",
        file=sys.stderr,
    )

    if seq.num_frames < 10:
        print(
            f"WARNING: Only {seq.num_frames} frames — metrics will be unreliable.",
            file=sys.stderr,
        )
        if seq.num_frames < 2:
            print("Cannot evaluate fewer than 2 frames.", file=sys.stderr)
            return

    ref_seq = None
    if reference is not None:
        print(f"Loading reference {reference}...", file=sys.stderr)
        ref_seq = load_capture(reference)
        print(f"Reference: {ref_seq.num_frames} frames", file=sys.stderr)

    print(file=sys.stderr)
    result = evaluate_sequence(seq, ref_seq)
    print(result.summary())


def main() -> None:
    parser = argparse.ArgumentParser(
        description="K1 serial capture and quality evaluation",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    parser.add_argument(
        "--port", "-p",
        default=DEFAULT_PORT,
        help=f"Serial port (default: {DEFAULT_PORT})",
    )
    parser.add_argument(
        "--baud", "-b",
        type=int,
        default=DEFAULT_BAUD,
        help=f"Baud rate (default: {DEFAULT_BAUD})",
    )
    parser.add_argument(
        "--fps", "-f",
        type=int,
        default=DEFAULT_FPS,
        help=f"Capture FPS, 1–60 (default: {DEFAULT_FPS})",
    )
    parser.add_argument(
        "--duration", "-d",
        type=float,
        default=DEFAULT_DURATION,
        help=f"Capture duration in seconds (default: {DEFAULT_DURATION})",
    )
    parser.add_argument(
        "--tap", "-t",
        choices=["a", "b", "c"],
        default=DEFAULT_TAP,
        help=f"Capture tap point (default: {DEFAULT_TAP})",
    )
    parser.add_argument(
        "--output", "-o",
        type=Path,
        default=None,
        help="Output .bin file (auto-generated if omitted)",
    )
    parser.add_argument(
        "--version", "-v",
        type=int,
        choices=[2, 4],
        default=2,
        help="Protocol version (default: 2)",
    )
    parser.add_argument(
        "--no-eval",
        action="store_true",
        help="Skip evaluation after capture",
    )
    parser.add_argument(
        "--evaluate", "-e",
        type=Path,
        default=None,
        metavar="FILE",
        help="Evaluate an existing capture file (skip capture)",
    )
    parser.add_argument(
        "--reference", "-r",
        type=Path,
        default=None,
        metavar="FILE",
        help="Reference capture file for L3 comparison",
    )
    parser.add_argument(
        "--list-ports",
        action="store_true",
        help="List available serial ports and exit",
    )

    args = parser.parse_args()

    if args.list_ports:
        list_ports()
        return

    # Evaluate-only mode
    if args.evaluate is not None:
        if not args.evaluate.exists():
            print(f"File not found: {args.evaluate}", file=sys.stderr)
            sys.exit(1)
        evaluate_file(args.evaluate, reference=args.reference)
        return

    # Capture (and optionally evaluate)
    output = capture(
        port=args.port,
        baud=args.baud,
        fps=args.fps,
        duration=args.duration,
        tap=args.tap,
        output=args.output,
        version=args.version,
    )

    if not args.no_eval:
        print(file=sys.stderr)
        evaluate_file(output, reference=args.reference)


if __name__ == "__main__":
    main()
