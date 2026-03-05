#!/usr/bin/env python3
"""
Static regression checks for IEffect contract constraints.

Checks:
1. Centre-origin remediation files do not contain exception markers and still
   render via centre-pair addressing.
2. No-rainbow remediation files do not use HSV hue-wheel helpers.
3. IEffect files do not read raw ctx.audio.controlBus directly.
4. render() paths do not allocate heap memory.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
IEFFECT_DIR = ROOT / "src" / "effects" / "ieffect"

CENTRE_FILES = [
    "LGPAnisotropicCloakEffect.cpp",
    "LGPAuroraBorealisEffect.cpp",
    "LGPBioluminescentWavesEffect.cpp",
    "LGPBirefringentShearEffect.cpp",
    "LGPCrystallineGrowthEffect.cpp",
    "LGPGrinCloakEffect.cpp",
    "LGPMetamaterialCloakEffect.cpp",
    "LGPNeuralNetworkEffect.cpp",
    "LGPPlasmaMembraneEffect.cpp",
    "LGPSolitonWavesEffect.cpp",
]

NO_RAINBOW_FILES = [
    "BloomParityEffect.cpp",
    "LGPSpectrumDetailEffect.cpp",
]

EXCEPTION_MARKERS = (
    "CENTRE-ORIGIN EXCEPTION",
    "CENTER-ORIGIN EXCEPTION",
    "CENTER ORIGIN EXCEPTION",
)

RAINBOW_PATTERNS = (
    re.compile(r"\bCHSV\b"),
    re.compile(r"rgb2hsv_approximate"),
    re.compile(r"prog\s*\*\s*255(\.0f)?"),
)

RAW_CONTROL_BUS_PATTERN = re.compile(r"ctx\.audio\.controlBus")
RENDER_START_PATTERN = re.compile(r"^\s*void\s+[\w:]+::render\s*\([^)]*\)\s*\{")
HEAP_IN_RENDER_PATTERN = re.compile(
    r"\b(new|malloc|calloc|realloc|heap_caps_malloc)\b|(?:^|[^A-Za-z0-9_])String\s*\("
)


def effect_files():
    for path in IEFFECT_DIR.rglob("*"):
        if not path.is_file():
            continue
        if path.suffix not in {".cpp", ".h"}:
            continue
        if any("reference" in part for part in path.parts):
            continue
        yield path


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def check_centre_files(violations: list[str]) -> None:
    for rel in CENTRE_FILES:
        path = IEFFECT_DIR / rel
        if not path.exists():
            violations.append(f"[centre] Missing expected remediation file: {path}")
            continue
        text = read_text(path)
        if any(marker in text for marker in EXCEPTION_MARKERS):
            violations.append(f"[centre] Exception marker present: {path}")
        if "SET_CENTER_PAIR(" not in text and "CENTER_LEFT" not in text:
            violations.append(f"[centre] No centre-pair addressing marker found: {path}")


def check_no_rainbow_files(violations: list[str]) -> None:
    for rel in NO_RAINBOW_FILES:
        path = IEFFECT_DIR / rel
        if not path.exists():
            violations.append(f"[colour] Missing expected remediation file: {path}")
            continue
        text = read_text(path)
        for pattern in RAINBOW_PATTERNS:
            if pattern.search(text):
                violations.append(
                    f"[colour] Disallowed rainbow/hue-wheel pattern '{pattern.pattern}' in {path}"
                )


def check_raw_control_bus_usage(violations: list[str]) -> None:
    for path in effect_files():
        for idx, line in enumerate(read_text(path).splitlines(), start=1):
            if RAW_CONTROL_BUS_PATTERN.search(line):
                violations.append(f"[api] Raw control bus access in {path}:{idx}")


def check_heap_alloc_in_render(violations: list[str]) -> None:
    for path in IEFFECT_DIR.rglob("*.cpp"):
        if any("reference" in part for part in path.parts):
            continue
        lines = read_text(path).splitlines()
        in_render = False
        brace_depth = 0

        for idx, line in enumerate(lines, start=1):
            if not in_render:
                if RENDER_START_PATTERN.search(line):
                    in_render = True
                    brace_depth = line.count("{") - line.count("}")
                    code_part = line.split("//", 1)[0]
                    if HEAP_IN_RENDER_PATTERN.search(code_part):
                        violations.append(f"[heap] Heap allocation in render at {path}:{idx}")
                continue

            code_part = line.split("//", 1)[0]
            if HEAP_IN_RENDER_PATTERN.search(code_part):
                violations.append(f"[heap] Heap allocation in render at {path}:{idx}")

                brace_depth += line.count("{") - line.count("}")
                if brace_depth <= 0:
                    in_render = False


def check_ar_control_liveness(violations: list[str]) -> None:
    """
    Ensure 5-layer AR effects are wired to the shared control modulation path.

    We intentionally check for shared helper usage instead of raw control names:
    - updateSignals(...) carries attack/release + spectral gains
    - buildModulation(...) carries motion/colour/beat profile
    - applyBedImpactMemoryMix(...) carries audio_mix + beat_gain + memory_gain + motion_depth blend
    """
    required_regex = {
        "timing_pipeline": re.compile(r"\bupdateSignals\s*\("),
        "modulation_pipeline": re.compile(r"\bbuildModulation\s*\("),
        "ambient_reactive_mix": re.compile(r"\bapplyBedImpactMemoryMix\s*\("),
        "motion_usage": re.compile(r"\bmod\.motionRate\b"),
        "colour_anchor_usage": re.compile(r"\bm_ar\.tonalHue\s*=\s*mod\.baseHue\b"),
        "spectral_usage": re.compile(r"\bsig\.(bass|mid|treble|flux|harmonic|rhythmic)\b"),
    }

    for path in sorted(IEFFECT_DIR.glob("*AREffect.cpp")):
        text = read_text(path)
        if "buildModulation(" not in text and "Ar16Controls" not in text:
            continue
        for check_name, pattern in required_regex.items():
            if not pattern.search(text):
                violations.append(f"[ar-liveness] Missing {check_name} in {path}")


def main() -> int:
    violations: list[str] = []

    check_centre_files(violations)
    check_no_rainbow_files(violations)
    check_raw_control_bus_usage(violations)
    check_heap_alloc_in_render(violations)
    check_ar_control_liveness(violations)

    if violations:
        print("FAIL: effect contract checks found issues:")
        for issue in violations:
            print(f" - {issue}")
        return 1

    print("PASS: effect contract checks")
    print(f"Checked {len(list(effect_files()))} ieffect source files.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
