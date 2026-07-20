#!/usr/bin/env python3
"""Reusable MAC-verified serial capture for the K1 mic A/B (READ-ONLY, no flash).

Captures the DUT's [AP]/TEMPO telemetry to a labelled log for a fixed duration. Enforces
MAC-verify-before-interaction (hard rule) by matching the USB serial-number descriptor to the
expected MAC and REFUSING if it does not match — you cannot capture the wrong board by accident.

Usage:
  python3 capture_serial.py --unit bench --seconds 60 --out captures/p4_bench_trial01.log
  python3 capture_serial.py --unit main  --seconds 60 --out captures/p4_main_trial01.log
Then score with mic_ab_scorer (parse_frames + the P4/P1/P7 metric functions).
"""
import argparse, os, sys, time
import serial
from serial.tools import list_ports

UNITS = {
    "bench": {"mac": "B4:3A:45:A5:89:B4", "mic": "IM73D122"},
    "main":  {"mac": "B4:3A:45:A5:87:F8", "mic": "SPH0645"},
}

def find_port(mac: str):
    for p in list_ports.comports():
        sn = (p.serial_number or "").upper().replace("-", ":")
        if sn == mac.upper():
            return p.device
    return None

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--unit", choices=UNITS, required=True)
    ap.add_argument("--seconds", type=float, default=60.0)
    ap.add_argument("--out", required=True)
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()
    u = UNITS[args.unit]
    port = find_port(u["mac"])
    if not port:
        sys.exit(f"REFUSED (MAC-verify-before-interaction): no connected device with MAC "
                 f"{u['mac']} for unit '{args.unit}' ({u['mic']}). Check the cable / power.")
    print(f"MAC-VERIFIED: {args.unit} ({u['mic']}) @ {port}  SER={u['mac']}")
    os.makedirs(os.path.dirname(os.path.abspath(args.out)) or ".", exist_ok=True)
    ser = serial.Serial(port, args.baud, timeout=0.3)
    n_ap = n_tempo = 0
    t0 = time.time()
    with open(args.out, "w") as f:
        f.write(f"# unit={args.unit} mic={u['mic']} mac={u['mac']} port={port} "
                f"start={time.strftime('%Y-%m-%dT%H:%M:%S')} seconds={args.seconds}\n")
        while time.time() - t0 < args.seconds:
            ln = ser.readline().decode("utf-8", "replace").rstrip()
            if not ln:
                continue
            f.write(ln + "\n")
            if ln.startswith("TEMPO,"):
                n_tempo += 1
            elif ln.startswith("[AP]"):
                n_ap += 1
    ser.close()
    print(f"captured {args.seconds:.0f}s -> {args.out}  ([AP]={n_ap}, TEMPO={n_tempo})")
    if n_tempo == 0:
        print("NOTE: 0 TEMPO frames — this is the shipping build (~1.4 Hz [AP] only, gives "
              "BPM/lock/conf). For beat-F-measure, flash the ENABLE_TEMPO_STREAM composite build.")

if __name__ == "__main__":
    main()
