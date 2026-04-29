#!/usr/bin/env python3
"""Compare two GFRM v2 binary captures and emit a JSON + markdown report.

Usage:
    golden_compare.py <reference.gfrm> <test.gfrm> [--report PATH] [--threshold-pass N] [--threshold-warn N]

Exit codes:
    0  pass — L2 mean below pass threshold
    1  warn — L2 mean between pass and warn thresholds
    2  fail — L2 mean above warn threshold
    3  format error — header mismatch / unreadable

GFRM v2 layout (firmware-v3/test/test_golden/golden_runner.cpp header doc).
"""
import argparse
import json
import math
import struct
import sys
from pathlib import Path

HEADER_FMT = "<4sBBHBBH"  # magic, version, mode, ledsPerStrip, stripCount, rsv, frameCount
HEADER_SIZE = struct.calcsize(HEADER_FMT)
FRAME_META_FMT = "<IIB3x"  # frameIndex, totalTimeMs, beatFired, 3 pad
FRAME_META_SIZE = struct.calcsize(FRAME_META_FMT)


def parse_gfrm(path: Path):
    """Parse a GFRM v2 file. Returns (header_dict, frames) where each frame is
    a dict with 'meta' and a flat list of (r,g,b) tuples for strip1+strip2."""
    data = path.read_bytes()
    if len(data) < HEADER_SIZE:
        raise ValueError(f"{path}: file too small ({len(data)} bytes)")
    magic, version, mode, lps, sc, _rsv, fc = struct.unpack_from(HEADER_FMT, data, 0)
    if magic != b"GFRM":
        raise ValueError(f"{path}: bad magic {magic!r}")
    if version != 2:
        raise ValueError(f"{path}: unsupported GFRM version {version}")
    pixel_bytes = lps * sc * 3
    frame_size = FRAME_META_SIZE + pixel_bytes
    expected = HEADER_SIZE + fc * frame_size
    if len(data) != expected:
        raise ValueError(
            f"{path}: size mismatch — got {len(data)}, expected {expected} "
            f"(header {HEADER_SIZE} + {fc} frames × {frame_size})"
        )
    frames = []
    for f in range(fc):
        off = HEADER_SIZE + f * frame_size
        idx, totalMs, beat = struct.unpack_from(FRAME_META_FMT, data, off)[:3]
        pixels_off = off + FRAME_META_SIZE
        pixels = data[pixels_off : pixels_off + pixel_bytes]
        frames.append({
            "index": idx,
            "totalTimeMs": totalMs,
            "beat": beat,
            "pixels": pixels,
        })
    return {"version": version, "mode": mode, "ledsPerStrip": lps, "stripCount": sc, "frameCount": fc}, frames


def per_pixel_l2(ref_pixels: bytes, test_pixels: bytes) -> float:
    """RMS Euclidean distance per pixel across all strips/frames."""
    if len(ref_pixels) != len(test_pixels):
        return float("inf")
    n_pixels = len(ref_pixels) // 3
    if n_pixels == 0:
        return 0.0
    total_sq = 0.0
    for i in range(n_pixels):
        r1, g1, b1 = ref_pixels[3 * i : 3 * i + 3]
        r2, g2, b2 = test_pixels[3 * i : 3 * i + 3]
        dr, dg, db = r1 - r2, g1 - g2, b1 - b2
        total_sq += dr * dr + dg * dg + db * db
    return math.sqrt(total_sq / n_pixels)


def compare(ref: Path, test: Path, pass_thresh: float, warn_thresh: float) -> dict:
    ref_hdr, ref_frames = parse_gfrm(ref)
    test_hdr, test_frames = parse_gfrm(test)

    if ref_hdr != test_hdr:
        return {
            "status": "fail",
            "reason": "header mismatch",
            "ref_header": ref_hdr,
            "test_header": test_hdr,
            "ref": str(ref),
            "test": str(test),
        }

    per_frame = []
    cumulative_sq = 0.0
    n_frames = ref_hdr["frameCount"]
    pixels_per_frame = ref_hdr["ledsPerStrip"] * ref_hdr["stripCount"]
    max_l2 = 0.0

    for i in range(n_frames):
        l2 = per_pixel_l2(ref_frames[i]["pixels"], test_frames[i]["pixels"])
        per_frame.append({"frame": i, "l2": round(l2, 6)})
        cumulative_sq += l2 * l2
        if l2 > max_l2:
            max_l2 = l2

    mean_l2 = math.sqrt(cumulative_sq / n_frames) if n_frames > 0 else 0.0

    if mean_l2 <= pass_thresh:
        status = "pass"
    elif mean_l2 <= warn_thresh:
        status = "warn"
    else:
        status = "fail"

    return {
        "status": status,
        "ref": str(ref),
        "test": str(test),
        "header": ref_hdr,
        "frames": n_frames,
        "pixels_per_frame": pixels_per_frame,
        "mean_l2": round(mean_l2, 6),
        "max_l2": round(max_l2, 6),
        "pass_threshold": pass_thresh,
        "warn_threshold": warn_thresh,
        "per_frame_l2": per_frame,
    }


def emit_markdown(report: dict) -> str:
    icon = {"pass": "✓", "warn": "⚠", "fail": "✗"}.get(report.get("status"), "?")
    lines = [
        f"# Golden frame comparison — {icon} {report.get('status', '?').upper()}",
        "",
        f"- Reference: `{report.get('ref')}`",
        f"- Test:      `{report.get('test')}`",
    ]
    if report.get("status") == "fail" and "reason" in report:
        lines += ["", f"**Failure reason:** {report['reason']}"]
        return "\n".join(lines)
    lines += [
        f"- Frames: {report['frames']}, pixels/frame: {report['pixels_per_frame']}",
        f"- Mean L2: **{report['mean_l2']}**  (pass ≤ {report['pass_threshold']}, warn ≤ {report['warn_threshold']})",
        f"- Max  L2: {report['max_l2']}",
    ]
    return "\n".join(lines)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("reference", type=Path)
    p.add_argument("test", type=Path)
    p.add_argument("--report", type=Path, default=None,
                   help="Output JSON report path (default: stdout summary only)")
    p.add_argument("--markdown", type=Path, default=None,
                   help="Output markdown report path")
    p.add_argument("--threshold-pass", type=float, default=0.5,
                   help="Mean L2 threshold for pass (default 0.5; identical = 0)")
    p.add_argument("--threshold-warn", type=float, default=5.0,
                   help="Mean L2 threshold for warn (default 5.0)")
    p.add_argument("--quiet", action="store_true", help="Suppress stdout summary")
    args = p.parse_args()

    try:
        report = compare(args.reference, args.test, args.threshold_pass, args.threshold_warn)
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 3

    if args.report:
        args.report.write_text(json.dumps(report, indent=2))
    md = emit_markdown(report)
    if args.markdown:
        args.markdown.write_text(md)
    if not args.quiet:
        print(md)

    return {"pass": 0, "warn": 1, "fail": 2}.get(report["status"], 3)


if __name__ == "__main__":
    sys.exit(main())
