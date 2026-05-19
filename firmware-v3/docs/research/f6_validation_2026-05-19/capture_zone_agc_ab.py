#!/usr/bin/env python3
"""
F-6 Zone AGC bench A/B — single continuous capture.

One serial process. One log file. 80-second sequence:
  - Boot: verify bench toggle names exist (abort if they don't).
  - 0-32s ON window: 4 adbg spectrum snapshots (both gates ON, default).
  - 35-36s: toggle audio.zone_agc OFF + audio.chroma_zone_agc OFF.
  - 40-70s OFF window: 4 adbg spectrum snapshots.
  - 73-74s: restore both gates ON.
  - 76s: bench list to confirm final state.
  - try/finally restores gates ON unconditionally even on crash.

Discharges part 2 of Captain's D-revised F-6 attestation contract (2026-05-19).
Single-log requirement per Captain's accept-modify reply 2026-05-19.

Usage: python3 capture_zone_agc_ab.py [port] [log_path]
Defaults: /dev/tty.usbmodem1301  /tmp/f6_bench_ab.log

CRITICAL — port mapping verified 2026-05-19:
  K1v2 → /dev/tty.usbmodem1301
  K1v1 → /dev/tty.usbmodem1401  (DO NOT USE for F-6 — different hardware)

CRITICAL — case sensitivity:
  `bench` (lowercase) is consumed by the single-char `b` (RD Triangle K+) handler
  and never reaches the multi-char bench dispatcher. Use UPPERCASE `BENCH ...`.
  `adbg` and `ADBG` both work (verified 2026-05-19 on usbmodem1301).

Pre-flight:
  - Close any open serial monitor (pio device monitor, screen) — single ownership of port.
  - K1v2 must be flashed with a build that registers `audio.zone_agc` + `audio.chroma_zone_agc`
    bench toggles. Verified on usbmodem1301 2026-05-19 via `BENCH LIST`.
  - Audio playback device staged within K1v2 mic range, not yet playing.

The script will prompt: "START MUSIC, then press ENTER to begin 80s capture."
"""

import serial
import sys
import time


PORT = sys.argv[1] if len(sys.argv) > 1 else '/dev/tty.usbmodem1301'
LOG_PATH = sys.argv[2] if len(sys.argv) > 2 else '/tmp/f6_bench_ab.log'
BAUDRATE = 115200

# Bench toggle names verified against firmware-v3/src/utils/BenchRegistry.cpp:~117
# AND empirically against running K1v2 firmware 2026-05-19 via `BENCH LIST`.
ZONE_AGC_TOGGLE = 'audio.zone_agc'
CHROMA_ZONE_AGC_TOGGLE = 'audio.chroma_zone_agc'


def verify_toggles(ser):
    """Run `BENCH LIST` (uppercase — see file header) and verify expected toggle names exist."""
    print(f"[f6-capture] Verifying bench toggle names via BENCH LIST...")
    ser.reset_input_buffer()
    ser.write(b'\r\nBENCH LIST\r\n')
    time.sleep(2.0)  # let bench list output drain
    output = ser.read(16384).decode('ascii', errors='replace')
    print(output)

    if ZONE_AGC_TOGGLE not in output:
        print(f"[f6-capture] FATAL: bench toggle '{ZONE_AGC_TOGGLE}' not found in `BENCH LIST` output.")
        print(f"[f6-capture] Check BenchRegistry.cpp registration or K1v2 firmware build.")
        sys.exit(2)
    if CHROMA_ZONE_AGC_TOGGLE not in output:
        print(f"[f6-capture] FATAL: bench toggle '{CHROMA_ZONE_AGC_TOGGLE}' not found in `BENCH LIST` output.")
        sys.exit(2)
    print(f"[f6-capture] OK: both bench toggles registered.")


SEQUENCE = [
    # (t_seconds, command) — BENCH must be uppercase, adbg can be lowercase
    (3,  f'BENCH TOGGLE {ZONE_AGC_TOGGLE} on'),         # explicit ON (default but confirm)
    (4,  f'BENCH TOGGLE {CHROMA_ZONE_AGC_TOGGLE} on'),
    (8,  'adbg spectrum'),                              # ON snapshot 1
    (16, 'adbg spectrum'),                              # ON snapshot 2
    (24, 'adbg spectrum'),                              # ON snapshot 3
    (32, 'adbg spectrum'),                              # ON snapshot 4
    (35, f'BENCH TOGGLE {ZONE_AGC_TOGGLE} off'),        # → OFF
    (36, f'BENCH TOGGLE {CHROMA_ZONE_AGC_TOGGLE} off'),
    (43, 'adbg spectrum'),                              # OFF snapshot 1
    (51, 'adbg spectrum'),                              # OFF snapshot 2
    (59, 'adbg spectrum'),                              # OFF snapshot 3
    (67, 'adbg spectrum'),                              # OFF snapshot 4
    (73, f'BENCH TOGGLE {ZONE_AGC_TOGGLE} on'),         # restore default ON
    (74, f'BENCH TOGGLE {CHROMA_ZONE_AGC_TOGGLE} on'),
    (76, 'BENCH LIST'),                                 # confirm final state
]
TOTAL_DURATION = 80


def main():
    print(f"[f6-capture] Port: {PORT}")
    print(f"[f6-capture] Log:  {LOG_PATH}")
    print(f"[f6-capture] Bench toggles: {ZONE_AGC_TOGGLE}, {CHROMA_ZONE_AGC_TOGGLE}")
    print()

    ser = serial.Serial(PORT, BAUDRATE, timeout=0.1)

    try:
        verify_toggles(ser)

        print()
        print("[f6-capture] Ready. Approved corpus track:")
        print("[f6-capture]   /Users/spectrasynq/Workspace_Management/Software/hybrid-beat-tracker/tests/benchmark/clips/James_Brown_-_Papa_s_Got_A_Brand_New_Bag.wav")
        print()
        input("[f6-capture] START MUSIC, then press ENTER to begin 80s capture...")

        # Drain any accumulated periodic logs (audio debug, etc.) that piled up while waiting
        ser.reset_input_buffer()

        start = time.time()
        deadline = start + TOTAL_DURATION
        injected = set()

        with open(LOG_PATH, 'wb') as f:
            f.write(b"# F-6 Zone AGC bench A/B continuous capture\n")
            f.write(f"# Date:     {time.strftime('%Y-%m-%d %H:%M:%S')}\n".encode())
            f.write(f"# Port:     {PORT}\n".encode())
            f.write(f"# Duration: {TOTAL_DURATION}s\n".encode())
            f.write(b"# Sequence:\n")
            for t, cmd in SEQUENCE:
                f.write(f"#   t={t:>3}s  {cmd}\n".encode())
            f.write(b"#\n")
            f.flush()

            while time.time() < deadline:
                elapsed = time.time() - start
                for i, (t, cmd) in enumerate(SEQUENCE):
                    if i not in injected and elapsed >= t:
                        injected.add(i)
                        ser.write((cmd + '\r\n').encode())
                        marker = f"\n--- INJECT t={elapsed:.2f}s [{i+1}/{len(SEQUENCE)}] cmd={cmd} ---\n"
                        f.write(marker.encode())
                        f.flush()
                        print(f"  t={elapsed:5.1f}s  →  {cmd}")

                data = ser.read(4096)
                if data:
                    f.write(data)
                    f.flush()

            f.write(f"\n# Capture complete @ {time.strftime('%Y-%m-%d %H:%M:%S')}\n".encode())

        print(f"\n[f6-capture] Capture complete.")
    finally:
        # UNCONDITIONAL restore — runs even on crash, signal, or KeyboardInterrupt
        print("[f6-capture] Restoring both gates ON...")
        for cmd in [f'BENCH TOGGLE {ZONE_AGC_TOGGLE} on', f'BENCH TOGGLE {CHROMA_ZONE_AGC_TOGGLE} on']:
            try:
                ser.write((cmd + '\r\n').encode())
                time.sleep(0.5)
            except Exception as e:
                print(f"[f6-capture] WARN: restore command '{cmd}' failed: {e}")
        try:
            ser.close()
        except Exception:
            pass

    print(f"\n[f6-capture] Done. Log: {LOG_PATH}")
    print(f"[f6-capture] STOP MUSIC NOW.")
    print(f"[f6-capture] Next: agent parses log and assesses ON/OFF bands[] differential.")


if __name__ == '__main__':
    main()
