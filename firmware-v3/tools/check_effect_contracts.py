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

Brand-voice extensions (Block 2 items 19, 20, 15 — Phase 0A first enforcement primitive):
9. [BRAND-VOICE §3.5 / Block 2 item 19] Per-bin tempo-bank read inside render() —
   FAIL.  `tempi[*]` / `tempoBank[*]` indexed access from the render call path
   contradicts BRAND_VOICE_POSTURE.md §3.5 (literal ES tempo-bank swarm rendering
   banned) and §4.5 (tempo-bank as engine plumbing only).  Single-tempo `tempoPhase`
   scalar reads remain compliant.
10. [BRAND-VOICE §3.6 / Block 2 item 20] GEO-06 CircularRing / GEO-10
    AsymmetricDriftOrigin name patterns — FAIL.  These effect families violate
    HW-03 Centre-Origin strict invariant per §6 item 10 / §3 C-8.  No allowlist.
11. [BRAND-VOICE §3.3 / Block 2 item 15] Multi-element fragmentation patterns
    (PendulumChain / PendulumArray / PendulumSwarm / BoidSwarm / BoidFlock /
    KuramotoSwarm / KuramotoOscillators / OscillatorChain / OscillatorArray) —
    WARN (boundary-flag, not hard FAIL).  These name patterns are likely-but-not-
    certain indicators of multi-element fragmentation per §3.3.  Continuum-class
    alternatives (heat-eq, spring-mass-lattice as ≥80 coupled cells reading as
    continuum) are §4.3 boundary cases and remain allowed.
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

RAINBOW_ALLOWLIST: set[str] = {
    # LGP AR-family effects whose CHSV(hue, ...) pattern is *deliberate*
    # palette-locked colouring (the hue argument is a static palette index,
    # not a full hue-wheel sweep) per Move 0.2 audit §3.5. Allowlisted to
    # silence the rainbow-scan rule without weakening the rule itself.
    # LGPReactionDiffusionAREffect.cpp deliberately excluded — pending
    # Captain hardware A/B per audit §3.2 I2 INVESTIGATE.
    "LGPAiryCometAREffect.cpp",
    "LGPChimeraCrownAREffect.cpp",
    "LGPCymaticLadderAREffect.cpp",
    "LGPHarmonographHaloAREffect.cpp",
    "LGPHyperbolicPortalAREffect.cpp",
    "LGPLangtonHighwayAREffect.cpp",
    "LGPLorenzRibbonAREffect.cpp",
    "LGPMachDiamondsAREffect.cpp",
    "LGPMoireCathedralAREffect.cpp",
    "LGPRoseBloomAREffect.cpp",
    "LGPSchlierenFlowAREffect.cpp",
    "LGPSpirographCrownAREffect.cpp",
    "LGPSuperformulaGlyphAREffect.cpp",
    "LGPTalbotCarpetAREffect.cpp",
    "LGPWaterCausticsAREffect.cpp",
}

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
    # serial/SerialCLI.cpp uses a read-only diagnostic ternary against
    # WIFI_MODE_STA to print the current mode — no STA activation. Audit §2.4.
    "serial/SerialCLI.cpp",
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
    r"|writeCentrePair"
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
# Brand-voice extensions (Phase 0A first enforcement primitive)
# ---------------------------------------------------------------------------

# §3.5 / item 19 — per-bin tempo-bank read in render() path.
# Matches `tempi[<expr>]` and `tempoBank[<expr>]` where the index is NOT the
# literal `0`.  Single-element access at index 0 is permitted as the engine-
# plumbing escape (a single-tempo bank-of-1 is semantically equivalent to a
# scalar `tempoPhase`).
TEMPO_BANK_INDEXED_PATTERN = re.compile(
    r"\b(tempi|tempoBank)\s*\[\s*(?!0\s*\])([^\]]+)\]"
)

# Empty allowlist — any new tempo-bank-indexed render-path access must be
# explicitly justified by adding the filename here.
TEMPO_BANK_ALLOWLIST: set[str] = set()

# §3.6 / item 20 — GEO-06 / GEO-10 kill: filename or class-name match against
# CircularRing / AsymmetricDriftOrigin / DriftOrigin patterns.
# No allowlist (HW-03 strict per §3 C-8).
GEO_KILL_NAME_PATTERNS = (
    re.compile(r"CircularRing", re.IGNORECASE),
    re.compile(r"AsymmetricDriftOrigin", re.IGNORECASE),
    re.compile(r"AsymmetricDrift", re.IGNORECASE),
    re.compile(r"\bDriftOrigin\b", re.IGNORECASE),
)

# §3.3 / item 15 — multi-element fragmentation: filename or class-name match.
# Conservative pattern set — only flags exact fragmentation indicators.
# Continuum-class names (KuramotoTransport, ModalResonance, SpringMassLattice
# etc.) are NOT matched.
FRAGMENTATION_NAME_PATTERNS = (
    re.compile(r"PendulumChain", re.IGNORECASE),
    re.compile(r"PendulumArray", re.IGNORECASE),
    re.compile(r"PendulumSwarm", re.IGNORECASE),
    re.compile(r"BoidSwarm", re.IGNORECASE),
    re.compile(r"BoidFlock", re.IGNORECASE),
    re.compile(r"KuramotoSwarm", re.IGNORECASE),
    re.compile(r"KuramotoOscillators", re.IGNORECASE),
    re.compile(r"OscillatorChain", re.IGNORECASE),
    re.compile(r"OscillatorArray", re.IGNORECASE),
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


def check_tempo_bank_in_render(violations: list[str], stats: dict,
                                effect_dir: Path = IEFFECT_DIR) -> None:
    """
    [BRAND-VOICE §3.5 / Block 2 item 19] FAIL on per-bin tempo-bank reads
    inside render() blocks.

    Walks each .cpp file's render() lexical block (same scheme as
    check_heap_alloc_in_render).  Flags any `tempi[expr]` or `tempoBank[expr]`
    indexed access where the index is NOT the literal `0`.

    False-positive risk: low.  The pattern only matches genuine indexed access;
    scalar reads like `controlBus.tempoPhase` are not affected.  Single-element
    bank-of-1 access at literal index 0 is exempt (engine-plumbing escape).
    """
    scanned = 0
    flagged = 0
    for path in sorted(effect_dir.rglob("*.cpp")):
        if _is_reference_path(path):
            continue
        if path.name in TEMPO_BANK_ALLOWLIST:
            continue
        scanned += 1
        lines = read_text(path).splitlines()
        in_render = False
        brace_depth = 0

        for idx, line in enumerate(lines, start=1):
            code_part = line.split("//", 1)[0]
            if not in_render:
                if RENDER_START_PATTERN.search(line):
                    in_render = True
                    brace_depth = line.count("{") - line.count("}")
                    if TEMPO_BANK_INDEXED_PATTERN.search(code_part):
                        violations.append(
                            f"[tempo-bank-in-render] Per-bin tempo-bank read in render() at {path.name}:{idx}"
                        )
                        flagged += 1
                continue

            if TEMPO_BANK_INDEXED_PATTERN.search(code_part):
                violations.append(
                    f"[tempo-bank-in-render] Per-bin tempo-bank read in render() at {path.name}:{idx}"
                )
                flagged += 1

            brace_depth += line.count("{") - line.count("}")
            if brace_depth <= 0:
                in_render = False

    stats["tempo_bank_scan_total"] = scanned
    stats["tempo_bank_scan_flagged"] = flagged


def check_geo_kill_patterns(violations: list[str], stats: dict,
                             effect_dir: Path = IEFFECT_DIR) -> None:
    """
    [BRAND-VOICE §3.6 / Block 2 item 20] FAIL on GEO-06 CircularRing /
    GEO-10 AsymmetricDriftOrigin name patterns.

    Scans filenames AND class-name declarations.  No allowlist (HW-03 strict
    per §3 C-8 + §6 item 10).

    False-positive risk: low.  Names are specific.  ConcentricRings (existing
    allowlisted family) does NOT match `CircularRing` because the pattern
    requires the literal token "CircularRing" not "ConcentricRings".
    """
    scanned = 0
    flagged = 0
    class_decl_pattern = re.compile(r"\bclass\s+(\w+)")

    for path in sorted(effect_dir.rglob("*")):
        if not path.is_file():
            continue
        if path.suffix not in {".cpp", ".h"}:
            continue
        if _is_reference_path(path):
            continue
        scanned += 1

        # Filename check
        for pat in GEO_KILL_NAME_PATTERNS:
            if pat.search(path.name):
                violations.append(
                    f"[geo-kill] HW-03 violation — name pattern matches "
                    f"GEO-06/GEO-10 kill in filename: {path.name} (matched: {pat.pattern})"
                )
                flagged += 1
                break  # one violation per file from filename check

        # Class-name check
        text = read_text(path)
        for class_match in class_decl_pattern.finditer(text):
            class_name = class_match.group(1)
            for pat in GEO_KILL_NAME_PATTERNS:
                if pat.search(class_name):
                    violations.append(
                        f"[geo-kill] HW-03 violation — class name matches "
                        f"GEO-06/GEO-10 kill: {class_name} in {path.name} "
                        f"(matched: {pat.pattern})"
                    )
                    flagged += 1

    stats["geo_kill_scan_total"] = scanned
    stats["geo_kill_scan_flagged"] = flagged


def check_fragmentation_patterns(warnings: list[str], stats: dict,
                                  effect_dir: Path = IEFFECT_DIR) -> None:
    """
    [BRAND-VOICE §3.3 / Block 2 item 15] WARN (boundary-flag, not hard FAIL)
    on multi-element fragmentation name patterns.

    Captain decision: emit as warning rather than violation because pattern
    detection is naming-convention-dependent and false-positive risk is medium.
    Continuum-class alternatives (KuramotoTransport, ModalResonance,
    SpringMassLattice, heat-eq) are §4.3 boundary cases and intentionally NOT
    matched by these patterns.

    False-positive risk: medium.  An effect with a fragmentation-style name
    that is actually a continuum-class implementation would WARN; reviewer
    decides PASS / KILL.  Add to a future allowlist or rename if it survives
    review.
    """
    scanned = 0
    flagged = 0
    class_decl_pattern = re.compile(r"\bclass\s+(\w+)")

    for path in sorted(effect_dir.rglob("*")):
        if not path.is_file():
            continue
        if path.suffix not in {".cpp", ".h"}:
            continue
        if _is_reference_path(path):
            continue
        scanned += 1

        # Filename check
        for pat in FRAGMENTATION_NAME_PATTERNS:
            if pat.search(path.name):
                warnings.append(
                    f"[fragmentation-warn] §3.3 multi-element fragmentation "
                    f"pattern in filename: {path.name} (matched: {pat.pattern}) "
                    f"— reviewer must classify PASS / KILL"
                )
                flagged += 1
                break

        # Class-name check
        text = read_text(path)
        for class_match in class_decl_pattern.finditer(text):
            class_name = class_match.group(1)
            for pat in FRAGMENTATION_NAME_PATTERNS:
                if pat.search(class_name):
                    warnings.append(
                        f"[fragmentation-warn] §3.3 multi-element fragmentation "
                        f"pattern in class name: {class_name} in {path.name} "
                        f"(matched: {pat.pattern}) — reviewer must classify PASS / KILL"
                    )
                    flagged += 1

    stats["fragmentation_scan_total"] = scanned
    stats["fragmentation_scan_flagged"] = flagged


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
    warnings: list[str] = []
    stats: dict = {}

    # Original checks (unchanged)
    check_centre_files(violations)
    check_no_rainbow_files(violations)
    check_raw_control_bus_usage(violations)
    check_heap_alloc_in_render(violations)
    check_ar_control_liveness(violations)

    # Inverted checks
    check_centre_origin_inverted(violations, stats)
    check_rainbow_inverted(violations, stats)
    check_k1_ap_only(violations, stats)

    # Brand-voice extensions (Phase 0A first enforcement primitive)
    check_tempo_bank_in_render(violations, stats)
    check_geo_kill_patterns(violations, stats)
    check_fragmentation_patterns(warnings, stats)

    # Count total effect files
    effect_file_count = len(list(effect_files()))

    if violations:
        print("FAIL: effect contract checks found issues:")
        for issue in violations:
            print(f"  - {issue}")
        print()

    if warnings:
        print("WARN: brand-voice boundary flags (review required, not blocking):")
        for warn in warnings:
            print(f"  - {warn}")
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
    print(f"  Tempo-bank scan:    {stats.get('tempo_bank_scan_total', 0)} .cpp files checked"
          f" | allowlist: {len(TEMPO_BANK_ALLOWLIST)} files"
          f" | flagged: {stats.get('tempo_bank_scan_flagged', 0)}")
    print(f"  GEO-kill scan:      {stats.get('geo_kill_scan_total', 0)} .cpp/.h files checked"
          f" | flagged: {stats.get('geo_kill_scan_flagged', 0)}")
    print(f"  Fragmentation warn: {stats.get('fragmentation_scan_total', 0)} .cpp/.h files checked"
          f" | flagged: {stats.get('fragmentation_scan_flagged', 0)}")

    if violations:
        return 1

    if warnings:
        print("\nPASS: all effect contract checks passed (with brand-voice warnings — see above).")
        return 0

    print("\nPASS: all effect contract checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
