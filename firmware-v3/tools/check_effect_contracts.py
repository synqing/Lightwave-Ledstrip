#!/usr/bin/env python3
"""
Static regression checks for IEffect contract constraints.

Checks:
1. Centre-origin remediation files do not contain exception markers and still
   render via centre-pair addressing.
2. No-rainbow remediation files do not use HSV hue-wheel helpers.
3. IEffect files do not read raw ctx.audio.controlBus directly.
4. render() paths do not allocate heap memory.
5. AR control liveness wiring.
6. [INVERTED] ALL effect .cpp files scanned for linear-only iteration (no
   centre-origin).  Non-allowlisted files FAIL.
7. [INVERTED] ALL effect .cpp files scanned for rainbow/hue-wheel patterns.
   Non-allowlisted files FAIL.
8. K1 AP-only: no STA-mode WiFi usage outside allowlisted infrastructure files.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
IEFFECT_DIR = ROOT / "src" / "effects" / "ieffect"
SRC_DIR = ROOT / "src"

# ---------------------------------------------------------------------------
# Original positive-check lists (kept for backwards compatibility)
# ---------------------------------------------------------------------------

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

# ---------------------------------------------------------------------------
# Inverted centre-origin allowlist
#
# Files that legitimately use linear 0->N iteration without centre-pair
# addressing (utility effects, ambient Perlin backends, post-processing,
# reaction-diffusion buffers, etc.).
#
# Any NEW .cpp file under ieffect/ that contains a bare for(int i=0;) loop
# without centre-pair addressing MUST be added here or refactored to use
# centre-origin rendering.
# ---------------------------------------------------------------------------

CENTRE_LINEAR_ALLOWLIST: set[str] = {
    # --- Post-processing / utility / film-grade helpers ---
    "LGPFilmPost.cpp",
    # --- Perlin-backend effects (buffer-fill by definition) ---
    "LGPPerlinBackendEmotiscopeFullEffect.cpp",
    "LGPPerlinBackendEmotiscopeQuarterEffect.cpp",
    "LGPPerlinBackendFastLEDEffect.cpp",
    "LGPPerlinCausticsAmbientEffect.cpp",
    "LGPPerlinCausticsEffect.cpp",
    "LGPPerlinInterferenceWeaveAmbientEffect.cpp",
    "LGPPerlinInterferenceWeaveEffect.cpp",
    "LGPPerlinVeilAmbientEffect.cpp",
    "LGPPerlinVeilEffect.cpp",
    # --- Reaction-diffusion / cellular-automata (buffer simulations) ---
    "LGPReactionDiffusionEffect.cpp",
    "LGPReactionDiffusionTestRigEffect.cpp",
    "LGPReactionDiffusionTriangleEffect.cpp",
    "LGPReactionDiffusionAREffect.cpp",
    "LGPRDTriangleAREffect.cpp",
    "LGPRule30CathedralAREffect.cpp",
    "LGPLangtonHighwayAREffect.cpp",
    "LGPSierpinskiEffect.cpp",
    # --- Physics simulations (buffer-based) ---
    "LGPFluidDynamicsEffect.cpp",
    "LGPPhaseTransitionEffect.cpp",
    "LGPKdVSolitonPairEffect.cpp",
    "KuramotoTransportEffect.cpp",
    "ModalResonanceEffect.cpp",
    "LGPMeshNetworkEffect.cpp",
    "LGPHexagonalGridEffect.cpp",
    "LGPQuantumEntanglementEffect.cpp",
    "LGPTimeCrystalEffect.cpp",
    "LGPQuantumColorsEffect.cpp",
    # --- Holographic / interference / optical (intrinsically linear) ---
    "LGPHolographicEffect.cpp",
    "LGPHolographicEsTunedEffect.cpp",
    "LGPHolographicAutoCycleEffect.cpp",
    "LGPHolographicVortexEffect.cpp",
    "LGPInterferenceScannerEffect.cpp",
    "LGPInterferenceScannerEffectEnhanced.cpp",
    "LGPFresnelZonesEffect.cpp",
    "LGPFresnelCausticSweepEffect.cpp",
    "LGPFresnelCausticReactiveEffect.cpp",
    "LGPGratingScanEffect.cpp",
    "LGPGratingScanBreakupEffect.cpp",
    "LGPDiamondLatticeEffect.cpp",
    "LGPPhotonicCrystalEffect.cpp",
    "LGPPhotonicCrystalEffectEnhanced.cpp",
    "LGPQuasicrystalLatticeEffect.cpp",
    "LGPMoireCurtainsEffect.cpp",
    "LGPMoireSilkEffect.cpp",
    "LGPMoireCathedralAREffect.cpp",
    "InterferenceEffect.cpp",
    "ChromaticInterferenceEffect.cpp",
    # --- Chromatic / colour-processing effects ---
    "LGPChromaticAberrationEffect.cpp",
    "LGPChromaticLensEffect.cpp",
    "LGPChromaticPulseEffect.cpp",
    "LGPChromaticShearEffect.cpp",
    "LGPComplementaryMixingEffect.cpp",
    "LGPColorAcceleratorEffect.cpp",
    "LGPDopplerShiftEffect.cpp",
    "LGPRileyDissonanceEffect.cpp",
    "LGPOpalFilmEffect.cpp",
    "LGPStressGlassEffect.cpp",
    "LGPStressGlassMeltEffect.cpp",
    # --- Spatial / geometric effects ---
    "LGPBoxWaveEffect.cpp",
    "LGPConcentricRingsEffect.cpp",
    "LGPRadialRippleEffect.cpp",
    "LGPSpiralVortexEffect.cpp",
    "LGPParallaxDepthEffect.cpp",
    "LGPDNAHelixEffect.cpp",
    "LGPEvanescentSkinEffect.cpp",
    "LGPEvanescentDriftEffect.cpp",
    "LGPChladniHarmonicsEffect.cpp",
    "LGPGravitationalWaveChirpEffect.cpp",
    "LGPModalCavityEffect.cpp",
    "LGPGoldCodeSpeckleEffect.cpp",
    # --- Wave / collision effects ---
    "LGPWaveCollisionEffect.cpp",
    "LGPWaveCollisionEffectEnhanced.cpp",
    "LGPWaterCausticsEffect.cpp",
    "LGPWaterCausticsAREffect.cpp",
    "LGPCausticShardsEffect.cpp",
    "LGPCausticFanEffect.cpp",
    "WaveEffect.cpp",
    "WaveAmbientEffect.cpp",
    "WaveReactiveEffect.cpp",
    "OceanEffect.cpp",
    # --- Time-reversal mirror variants ---
    "LGPTimeReversalMirrorEffect.cpp",
    "LGPTimeReversalMirrorEffect_AR.cpp",
    "LGPTimeReversalMirrorEffect_Mod1.cpp",
    "LGPTimeReversalMirrorEffect_Mod2.cpp",
    "LGPTimeReversalMirrorEffect_Mod3.cpp",
    # --- StarBurst / shape packs ---
    "LGPStarBurstEffect.cpp",
    "LGPStarBurstEffectEnhanced.cpp",
    "LGPStarBurstNarrativeEffect.cpp",
    "LGPHolyShitBangersPack.cpp",
    "LGPShapeBangersPack.cpp",
    # --- AR effects (5-layer architecture uses own addressing) ---
    "LGPAiryCometAREffect.cpp",
    "LGPCatastropheCausticsAREffect.cpp",
    "LGPChimeraCrownAREffect.cpp",
    "LGPCymaticLadderAREffect.cpp",
    "LGPHarmonographHaloAREffect.cpp",
    "LGPHyperbolicPortalAREffect.cpp",
    "LGPIFSBioRelicAREffect.cpp",
    "LGPLorenzRibbonAREffect.cpp",
    "LGPMachDiamondsAREffect.cpp",
    "LGPRoseBloomAREffect.cpp",
    "LGPSchlierenFlowAREffect.cpp",
    "LGPSpirographCrownAREffect.cpp",
    "LGPSuperformulaGlyphAREffect.cpp",
    "LGPTalbotCarpetAREffect.cpp",
    # --- Classic / legacy effects ---
    "AudioWaveformEffect.cpp",
    "BPMEffect.cpp",
    "BPMEnhancedEffect.cpp",
    "BeatPulseBloomEffect.cpp",
    "BloomParityEffect.cpp",
    "BreathingEffect.cpp",
    "BreathingEnhancedEffect.cpp",
    "ChevronWavesEffect.cpp",
    "ChevronWavesEffectEnhanced.cpp",
    "FireEffect.cpp",
    "LGPSpectrumDetailEffect.cpp",
    "LGPSpectrumDetailEnhancedEffect.cpp",
    "LGPPerlinShocklinesEffect.cpp",
    "LGPSchlierenFlowEffect.cpp",
    "LGPQuantumTunnelingEffect.cpp",
    "SnapwaveLinearEffect.cpp",
    "RippleEnhancedEffect.cpp",
    "PlasmaEffect.cpp",
}

# ---------------------------------------------------------------------------
# Inverted rainbow/hue-wheel allowlist
#
# Files that legitimately use full HSV hue range (CHSV, hue++, fill_rainbow).
# Any NEW .cpp file under ieffect/ using these patterns MUST be added here
# or refactored to use palette-based colouring.
# ---------------------------------------------------------------------------

RAINBOW_ALLOWLIST: set[str] = set()
# Currently empty: no effect files legitimately use rainbow patterns.
# Add filenames here if a new effect genuinely requires fill_rainbow,
# CHSV(hue...) with full-range hue cycling, or hue++ in a loop.

# ---------------------------------------------------------------------------
# K1 AP-only allowlist
#
# Files under firmware-v3/src/ that legitimately reference WiFi STA mode.
# These are network infrastructure files that implement the WiFi subsystem
# and need STA references for diagnostics, event handling, or conditional
# compilation.
# ---------------------------------------------------------------------------

K1_STA_ALLOWLIST: set[str] = {
    "network/WiFiManager.cpp",
    "network/WebServer.cpp",
    "main.cpp",
}

# ---------------------------------------------------------------------------
# Patterns
# ---------------------------------------------------------------------------

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

RAINBOW_SCAN_PATTERNS = (
    re.compile(r"\bfill_rainbow\b"),
    re.compile(r"\bCHSV\s*\(\s*hue"),
    re.compile(r"\bhue\s*\+="),
    re.compile(r"\bhue\+\+"),
)

LINEAR_SWEEP_PATTERN = re.compile(
    r"for\s*\(\s*(?:int|uint\d+_t|size_t)\s+\w+\s*=\s*0\s*;"
)

CENTRE_ORIGIN_PATTERN = re.compile(
    r"SET_CENTER_PAIR|CENTER_LEFT"
    r"|79\s*-\s*\w"
    r"|80\s*\+\s*\w"
    r"|NUM_LEDS\s*/\s*2"
    r"|numLeds\s*/\s*2"
)

K1_STA_PATTERNS = (
    re.compile(r"\bWIFI_MODE_STA\b"),
    re.compile(r"\bWiFi\.begin\s*\("),
    re.compile(r"\besp_wifi_set_mode\s*\(\s*WIFI_MODE_STA"),
    re.compile(r"\bWIFI_STA\b"),
)

RAW_CONTROL_BUS_PATTERN = re.compile(r"ctx\.audio\.controlBus")
RENDER_START_PATTERN = re.compile(r"^\s*void\s+[\w:]+::render\s*\([^)]*\)\s*\{")
HEAP_IN_RENDER_PATTERN = re.compile(
    r"\b(new|malloc|calloc|realloc|heap_caps_malloc)\b|(?:^|[^A-Za-z0-9_])String\s*\("
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _is_reference_path(path: Path) -> bool:
    return any("reference" in part for part in path.parts)


def effect_cpp_files():
    """Yield all .cpp files under ieffect/, excluding reference subdirs."""
    for path in sorted(IEFFECT_DIR.rglob("*.cpp")):
        if _is_reference_path(path):
            continue
        yield path


def effect_files():
    """Yield all .cpp and .h files under ieffect/, excluding reference subdirs."""
    for path in IEFFECT_DIR.rglob("*"):
        if not path.is_file():
            continue
        if path.suffix not in {".cpp", ".h"}:
            continue
        if _is_reference_path(path):
            continue
        yield path


def src_files():
    """Yield all .cpp and .h files under src/."""
    for path in sorted(SRC_DIR.rglob("*")):
        if not path.is_file():
            continue
        if path.suffix not in {".cpp", ".h"}:
            continue
        yield path


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


# ---------------------------------------------------------------------------
# Checks (original, unchanged)
# ---------------------------------------------------------------------------

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
        if _is_reference_path(path):
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


# ---------------------------------------------------------------------------
# New inverted checks
# ---------------------------------------------------------------------------

def check_centre_origin_inverted(violations: list[str], stats: dict) -> None:
    """
    Scan ALL .cpp files under ieffect/ for linear sweep patterns without
    centre-pair addressing.  Files not in the allowlist FAIL.
    """
    scanned = 0
    flagged = 0
    for path in effect_cpp_files():
        scanned += 1
        text = read_text(path)
        has_linear = bool(LINEAR_SWEEP_PATTERN.search(text))
        if not has_linear:
            continue
        has_centre = bool(CENTRE_ORIGIN_PATTERN.search(text))
        if has_centre:
            continue
        # Linear iteration without centre-origin addressing
        if path.name not in CENTRE_LINEAR_ALLOWLIST:
            violations.append(
                f"[centre-scan] Linear sweep without centre-origin in non-allowlisted file: {path.name}"
            )
            flagged += 1
    stats["centre_scan_total"] = scanned
    stats["centre_scan_flagged"] = flagged


def check_rainbow_inverted(violations: list[str], stats: dict) -> None:
    """
    Scan ALL .cpp files under ieffect/ for rainbow/hue-wheel patterns.
    Files not in the allowlist FAIL.
    """
    scanned = 0
    flagged = 0
    for path in effect_cpp_files():
        scanned += 1
        text = read_text(path)
        matched_patterns = []
        for pat in RAINBOW_SCAN_PATTERNS:
            if pat.search(text):
                matched_patterns.append(pat.pattern)
        if not matched_patterns:
            continue
        if path.name not in RAINBOW_ALLOWLIST:
            violations.append(
                f"[rainbow-scan] Rainbow/hue-wheel pattern in non-allowlisted file: "
                f"{path.name} (matched: {', '.join(matched_patterns)})"
            )
            flagged += 1
    stats["rainbow_scan_total"] = scanned
    stats["rainbow_scan_flagged"] = flagged


def check_k1_ap_only(violations: list[str], stats: dict) -> None:
    """
    Scan ALL .cpp and .h files under src/ for WiFi STA-mode references.
    Files not in the allowlist FAIL.
    """
    scanned = 0
    flagged = 0
    for path in src_files():
        scanned += 1
        text = read_text(path)
        # Compute relative path from src/ for allowlist matching
        try:
            rel = str(path.relative_to(SRC_DIR))
        except ValueError:
            rel = path.name

        matched_patterns = []
        for pat in K1_STA_PATTERNS:
            m = pat.search(text)
            if m:
                matched_patterns.append(pat.pattern)
        if not matched_patterns:
            continue
        if rel not in K1_STA_ALLOWLIST:
            violations.append(
                f"[k1-ap-only] WiFi STA-mode reference in non-allowlisted file: "
                f"{rel} (matched: {', '.join(matched_patterns)})"
            )
            flagged += 1
    stats["k1_sta_scan_total"] = scanned
    stats["k1_sta_scan_flagged"] = flagged


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    violations: list[str] = []
    stats: dict = {}

    # Original checks (unchanged)
    check_centre_files(violations)
    check_no_rainbow_files(violations)
    check_raw_control_bus_usage(violations)
    check_heap_alloc_in_render(violations)
    check_ar_control_liveness(violations)

    # New inverted checks
    check_centre_origin_inverted(violations, stats)
    check_rainbow_inverted(violations, stats)
    check_k1_ap_only(violations, stats)

    # Count total effect files
    effect_file_count = len(list(effect_files()))

    if violations:
        print("FAIL: effect contract checks found issues:")
        for issue in violations:
            print(f"  - {issue}")
        print()

    # Always print scan summary
    print(f"=== Scan Summary ===")
    print(f"  Effect source files (ieffect/, .cpp+.h): {effect_file_count}")
    print(f"  Centre-origin scan: {stats.get('centre_scan_total', 0)} .cpp files checked"
          f" | allowlist: {len(CENTRE_LINEAR_ALLOWLIST)} files"
          f" | flagged: {stats.get('centre_scan_flagged', 0)}")
    print(f"  Rainbow scan:       {stats.get('rainbow_scan_total', 0)} .cpp files checked"
          f" | allowlist: {len(RAINBOW_ALLOWLIST)} files"
          f" | flagged: {stats.get('rainbow_scan_flagged', 0)}")
    print(f"  K1 AP-only scan:    {stats.get('k1_sta_scan_total', 0)} .cpp/.h files checked"
          f" | allowlist: {len(K1_STA_ALLOWLIST)} files"
          f" | flagged: {stats.get('k1_sta_scan_flagged', 0)}")

    if violations:
        return 1

    print("\nPASS: all effect contract checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
