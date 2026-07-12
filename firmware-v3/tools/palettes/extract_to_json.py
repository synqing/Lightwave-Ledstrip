#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
extract_to_json.py — Generate Palettes_Master.json from canonical C++ source.

PURPOSE
-------
Parse the gradient-pair palette definitions in
firmware-v3/src/palettes/Palettes_MasterData.cpp (the single source of truth
for K1 palette colour data) and emit a deterministic JSON file matching the
iOS Codable shape `MasterPalettePayload` declared in
lightwave-ios-v2/LightwaveOS/Models/PaletteMetadata.swift:94-98.

The output bundle is consumed by the iOS app's PaletteStore (see
PaletteMetadata.swift) so palette swatches render real gradients instead of
the fallback `[.gray, .white]`.

CANONICAL OUTPUT PATH
---------------------
lightwave-ios-v2/LightwaveOS/Resources/Palettes/Palettes_Master.json

INPUT FORMAT (what we parse)
----------------------------
Each palette is declared like:

    DEFINE_GRADIENT_PALETTE(name_gp){
      0,   r0, g0, b0,
      37,  r1, g1, b1,
      ...
      255, rN, gN, bN
    };

Where the first integer is a uint8_t position (0..255 inclusive) and the
remaining three are uint8_t r/g/b. Whitespace and comments between entries
are tolerated.

The master ordering (id 0..74) is taken from the gMasterPalettes[] array in
the same file; the human-readable names come from MasterPaletteNames[].

SAMPLING ALGORITHM
------------------
Per Captain spec: gradient-pair palettes are emitted VERBATIM — we do not
oversample. Each input keyframe `(pos, r, g, b)` becomes one output stop
`{ position: pos / 255, r, g, b }`. Position is rounded to 3 decimals.

This matches the iOS Codable contract (`PaletteColor.position: Double`,
`r/g/b: UInt8`) exactly. All 75 palettes happen to be gradient-pair
declarations, so no fixed-256 sampling is required for the current corpus.

If a future palette is added as a fixed 256-entry table, extend `parse_cpp()`
to detect that form and fall back to the 8-stop sweep at positions
[0, 36, 72, 108, 144, 180, 216, 255]/255.

DETERMINISM
-----------
- Input order = output order (gMasterPalettes[] index = JSON id).
- No randomness, no time-based fields beyond the optional `source` commit SHA.
- JSON is serialised with sort_keys=False to preserve field order
  (id, name, category, colors), indent=2 for diff readability.

RE-RUN
------
    cd Lightwave-Ledstrip
    python3 firmware-v3/tools/palettes/extract_to_json.py

By default writes
lightwave-ios-v2/LightwaveOS/Resources/Palettes/Palettes_Master.json relative
to the repo root. Override with --output PATH. The `source` field is set to
"Palettes_MasterData.cpp@<short-sha>" using `git rev-parse --short HEAD`;
override with --source-tag.

REQUIREMENTS
------------
Python 3.8+ standard library only. No third-party dependencies.

BRITISH ENGLISH NOTE
--------------------
This file uses 'colour' for human-facing prose; field names follow the iOS
Codable contract (which uses 'colors' to match firmware JSON). Field names
are not translated.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple


# ---------------------------------------------------------------------------
# Repo layout
# ---------------------------------------------------------------------------

# This script lives at firmware-v3/tools/palettes/extract_to_json.py
# Repo root is three levels up.
SCRIPT_PATH = Path(__file__).resolve()
REPO_ROOT = SCRIPT_PATH.parents[3]

CPP_SOURCE = REPO_ROOT / "firmware-v3" / "src" / "palettes" / "Palettes_MasterData.cpp"
DEFAULT_OUTPUT = (
    REPO_ROOT
    / "lightwave-ios-v2"
    / "LightwaveOS"
    / "Resources"
    / "Palettes"
    / "Palettes_Master.json"
)


# ---------------------------------------------------------------------------
# Category resolver — mirrors helpers in firmware-v3/src/palettes/Palettes_Master.h
# (CPT_CITY 0..32 → "Artistic", CRAMERI 33..56 → "Scientific",
#  COLORSPACE 57..74 → "LGP-Optimised").
#
# Note: the firmware-side getPaletteCategory() returns "LGP-Optimized" (US
# spelling). The iOS bundle uses British English "LGP-Optimised" to match
# the iOS hard-coded fallbacks in PaletteMetadata.swift `defaults` (line 179
# onward). Both are accepted by the iOS Codable layer because `category` is
# a free-form String, but matching the iOS fallback string keeps the UI
# consistent across bundle-load and fallback-load paths.
# ---------------------------------------------------------------------------

def category_for_id(palette_id: int) -> str:
    if 0 <= palette_id <= 32:
        return "Artistic"
    if 33 <= palette_id <= 56:
        return "Scientific"
    if 57 <= palette_id <= 74:
        return "LGP-Optimised"
    return "Unknown"


# ---------------------------------------------------------------------------
# Parser
# ---------------------------------------------------------------------------

# Match `DEFINE_GRADIENT_PALETTE(name_gp){ ... };`
# We capture the symbol name and the body (between { and };).
GRADIENT_RE = re.compile(
    r"DEFINE_GRADIENT_PALETTE\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)\s*\{(.*?)\}\s*;",
    re.DOTALL,
)

# Match the master palette array body.
MASTER_ARRAY_RE = re.compile(
    r"const\s+TProgmemRGBGradientPaletteRef\s+gMasterPalettes\s*\[\s*\]\s*=\s*\{(.*?)\};",
    re.DOTALL,
)

# Match the palette names array body.
NAMES_ARRAY_RE = re.compile(
    r"const\s+char\s*\*\s*const\s+MasterPaletteNames\s*\[\s*\]\s*=\s*\{(.*?)\};",
    re.DOTALL,
)

# Strip C/C++ comments so we don't accidentally pick up commented-out names.
LINE_COMMENT_RE = re.compile(r"//[^\n]*")
BLOCK_COMMENT_RE = re.compile(r"/\*.*?\*/", re.DOTALL)


def strip_comments(src: str) -> str:
    src = BLOCK_COMMENT_RE.sub("", src)
    src = LINE_COMMENT_RE.sub("", src)
    return src


def parse_gradient_body(body: str) -> List[Tuple[int, int, int, int]]:
    """Parse the body of a DEFINE_GRADIENT_PALETTE block into (pos, r, g, b)
    tuples. Tolerates whitespace, newlines, trailing commas. Numbers may be
    decimal integers in 0..255.
    """
    cleaned = strip_comments(body)
    nums = re.findall(r"-?\d+", cleaned)
    if len(nums) % 4 != 0:
        raise ValueError(
            f"Gradient body has {len(nums)} integers, not divisible by 4: "
            f"{cleaned[:120]!r}"
        )
    stops: List[Tuple[int, int, int, int]] = []
    for i in range(0, len(nums), 4):
        pos = int(nums[i])
        r = int(nums[i + 1])
        g = int(nums[i + 2])
        b = int(nums[i + 3])
        for v, label in ((pos, "position"), (r, "r"), (g, "g"), (b, "b")):
            if not (0 <= v <= 255):
                raise ValueError(
                    f"{label} value {v} out of range [0,255] in body "
                    f"{cleaned[:120]!r}"
                )
        stops.append((pos, r, g, b))
    if not stops:
        raise ValueError("Gradient body produced no stops")
    # The C++ data is authored in ascending-position order. Sanity-check.
    for prev, nxt in zip(stops, stops[1:]):
        if nxt[0] < prev[0]:
            raise ValueError(
                f"Stops not in ascending order: prev={prev}, next={nxt}"
            )
    return stops


def parse_cpp(src_path: Path) -> Tuple[List[str], Dict[str, List[Tuple[int, int, int, int]]], List[str]]:
    """Return (master_order, gradient_map, names) where:
      - master_order is the list of gradient symbol identifiers in
        gMasterPalettes[] order (ids 0..74).
      - gradient_map maps each symbol → list of (pos,r,g,b) stops.
      - names is the human-readable palette name list in id order.
    """
    text = src_path.read_text(encoding="utf-8")

    # ---- Gradients ----
    gradient_map: Dict[str, List[Tuple[int, int, int, int]]] = {}
    for match in GRADIENT_RE.finditer(text):
        sym = match.group(1)
        body = match.group(2)
        gradient_map[sym] = parse_gradient_body(body)

    if not gradient_map:
        raise RuntimeError(f"No DEFINE_GRADIENT_PALETTE blocks found in {src_path}")

    # ---- Master order ----
    master_match = MASTER_ARRAY_RE.search(text)
    if master_match is None:
        raise RuntimeError(
            f"Could not locate gMasterPalettes[] declaration in {src_path}"
        )
    master_body = strip_comments(master_match.group(1))
    master_order = re.findall(r"[A-Za-z_][A-Za-z0-9_]*", master_body)
    if not master_order:
        raise RuntimeError("Failed to extract symbol identifiers from gMasterPalettes[]")

    # ---- Names ----
    names_match = NAMES_ARRAY_RE.search(text)
    if names_match is None:
        raise RuntimeError(
            f"Could not locate MasterPaletteNames[] declaration in {src_path}"
        )
    names_body = strip_comments(names_match.group(1))
    names = re.findall(r'"((?:[^"\\]|\\.)*)"', names_body)

    # Sanity: master_order length should match names length.
    if len(master_order) != len(names):
        raise RuntimeError(
            f"Mismatch: gMasterPalettes has {len(master_order)} entries, "
            f"MasterPaletteNames has {len(names)} entries"
        )

    # Sanity: every symbol in master_order has a parsed gradient body.
    missing = [s for s in master_order if s not in gradient_map]
    if missing:
        raise RuntimeError(
            f"Master-array references symbols with no gradient body: {missing}"
        )

    return master_order, gradient_map, names


# ---------------------------------------------------------------------------
# Output assembly
# ---------------------------------------------------------------------------

def stops_to_payload(stops: List[Tuple[int, int, int, int]]) -> List[Dict[str, object]]:
    """Convert raw (pos[0..255], r, g, b) stops to the iOS Codable shape:
      [ { position: float [0..1], r: int, g: int, b: int }, ... ]
    Position is rounded to 3 decimals for stable diffs.
    """
    return [
        {
            "position": round(pos / 255.0, 3),
            "r": r,
            "g": g,
            "b": b,
        }
        for (pos, r, g, b) in stops
    ]


def build_payload(
    master_order: List[str],
    gradient_map: Dict[str, List[Tuple[int, int, int, int]]],
    names: List[str],
    source_tag: str,
) -> Dict[str, object]:
    palettes = []
    for idx, (sym, name) in enumerate(zip(master_order, names)):
        stops = gradient_map[sym]
        palettes.append(
            {
                "id": idx,
                "name": name,
                "category": category_for_id(idx),
                "colors": stops_to_payload(stops),
            }
        )
    return {
        "version": 2,
        "source": source_tag,
        "palettes": palettes,
    }


def short_git_sha(repo_root: Path) -> Optional[str]:
    try:
        result = subprocess.run(
            ["git", "-C", str(repo_root), "rev-parse", "--short", "HEAD"],
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode == 0:
            sha = result.stdout.strip()
            return sha or None
    except (OSError, subprocess.SubprocessError):
        pass
    return None


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description="Generate Palettes_Master.json from canonical C++ source."
    )
    parser.add_argument(
        "--source",
        type=Path,
        default=CPP_SOURCE,
        help=f"Path to Palettes_MasterData.cpp (default: {CPP_SOURCE})",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help=f"Path to output JSON (default: {DEFAULT_OUTPUT})",
    )
    parser.add_argument(
        "--source-tag",
        type=str,
        default=None,
        help=(
            "Override the 'source' attribution string. Default: "
            "'Palettes_MasterData.cpp@<short-sha>' resolved from git HEAD."
        ),
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help=(
            "Do not write; verify that the existing output file matches what "
            "would be generated. Exits non-zero on mismatch."
        ),
    )
    args = parser.parse_args(argv)

    if not args.source.exists():
        print(f"error: source not found: {args.source}", file=sys.stderr)
        return 2

    master_order, gradient_map, names = parse_cpp(args.source)

    source_tag = args.source_tag
    if source_tag is None:
        sha = short_git_sha(REPO_ROOT)
        if sha:
            source_tag = f"Palettes_MasterData.cpp@{sha}"
        else:
            source_tag = "Palettes_MasterData.cpp"

    payload = build_payload(master_order, gradient_map, names, source_tag)
    text = json.dumps(payload, indent=2, sort_keys=False, ensure_ascii=False) + "\n"

    if args.check:
        existing = (
            args.output.read_text(encoding="utf-8") if args.output.exists() else ""
        )
        if existing == text:
            print(f"OK: {args.output} matches generated output")
            return 0
        print(
            f"DRIFT: {args.output} does not match generated output. "
            f"Re-run without --check to refresh.",
            file=sys.stderr,
        )
        return 1

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(text, encoding="utf-8")

    total_stops = sum(len(p["colors"]) for p in payload["palettes"])
    print(
        f"wrote {args.output}\n"
        f"  palettes: {len(payload['palettes'])}\n"
        f"  total colour stops: {total_stops}\n"
        f"  source: {source_tag}\n"
        f"  size: {len(text)} bytes"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
