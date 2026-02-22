#!/usr/bin/env python3
"""Fix remaining effect headers that were skipped by add_kid.py.

Handles:
1. Classes with 'final' keyword: class XXXEffect final : public ...
2. Pack headers with multiple classes per file
"""
import re
import os

BASE = os.path.dirname(os.path.abspath(__file__))
IEFFECT_DIR = os.path.join(BASE, "src/effects/ieffect")
ESREF_DIR = os.path.join(IEFFECT_DIR, "esv11_reference")
SBREF_DIR = os.path.join(IEFFECT_DIR, "sensorybridge_reference")

# Same mapping as add_kid.py
CLASS_TO_EID = {
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
    "WaveformParityEffect": "EID_LGP_OPAL_FILM",
    "HeartbeatEsTunedEffect": "EID_HEARTBEAT_ES_TUNED",
    "LGPHolographicEsTunedEffect": "EID_LGP_HOLOGRAPHIC_ES_TUNED",
    "SbWaveform310RefEffect": "EID_SB_WAVEFORM310",
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
    config_path = os.path.join(BASE, "src/config/effect_ids.h")
    file_dir = os.path.dirname(filepath)
    return os.path.relpath(config_path, file_dir).replace(os.sep, "/")


def process_file(filepath):
    """Process a file that may have one or more effect classes needing kId."""
    with open(filepath, 'r') as f:
        content = f.read()

    # Find ALL effect classes (with or without 'final')
    # Pattern: class XXXEffect [final] : public ...
    class_pattern = re.compile(r'class\s+(\w+Effect)\s+(?:final\s+)?:\s+public')
    matches = list(class_pattern.finditer(content))

    if not matches:
        return 0, "no effect classes found"

    # Filter to classes that need kId and are in our mapping
    classes_to_add = []
    for m in matches:
        class_name = m.group(1)
        if class_name not in CLASS_TO_EID:
            continue
        # Check if this specific class already has kId
        # Look between this class def and the next class def (or end of file)
        class_start = m.start()
        # Find next class or end of file
        next_match = class_pattern.search(content, m.end())
        class_end = next_match.start() if next_match else len(content)
        class_block = content[class_start:class_end]
        if 'kId' in class_block:
            continue
        classes_to_add.append((class_name, CLASS_TO_EID[class_name], m))

    if not classes_to_add:
        return 0, "all classes already have kId"

    # Add include if not present
    include_path = compute_include_path(filepath)
    include_line = f'#include "{include_path}"'
    needs_include = include_line not in content and 'effect_ids.h' not in content

    if needs_include:
        # Find last #include
        last_include = -1
        for m_inc in re.finditer(r'^#include\s+.*$', content, re.MULTILINE):
            last_include = m_inc.end()
        if last_include >= 0:
            content = content[:last_include] + '\n' + include_line + content[last_include:]
        else:
            # After #pragma once
            pragma = content.find('#pragma once')
            if pragma >= 0:
                nl = content.find('\n', pragma)
                content = content[:nl+1] + '\n' + include_line + '\n' + content[nl+1:]

    # Now add kId to each class, working from bottom to top to preserve positions
    # Re-find matches after include insertion
    matches = list(class_pattern.finditer(content))
    insertions = []  # (position, kId_line)

    for m in matches:
        class_name = m.group(1)
        if class_name not in CLASS_TO_EID:
            continue
        eid = CLASS_TO_EID[class_name]

        # Check if already has kId in this class block
        next_match = class_pattern.search(content, m.end())
        class_end = next_match.start() if next_match else len(content)
        class_block = content[m.start():class_end]
        if 'kId' in class_block:
            continue

        # Find "public:" after this class declaration
        public_pos = content.find('public:', m.start())
        if public_pos is None or public_pos < 0:
            continue
        if next_match and public_pos >= next_match.start():
            continue  # public: belongs to next class

        nl_pos = content.find('\n', public_pos)
        if nl_pos < 0:
            continue

        kId_line = f"    static constexpr lightwaveos::EffectId kId = lightwaveos::{eid};\n"
        insertions.append((nl_pos + 1, kId_line, class_name, eid))

    # Apply insertions from bottom to top
    insertions.sort(key=lambda x: x[0], reverse=True)
    for pos, line, _, _ in insertions:
        content = content[:pos] + line + content[pos:]

    with open(filepath, 'w') as f:
        f.write(content)

    names = [(cn, eid) for _, _, cn, eid in insertions]
    return len(insertions), names


def main():
    # Collect all header files
    headers = []
    for dirpath in [IEFFECT_DIR, ESREF_DIR, SBREF_DIR]:
        if not os.path.isdir(dirpath):
            continue
        for fname in sorted(os.listdir(dirpath)):
            if fname.endswith('.h'):
                headers.append(os.path.join(dirpath, fname))

    modified_files = 0
    modified_classes = 0
    skipped = 0

    for filepath in headers:
        fname = os.path.basename(filepath)
        count, info = process_file(filepath)
        if count > 0:
            print(f"  OK  {fname}: {count} class(es)")
            for cn, eid in info:
                print(f"       {cn} -> {eid}")
            modified_files += 1
            modified_classes += count
        else:
            skipped += 1

    print(f"\nDone: {modified_files} files, {modified_classes} classes modified, {skipped} files skipped")


if __name__ == "__main__":
    main()
