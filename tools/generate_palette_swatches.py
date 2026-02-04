#!/usr/bin/env python3
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CPP = ROOT / "firmware/v2/src/palettes/Palettes_MasterData.cpp"
OUT = ROOT / "lightwave-ios-v2/LightwaveOS/Resources/Palettes/Palettes_Master.json"

CATEGORY_BY_ID = [
    (0, 32, "Artistic"),
    (33, 56, "Scientific"),
    (57, 74, "LGP-Optimised"),
]


def category_for_id(idx: int) -> str:
    for start, end, name in CATEGORY_BY_ID:
        if start <= idx <= end:
            return name
    return "Unknown"


def parse_palettes(cpp_text: str):
    pattern = re.compile(r"DEFINE_GRADIENT_PALETTE\(([^)]+)\)\s*\{(.*?)\};", re.S)
    palettes = {}
    for match in pattern.finditer(cpp_text):
        name = match.group(1).strip()
        body = match.group(2)
        nums = [int(x) for x in re.findall(r"\d+", body)]
        if len(nums) % 4 != 0:
            raise ValueError(f"Palette {name} has non-multiple-of-4 entries")
        stops = []
        for i in range(0, len(nums), 4):
            pos, r, g, b = nums[i : i + 4]
            stops.append({
                "position": round(pos / 255.0, 6),
                "r": r,
                "g": g,
                "b": b,
            })
        palettes[name] = stops
    return palettes


def parse_master_order(cpp_text: str):
    block = re.search(r"gMasterPalettes\[\]\s*=\s*\{(.*?)\};", cpp_text, re.S)
    if not block:
        raise ValueError("gMasterPalettes block not found")
    body = block.group(1)
    # Strip comments
    body = re.sub(r"//.*", "", body)
    names = re.findall(r"([A-Za-z0-9_]+_gp)", body)
    return names


def parse_master_names(cpp_text: str):
    block = re.search(r"MasterPaletteNames\[\]\s*=\s*\{(.*?)\};", cpp_text, re.S)
    if not block:
        raise ValueError("MasterPaletteNames block not found")
    body = block.group(1)
    names = re.findall(r"\"([^\"]+)\"", body)
    return names


def main():
    cpp_text = CPP.read_text(encoding="utf-8")
    palette_defs = parse_palettes(cpp_text)
    order = parse_master_order(cpp_text)
    names = parse_master_names(cpp_text)

    if len(order) != len(names):
        raise ValueError(f"Order ({len(order)}) != names ({len(names)})")

    palettes = []
    for idx, (symbol, display_name) in enumerate(zip(order, names)):
        if symbol not in palette_defs:
            raise ValueError(f"Palette symbol {symbol} missing definition")
        palettes.append({
            "id": idx,
            "name": display_name,
            "category": category_for_id(idx),
            "colors": palette_defs[symbol],
        })

    out = {
        "version": 1,
        "source": str(CPP),
        "palettes": palettes,
    }

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(out, indent=2), encoding="utf-8")
    print(f"Wrote {OUT} ({len(palettes)} palettes)")


if __name__ == "__main__":
    main()
