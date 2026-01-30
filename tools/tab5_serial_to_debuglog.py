#!/usr/bin/env python3
"""
Tab5 encoder debug helper.

Reads Tab5 serial output, extracts JSON lines emitted by the firmware timing probes,
and appends them to `.cursor/debug.log` as NDJSON (one JSON object per line).

This is intentionally tiny: it’s only for debug-mode evidence collection.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
from typing import Optional


DEFAULT_BAUD = 115200
DEFAULT_LOG_PATH = os.path.join(os.path.dirname(os.path.dirname(__file__)), ".cursor", "debug.log")


def _now_ms() -> int:
    return int(time.time() * 1000)


def _try_parse_json_line(line: str) -> Optional[dict]:
    line = line.strip()
    if not line.startswith("{") or not line.endswith("}"):
        return None
    try:
        obj = json.loads(line)
    except Exception:
        return None
    if not isinstance(obj, dict):
        return None
    # Only accept our debug-session payloads.
    if obj.get("sessionId") != "debug-session":
        return None
    # Ensure basic fields exist (don’t crash if missing).
    obj.setdefault("timestamp", _now_ms())
    return obj


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True, help="Serial port (e.g. /dev/cu.usbmodemXXXX)")
    ap.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    ap.add_argument("--log-path", default=DEFAULT_LOG_PATH)
    ap.add_argument(
        "--passthrough",
        action="store_true",
        help="Print non-JSON serial lines to stdout",
    )
    args = ap.parse_args()

    try:
        import serial  # type: ignore
    except Exception:
        print("Missing dependency: pyserial", file=sys.stderr)
        print("Install with: pip3 install pyserial", file=sys.stderr)
        return 2

    # Open serial.
    ser = serial.Serial(args.port, args.baud, timeout=0.5)
    print(f"[tab5_serial_to_debuglog] Reading {args.port} @ {args.baud}")
    print(f"[tab5_serial_to_debuglog] Writing NDJSON to {args.log_path}")

    os.makedirs(os.path.dirname(args.log_path), exist_ok=True)

    try:
        with open(args.log_path, "a", encoding="utf-8") as f:
            while True:
                raw = ser.readline()
                if not raw:
                    continue
                try:
                    line = raw.decode("utf-8", errors="replace").strip()
                except Exception:
                    continue

                obj = _try_parse_json_line(line)
                if obj is None:
                    if args.passthrough:
                        print(line)
                    continue

                f.write(json.dumps(obj, separators=(",", ":")) + "\n")
                f.flush()
                # Minimal local hint that we captured something.
                if args.passthrough:
                    msg = obj.get("message", "event")
                    loc = obj.get("location", "?")
                    print(f"[captured] {msg} ({loc})")
    except KeyboardInterrupt:
        print("\n[tab5_serial_to_debuglog] Stopped.")
        return 0
    finally:
        try:
            ser.close()
        except Exception:
            pass


if __name__ == "__main__":
    raise SystemExit(main())

