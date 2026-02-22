#!/usr/bin/env python3
"""Add static constexpr EffectId kId to all effect headers.

Reads the mapping from effect_ids.h comments and applies to each header file.
"""
import re
import os
import sys

# Base directory
BASE = os.path.dirname(os.path.abspath(__file__))
IEFFECT_DIR = os.path.join(BASE, "src/effects/ieffect")
ESREF_DIR = os.path.join(IEFFECT_DIR, "esv11_reference")
SBREF_DIR = os.path.join(IEFFECT_DIR, "sensorybridge_reference")

# Mapping from class name -> EID constant
# Derived from CoreEffects.cpp registrations and effect_ids.h
CLASS_TO_EID = {
    "FireEffect": "EID_FIRE",
    "OceanEffect": "EID_OCEAN",
    "PlasmaEffect": "EID_PLASMA",
    "ConfettiEffect": "EID_CONFETTI",
    "SinelonEffect": "EID_SINELON",
    "JuggleEffect": "EID_JUGGLE",
    "BPMEffect": "EID_BPM",
    "WaveAmbientEffect": "EID_WAVE_AMBIENT",
    "WaveEffect": "EID_WAVE_AMBIENT",  # WaveEffect is the base, WaveAmbient is the registered one
    "RippleEffect": "EID_RIPPLE",
    "HeartbeatEffect": "EID_HEARTBEAT",
    "InterferenceEffect": "EID_INTERFERENCE",
    "BreathingEffect": "EID_BREATHING",
    "PulseEffect": "EID_PULSE",
    "LGPBoxWaveEffect": "EID_LGP_BOX_WAVE",
    "LGPHolographicEffect": "EID_LGP_HOLOGRAPHIC",
    "ModalResonanceEffect": "EID_MODAL_RESONANCE",
    "LGPInterferenceScannerEffect": "EID_LGP_INTERFERENCE_SCANNER",
    "LGPWaveCollisionEffect": "EID_LGP_WAVE_COLLISION",
    "LGPDiamondLatticeEffect": "EID_LGP_DIAMOND_LATTICE",
    "LGPHexagonalGridEffect": "EID_LGP_HEXAGONAL_GRID",
    "LGPSpiralVortexEffect": "EID_LGP_SPIRAL_VORTEX",
    "LGPSierpinskiEffect": "EID_LGP_SIERPINSKI",
    "ChevronWavesEffect": "EID_CHEVRON_WAVES",
    "LGPConcentricRingsEffect": "EID_LGP_CONCENTRIC_RINGS",
    "LGPStarBurstEffect": "EID_LGP_STAR_BURST",
    "LGPMeshNetworkEffect": "EID_LGP_MESH_NETWORK",
    "LGPMoireCurtainsEffect": "EID_LGP_MOIRE_CURTAINS",
    "LGPRadialRippleEffect": "EID_LGP_RADIAL_RIPPLE",
    "LGPHolographicVortexEffect": "EID_LGP_HOLOGRAPHIC_VORTEX",
    "LGPEvanescentDriftEffect": "EID_LGP_EVANESCENT_DRIFT",
    "LGPChromaticShearEffect": "EID_LGP_CHROMATIC_SHEAR",
    "LGPModalCavityEffect": "EID_LGP_MODAL_CAVITY",
    "LGPFresnelZonesEffect": "EID_LGP_FRESNEL_ZONES",
    "LGPPhotonicCrystalEffect": "EID_LGP_PHOTONIC_CRYSTAL",
    "LGPAuroraBorealisEffect": "EID_LGP_AURORA_BOREALIS",
    "LGPBioluminescentWavesEffect": "EID_LGP_BIOLUMINESCENT_WAVES",
    "LGPPlasmaMembraneEffect": "EID_LGP_PLASMA_MEMBRANE",
    "LGPNeuralNetworkEffect": "EID_LGP_NEURAL_NETWORK",
    "LGPCrystallineGrowthEffect": "EID_LGP_CRYSTALLINE_GROWTH",
    "LGPFluidDynamicsEffect": "EID_LGP_FLUID_DYNAMICS",
    "LGPQuantumTunnelingEffect": "EID_LGP_QUANTUM_TUNNELING",
    "LGPGravitationalLensingEffect": "EID_LGP_GRAVITATIONAL_LENSING",
    "LGPTimeCrystalEffect": "EID_LGP_TIME_CRYSTAL",
    "LGPSolitonWavesEffect": "EID_LGP_SOLITON_WAVES",
    "LGPMetamaterialCloakEffect": "EID_LGP_METAMATERIAL_CLOAK",
    "LGPGrinCloakEffect": "EID_LGP_GRIN_CLOAK",
    "LGPCausticFanEffect": "EID_LGP_CAUSTIC_FAN",
    "LGPBirefringentShearEffect": "EID_LGP_BIREFRINGENT_SHEAR",
    "LGPAnisotropicCloakEffect": "EID_LGP_ANISOTROPIC_CLOAK",
    "LGPEvanescentSkinEffect": "EID_LGP_EVANESCENT_SKIN",
    "LGPColorTemperatureEffect": "EID_LGP_COLOR_TEMPERATURE",
    "LGPRGBPrismEffect": "EID_LGP_RGB_PRISM",
    "LGPComplementaryMixingEffect": "EID_LGP_COMPLEMENTARY_MIXING",
    "LGPQuantumColorsEffect": "EID_LGP_QUANTUM_COLORS",
    "LGPDopplerShiftEffect": "EID_LGP_DOPPLER_SHIFT",
    "LGPColorAcceleratorEffect": "EID_LGP_COLOR_ACCELERATOR",
    "LGPDNAHelixEffect": "EID_LGP_DNA_HELIX",
    "LGPPhaseTransitionEffect": "EID_LGP_PHASE_TRANSITION",
    "LGPChromaticAberrationEffect": "EID_LGP_CHROMATIC_ABERRATION",
    "LGPPerceptualBlendEffect": "EID_LGP_PERCEPTUAL_BLEND",
    "LGPChladniHarmonicsEffect": "EID_LGP_CHLADNI_HARMONICS",
    "LGPGravitationalWaveChirpEffect": "EID_LGP_GRAVITATIONAL_WAVE_CHIRP",
    "LGPQuantumEntanglementEffect": "EID_LGP_QUANTUM_ENTANGLEMENT",
    "LGPMycelialNetworkEffect": "EID_LGP_MYCELIAL_NETWORK",
    "LGPRileyDissonanceEffect": "EID_LGP_RILEY_DISSONANCE",
    "LGPChromaticLensEffect": "EID_LGP_CHROMATIC_LENS",
    "LGPChromaticPulseEffect": "EID_LGP_CHROMATIC_PULSE",
    "ChromaticInterferenceEffect": "EID_CHROMATIC_INTERFERENCE",
    "LGPAudioTestEffect": "EID_LGP_AUDIO_TEST",
    "LGPBeatPulseEffect": "EID_LGP_BEAT_PULSE",
    "LGPSpectrumBarsEffect": "EID_LGP_SPECTRUM_BARS",
    "LGPBassBreathEffect": "EID_LGP_BASS_BREATH",
    "AudioWaveformEffect": "EID_AUDIO_WAVEFORM",
    "AudioBloomEffect": "EID_AUDIO_BLOOM",
    "LGPStarBurstNarrativeEffect": "EID_LGP_STAR_BURST_NARRATIVE",
    "LGPChordGlowEffect": "EID_LGP_CHORD_GLOW",
    "WaveReactiveEffect": "EID_WAVE_REACTIVE",
    "LGPPerlinVeilEffect": "EID_LGP_PERLIN_VEIL",
    "LGPPerlinShocklinesEffect": "EID_LGP_PERLIN_SHOCKLINES",
    "LGPPerlinCausticsEffect": "EID_LGP_PERLIN_CAUSTICS",
    "LGPPerlinInterferenceWeaveEffect": "EID_LGP_PERLIN_INTERFERENCE_WEAVE",
    "LGPPerlinVeilAmbientEffect": "EID_LGP_PERLIN_VEIL_AMBIENT",
    "LGPPerlinShocklinesAmbientEffect": "EID_LGP_PERLIN_SHOCKLINES_AMBIENT",
    "LGPPerlinCausticsAmbientEffect": "EID_LGP_PERLIN_CAUSTICS_AMBIENT",
    "LGPPerlinInterferenceWeaveAmbientEffect": "EID_LGP_PERLIN_INTERFERENCE_WEAVE_AMBIENT",
    "LGPPerlinBackendFastLEDEffect": "EID_LGP_PERLIN_BACKEND_FAST_LED",
    "LGPPerlinBackendEmotiscopeFullEffect": "EID_LGP_PERLIN_BACKEND_EMOTISCOPE_FULL",
    "LGPPerlinBackendEmotiscopeQuarterEffect": "EID_LGP_PERLIN_BACKEND_EMOTISCOPE_QUARTER",
    "BPMEnhancedEffect": "EID_BPM_ENHANCED",
    "BreathingEnhancedEffect": "EID_BREATHING_ENHANCED",
    "ChevronWavesEnhancedEffect": "EID_CHEVRON_WAVES_ENHANCED",
    "LGPInterferenceScannerEnhancedEffect": "EID_LGP_INTERFERENCE_SCANNER_ENHANCED",
    "LGPPhotonicCrystalEnhancedEffect": "EID_LGP_PHOTONIC_CRYSTAL_ENHANCED",
    "LGPSpectrumDetailEffect": "EID_LGP_SPECTRUM_DETAIL",
    "LGPSpectrumDetailEnhancedEffect": "EID_LGP_SPECTRUM_DETAIL_ENHANCED",
    "LGPStarBurstEnhancedEffect": "EID_LGP_STAR_BURST_ENHANCED",
    "LGPWaveCollisionEnhancedEffect": "EID_LGP_WAVE_COLLISION_ENHANCED",
    "RippleEnhancedEffect": "EID_RIPPLE_ENHANCED",
    "SnapwaveLinearEffect": "EID_SNAPWAVE_LINEAR",
    "TrinityTestEffect": "EID_TRINITY_TEST",
    "LGPHolographicAutoCycleEffect": "EID_LGP_HOLOGRAPHIC_AUTO_CYCLE",
    "EsAnalogRefEffect": "EID_ES_ANALOG",
    "EsSpectrumRefEffect": "EID_ES_SPECTRUM",
    "EsOctaveRefEffect": "EID_ES_OCTAVE",
    "EsBloomRefEffect": "EID_ES_BLOOM",
    "EsWaveformRefEffect": "EID_ES_WAVEFORM",
    "RippleEsTunedEffect": "EID_RIPPLE_ES_TUNED",
    "HeartbeatEsTunedEffect": "EID_HEARTBEAT_ES_TUNED",
    "LGPHolographicEsTunedEffect": "EID_LGP_HOLOGRAPHIC_ES_TUNED",
    "SbWaveform310RefEffect": "EID_SB_WAVEFORM310",
    "BeatPulseStackEffect": "EID_BEAT_PULSE_STACK",
    "BeatPulseShockwaveEffect": "EID_BEAT_PULSE_SHOCKWAVE",
    "BeatPulseVoidEffect": "EID_BEAT_PULSE_VOID",
    "BeatPulseResonantEffect": "EID_BEAT_PULSE_RESONANT",
    "BeatPulseRippleEffect": "EID_BEAT_PULSE_RIPPLE",
    "BeatPulseShockwaveCascadeEffect": "EID_BEAT_PULSE_SHOCKWAVE_CASCADE",
    "BeatPulseSpectralEffect": "EID_BEAT_PULSE_SPECTRAL",
    "BeatPulseSpectralPulseEffect": "EID_BEAT_PULSE_SPECTRAL_PULSE",
    "BeatPulseBreatheEffect": "EID_BEAT_PULSE_BREATHE",
    "BeatPulseLGPInterferenceEffect": "EID_BEAT_PULSE_LGP_INTERFERENCE",
    "BeatPulseBloomEffect": "EID_BEAT_PULSE_BLOOM",
    "BloomParityEffect": "EID_BLOOM_PARITY",
    "KuramotoTransportEffect": "EID_KURAMOTO_TRANSPORT",
    "WaveformParityEffect": "EID_LGP_OPAL_FILM",  # maps to old 124 slot
    "LGPOpalFilmEffect": "EID_LGP_OPAL_FILM",
    "LGPGratingScanEffect": "EID_LGP_GRATING_SCAN",
    "LGPStressGlassEffect": "EID_LGP_STRESS_GLASS",
    "LGPMoireSilkEffect": "EID_LGP_MOIRE_SILK",
    "LGPCausticShardsEffect": "EID_LGP_CAUSTIC_SHARDS",
    "LGPParallaxDepthEffect": "EID_LGP_PARALLAX_DEPTH",
    "LGPStressGlassMeltEffect": "EID_LGP_STRESS_GLASS_MELT",
    "LGPGratingScanBreakupEffect": "EID_LGP_GRATING_SCAN_BREAKUP",
    "LGPWaterCausticsEffect": "EID_LGP_WATER_CAUSTICS",
    "LGPSchlierenFlowEffect": "EID_LGP_SCHLIEREN_FLOW",
    "LGPReactionDiffusionEffect": "EID_LGP_REACTION_DIFFUSION",
    "LGPReactionDiffusionTriangleEffect": "EID_LGP_REACTION_DIFFUSION_TRIANGLE",
    # Shape Bangers Pack
    "LGPTalbotCarpetEffect": "EID_LGP_TALBOT_CARPET",
    "LGPAiryCometEffect": "EID_LGP_AIRY_COMET",
    "LGPMoireCathedralEffect": "EID_LGP_MOIRE_CATHEDRAL",
    "LGPSuperformulaGlyphEffect": "EID_LGP_SUPERFORMULA_GLYPH",
    "LGPSpirographCrownEffect": "EID_LGP_SPIROGRAPH_CROWN",
    "LGPRoseBloomEffect": "EID_LGP_ROSE_BLOOM",
    "LGPHarmonographHaloEffect": "EID_LGP_HARMONOGRAPH_HALO",
    "LGPRule30CathedralEffect": "EID_LGP_RULE30_CATHEDRAL",
    "LGPLangtonHighwayEffect": "EID_LGP_LANGTON_HIGHWAY",
    "LGPCymaticLadderEffect": "EID_LGP_CYMATIC_LADDER",
    "LGPMachDiamondsEffect": "EID_LGP_MACH_DIAMONDS",
    # Holy Shit Bangers
    "LGPChimeraCrownEffect": "EID_LGP_CHIMERA_CROWN",
    "LGPCatastropheCausticsEffect": "EID_LGP_CATASTROPHE_CAUSTICS",
    "LGPHyperbolicPortalEffect": "EID_LGP_HYPERBOLIC_PORTAL",
    "LGPLorenzRibbonEffect": "EID_LGP_LORENZ_RIBBON",
    "LGPIFSBioRelicEffect": "EID_LGP_IFS_BIO_RELIC",
    # Experimental Audio Pack
    "LGPFluxRiftEffect": "EID_LGP_FLUX_RIFT",
    "LGPBeatPrismEffect": "EID_LGP_BEAT_PRISM",
    "LGPHarmonicTideEffect": "EID_LGP_HARMONIC_TIDE",
    "LGPBassQuakeEffect": "EID_LGP_BASS_QUAKE",
    "LGPTrebleNetEffect": "EID_LGP_TREBLE_NET",
    "LGPRhythmicGateEffect": "EID_LGP_RHYTHMIC_GATE",
    "LGPSpectralKnotEffect": "EID_LGP_SPECTRAL_KNOT",
    "LGPSaliencyBloomEffect": "EID_LGP_SALIENCY_BLOOM",
    "LGPTransientLatticeEffect": "EID_LGP_TRANSIENT_LATTICE",
    "LGPWaveletMirrorEffect": "EID_LGP_WAVELET_MIRROR",
}

def compute_include_path(filepath):
    """Compute relative include path to effect_ids.h from a header file."""
    rel = os.path.relpath(filepath, BASE)
    # Count directory depth from src/effects/ieffect/ to src/config/
    parts = rel.split(os.sep)
    # e.g. src/effects/ieffect/FireEffect.h -> 3 dirs up to src, then config/
    # or src/effects/ieffect/esv11_reference/EsAnalogRefEffect.h -> 4 dirs up
    depth = len(parts) - 1  # number of directory components
    ups = "../" * depth
    # From src/effects/ieffect/ -> ../../config/effect_ids.h (depth=3, ups=../../../ -> wrong)
    # Actually: file is at src/effects/ieffect/X.h
    # config is at src/config/effect_ids.h
    # Relative: ../../config/effect_ids.h
    config_path = os.path.join(BASE, "src/config/effect_ids.h")
    file_dir = os.path.dirname(filepath)
    return os.path.relpath(config_path, file_dir).replace(os.sep, "/")


def process_header(filepath):
    """Add kId to a single effect header file."""
    with open(filepath, 'r') as f:
        content = f.read()

    # Already has kId?
    if 'kId' in content:
        return None, "already has kId"

    # Find class name pattern: class XXXEffect : public ...IEffect
    class_match = re.search(r'class\s+(\w+Effect)\s*:', content)
    if not class_match:
        return None, "no effect class found"

    class_name = class_match.group(1)

    if class_name not in CLASS_TO_EID:
        return None, f"class {class_name} not in mapping"

    eid = CLASS_TO_EID[class_name]
    include_path = compute_include_path(filepath)
    include_line = f'#include "{include_path}"'

    # Step 1: Add include after #pragma once or after last existing #include
    lines = content.split('\n')
    new_lines = []
    include_added = False

    # Find the best place to add the include
    # Strategy: add after the last #include before the namespace block
    last_include_idx = -1
    for i, line in enumerate(lines):
        if line.startswith('#include'):
            last_include_idx = i

    for i, line in enumerate(lines):
        new_lines.append(line)
        if i == last_include_idx and not include_added:
            new_lines.append(include_line)
            include_added = True

    if not include_added:
        # Fallback: add after #pragma once
        new_lines2 = []
        for line in new_lines:
            new_lines2.append(line)
            if line.strip() == '#pragma once':
                new_lines2.append('')
                new_lines2.append(include_line)
                include_added = True
        new_lines = new_lines2

    content = '\n'.join(new_lines)

    # Step 2: Add kId line after "public:" in the class
    # Pattern: find "public:" after the class declaration, add kId before the constructor
    # Look for: public:\n    ClassName(
    pattern = rf'(class\s+{re.escape(class_name)}\s*:[^{{]*\{{[^}}]*?)(public:\s*\n)'
    match = re.search(pattern, content, re.DOTALL)

    if match:
        insert_pos = match.end()
        kId_line = f"    static constexpr lightwaveos::EffectId kId = lightwaveos::{eid};\n\n"
        content = content[:insert_pos] + kId_line + content[insert_pos:]
    else:
        # Try simpler pattern: just after "public:\n" in the class
        # Find the class, then find "public:"
        class_pos = content.find(f"class {class_name}")
        if class_pos >= 0:
            public_pos = content.find("public:", class_pos)
            if public_pos >= 0:
                # Find the newline after "public:"
                nl_pos = content.find('\n', public_pos)
                if nl_pos >= 0:
                    kId_line = f"    static constexpr lightwaveos::EffectId kId = lightwaveos::{eid};\n\n"
                    content = content[:nl_pos+1] + kId_line + content[nl_pos+1:]
                else:
                    return None, "couldn't find newline after public:"
            else:
                return None, "couldn't find public: in class"
        else:
            return None, "couldn't find class position"

    with open(filepath, 'w') as f:
        f.write(content)

    return class_name, eid


def main():
    # Collect all header files
    headers = []
    for dirpath in [IEFFECT_DIR, ESREF_DIR, SBREF_DIR]:
        if not os.path.isdir(dirpath):
            continue
        for fname in sorted(os.listdir(dirpath)):
            if fname.endswith('.h'):
                headers.append(os.path.join(dirpath, fname))

    # Also check for pack headers
    pack_dirs = [
        os.path.join(IEFFECT_DIR),
    ]

    modified = 0
    skipped = 0
    errors = 0

    for filepath in headers:
        fname = os.path.basename(filepath)
        result, info = process_header(filepath)
        if result:
            print(f"  OK  {fname}: {result} -> {info}")
            modified += 1
        else:
            if "already has kId" in info:
                print(f"  SKIP {fname}: {info}")
                skipped += 1
            elif "not in mapping" in info:
                print(f"  MISS {fname}: {info}")
                skipped += 1
            elif "no effect class" in info:
                print(f"  SKIP {fname}: {info} (utility header)")
                skipped += 1
            else:
                print(f"  ERR  {fname}: {info}")
                errors += 1

    print(f"\nDone: {modified} modified, {skipped} skipped, {errors} errors")


if __name__ == "__main__":
    main()
