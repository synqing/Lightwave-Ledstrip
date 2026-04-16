#!/usr/bin/env python3
"""
Loaded 5-minute soak for K1 (serial + optional REST).

Serial: drives effect changes, hotkeys, periodic `s` status (FPS, drops, LED skips).
UART: scans for panic/WDT/RMT-style strings.

Optional REST (--k1-ip): POST /api/v1/effects/set and GET /api/v1/device/status when
the host can reach the K1 AP (FEATURE_API_AUTH=0 builds need no header).

Usage:
  python3 scripts/k1_loaded_soak.py --port /dev/cu.usbmodem1101 --duration 300
  python3 scripts/k1_loaded_soak.py --port /dev/cu.usbmodem1101 --k1-ip 192.168.4.1
"""

from __future__ import annotations

import argparse
import json
import re
import threading
import time
import urllib.error
import urllib.request

import serial

PANIC_RE = re.compile(
    r"Guru Meditation|abort\(\)|StoreProhibited|LoadProhibited|"
    r"IntegerDivideByZero|WDT|watchdog|Brownout|assert failed|"
    r"configASSERT|CORRUPT|RMT.*ERR|panic|tskKERNEL",
    re.I,
)

# One printStatus() renderer section (multiline)
STATUS_BLOCK_RE = re.compile(
    r"--- Renderer ---\s*\n"
    r"Effect:.*\n"
    r"Brightness:.*\n"
    r"Speed:.*\n"
    r"FPS:\s*(\d+).*?\n"
    r"CPU:.*\n"
    r"Frames:\s*\d+,\s*Drops:\s*(\d+)\s*\n"
    r"Frame time: avg=(\d+),\s*min=\d+,\s*max=\d+ us\s*\n"
    r"LED show: avg=(\d+),\s*max=\d+ us,\s*skips=(\d+)",
    re.DOTALL,
)

# K1 Waveform Hybrid + other heavy / audio-adjacent ids (hex)
EFFECT_ROTATION = [
    0x1313,  # EID_SB_K1_WAVEFORM_HYBRID
    0x1312,  # EID_SB_K1_WAVEFORM (parity)
    0x0A04,  # EID_AUDIO_WAVEFORM
    0x0A05,  # EID_AUDIO_BLOOM
    0x1314,  # EID_SB_K1_WAVEFORM_HARMONIC
]

HOTKEYS = b" +-[],.iIe`wW<>yY}"


def rest_worker(base: str, stop: threading.Event, stats: dict) -> None:
    base = base.rstrip("/")
    idx = 0
    while not stop.wait(0.35):
        try:
            if int(time.time() * 2) % 3 == 0:
                eid = EFFECT_ROTATION[idx % len(EFFECT_ROTATION)]
                idx += 1
                body = json.dumps({"effectId": eid}).encode()
                req = urllib.request.Request(
                    f"{base}/api/v1/effects/set",
                    data=body,
                    headers={"Content-Type": "application/json"},
                    method="POST",
                )
                with urllib.request.urlopen(req, timeout=2) as r:
                    r.read()
                stats["rest_posts"] = stats.get("rest_posts", 0) + 1
            else:
                req = urllib.request.Request(f"{base}/api/v1/device/status", method="GET")
                with urllib.request.urlopen(req, timeout=2) as r:
                    raw = r.read().decode()
                stats["rest_status"] = stats.get("rest_status", 0) + 1
                try:
                    doc = json.loads(raw)
                    d = doc.get("data") or doc
                    fps = d.get("fps")
                    if fps is not None:
                        stats["last_rest_fps"] = fps
                except json.JSONDecodeError:
                    pass
        except (urllib.error.URLError, OSError, TimeoutError):
            stats["rest_errors"] = stats.get("rest_errors", 0) + 1


def serial_stress(ser: serial.Serial, stop: threading.Event, stats: dict) -> None:
    ser.write(b"effect 0x1313\n")
    ser.flush()
    time.sleep(0.4)
    ei = 0
    hk = 0
    last_status = 0.0
    while not stop.is_set():
        now = time.time()
        if now - last_status >= 5.0:
            ser.write(b"s\n")
            ser.flush()
            last_status = now
            stats["status_polls"] = stats.get("status_polls", 0) + 1
        else:
            if int(now * 4) % 5 == 0:
                eid = EFFECT_ROTATION[ei % len(EFFECT_ROTATION)]
                ei += 1
                ser.write(f"effect 0x{eid:04X}\n".encode())
                ser.flush()
            else:
                ser.write(bytes([HOTKEYS[hk % len(HOTKEYS)]]))
                ser.flush()
                hk += 1
        time.sleep(0.12)


def extract_status_samples(buf: str, samples: list) -> str:
    """Consume complete renderer status blocks from buffer tail."""
    while True:
        m = STATUS_BLOCK_RE.search(buf)
        if not m:
            break
        rec = {
            "t": time.time(),
            "fps": int(m.group(1)),
            "drops": int(m.group(2)),
            "avg_frame_us": int(m.group(3)),
            "avg_show_us": int(m.group(4)),
            "skips": int(m.group(5)),
        }
        samples.append(rec)
        buf = buf[m.end() :]
    return buf


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/cu.usbmodem1101")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--duration", type=float, default=300.0)
    ap.add_argument("--k1-ip", default="", help="If set (e.g. 192.168.4.1), hammer REST in parallel")
    args = ap.parse_args()

    stats: dict = {}
    samples: list = []
    panic_lines: list = []
    buf = ""

    ser = serial.Serial(args.port, args.baud, timeout=0.2)
    ser.reset_input_buffer()

    stop = threading.Event()
    t_serial = threading.Thread(target=serial_stress, args=(ser, stop, stats), daemon=True)
    t_serial.start()

    t_rest = None
    if args.k1_ip:
        base = f"http://{args.k1_ip}"
        t_rest = threading.Thread(target=rest_worker, args=(base, stop, stats), daemon=True)
        t_rest.start()

    t0 = time.time()
    while time.time() - t0 < args.duration:
        chunk = ser.read(4096)
        if chunk:
            s = chunk.decode("utf-8", errors="replace")
            buf += s
            for line in s.splitlines():
                if PANIC_RE.search(line):
                    panic_lines.append(line[:400])
            buf = extract_status_samples(buf, samples)
            if len(buf) > 240000:
                buf = buf[-120000:]
        else:
            time.sleep(0.02)

    stop.set()
    t_serial.join(timeout=2.0)
    if t_rest:
        t_rest.join(timeout=2.0)
    ser.close()

    print("=== LOADED SOAK SUMMARY ===")
    print(f"port={args.port} duration_s={args.duration:.0f}")
    print(f"serial_stats={stats}")
    if samples:
        fps_vals = [x["fps"] for x in samples if "fps" in x]
        skip_vals = [x["skips"] for x in samples if "skips" in x]
        drop_vals = [x["drops"] for x in samples if "drops" in x]
        show_vals = [x["avg_show_us"] for x in samples if "avg_show_us" in x]
        frame_vals = [x["avg_frame_us"] for x in samples if "avg_frame_us" in x]
        if fps_vals:
            print(f"fps from status samples: min={min(fps_vals)} max={max(fps_vals)} n={len(fps_vals)}")
        if skip_vals:
            print(f"led_skips: min={min(skip_vals)} max={max(skip_vals)} last={skip_vals[-1]}")
        if drop_vals:
            print(f"frame_drops: min={min(drop_vals)} max={max(drop_vals)} last={drop_vals[-1]}")
        if show_vals:
            print(f"avg_show_us: min={min(show_vals)} max={max(show_vals)} last={show_vals[-1]}")
        if frame_vals:
            print(f"avg_frame_us: min={min(frame_vals)} max={max(frame_vals)} last={frame_vals[-1]}")
        print("last 6 status samples:", samples[-6:])
    else:
        print("WARN: no parsed status blocks (check serial line endings / buffer)")

    if panic_lines:
        print(f"FAIL: panic_pattern lines={len(panic_lines)}")
        for pl in panic_lines[:12]:
            print(pl)
    else:
        print("PASS: no panic-class UART strings")


if __name__ == "__main__":
    main()
