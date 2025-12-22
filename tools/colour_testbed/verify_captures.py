#!/usr/bin/env python3
"""
Verify capture folders produced by tools/colour_testbed/capture_test.py.

Checks:
- File size is 977 bytes
- Header magic/version/tap_id/frame_len are sane
- Header effect_id matches folder name effect_<id>
"""

from __future__ import annotations

import argparse
import os
import struct
from typing import List, Tuple


HEADER_FMT = "<BBBBBBBIIH"  # 17 bytes
HEADER_SIZE = 17
EXPECTED_TOTAL = 977
EXPECTED_FRAME_LEN = 960
EXPECTED_MAGIC = 0xFD
EXPECTED_VERSION = 0x01


def _parse_header(p: str):
    with open(p, "rb") as f:
        hdr = f.read(HEADER_SIZE)
    if len(hdr) != HEADER_SIZE:
        raise ValueError(f"short header: {len(hdr)}")
    h = struct.unpack(HEADER_FMT, hdr)
    return {
        "magic": h[0],
        "version": h[1],
        "tap_id": h[2],
        "effect_id": h[3],
        "palette_id": h[4],
        "brightness": h[5],
        "speed": h[6],
        "frame_index": h[7],
        "timestamp_us": h[8],
        "frame_len": h[9],
    }


def verify(root: str) -> Tuple[int, List[str]]:
    errors: List[str] = []
    checked = 0

    for d in sorted(os.listdir(root)):
        if not d.startswith("effect_"):
            continue
        try:
            expected_effect = int(d.split("_", 1)[1])
        except Exception:
            errors.append(f"{d}: invalid folder name")
            continue

        dp = os.path.join(root, d)
        if not os.path.isdir(dp):
            continue

        for fn in sorted(os.listdir(dp)):
            if not fn.endswith(".bin"):
                continue
            p = os.path.join(dp, fn)
            checked += 1

            sz = os.path.getsize(p)
            if sz != EXPECTED_TOTAL:
                errors.append(f"{p}: size {sz} (expected {EXPECTED_TOTAL})")
                continue

            try:
                meta = _parse_header(p)
            except Exception as e:
                errors.append(f"{p}: header parse failed: {e}")
                continue

            if meta["magic"] != EXPECTED_MAGIC:
                errors.append(f"{p}: magic 0x{meta['magic']:02X} (expected 0x{EXPECTED_MAGIC:02X})")
            if meta["version"] != EXPECTED_VERSION:
                errors.append(f"{p}: version {meta['version']} (expected {EXPECTED_VERSION})")
            if meta["tap_id"] not in (0, 1, 2):
                errors.append(f"{p}: tap_id {meta['tap_id']} (expected 0/1/2)")
            if meta["frame_len"] != EXPECTED_FRAME_LEN:
                errors.append(f"{p}: frame_len {meta['frame_len']} (expected {EXPECTED_FRAME_LEN})")
            if meta["effect_id"] != expected_effect:
                errors.append(
                    f"{p}: effect_id {meta['effect_id']} (expected {expected_effect})"
                )

    return checked, errors


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True, help="Capture folder root (e.g. captures/candidate_gamut2)")
    args = ap.parse_args()

    checked, errors = verify(args.root)
    print(f"Checked {checked} .bin files under {args.root}")
    if errors:
        print(f"FAIL: {len(errors)} issues")
        for e in errors[:50]:
            print(f"- {e}")
        raise SystemExit(2)
    print("PASS: all headers and sizes look correct")


if __name__ == "__main__":
    main()


