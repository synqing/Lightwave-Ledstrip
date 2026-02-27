#!/usr/bin/env python3
"""
Generate effect_ids.h from inventory.json.

Maps each effect from its old sequential ID to a stable namespaced uint16_t ID.
Family block is determined by the registration-order grouping in CoreEffects.cpp.
"""

import json
import sys
from pathlib import Path
from collections import defaultdict

# Family block definitions based on CoreEffects.cpp registration order
FAMILY_BLOCKS = [
    (0x01, "CORE",               range(0, 13),   "Core / Classic"),
    (0x02, "INTERFERENCE",       range(13, 18),  "LGP Interference"),
    (0x03, "GEOMETRIC",          range(18, 26),  "LGP Geometric"),
    (0x04, "ADVANCED_OPTICAL",   range(26, 34),  "LGP Advanced Optical"),
    (0x05, "ORGANIC",            range(34, 40),  "LGP Organic"),
    (0x06, "QUANTUM",            range(40, 50),  "LGP Quantum"),
    (0x07, "COLOUR_MIXING",      range(50, 60),  "LGP Colour Mixing"),
    (0x08, "NOVEL_PHYSICS",      range(60, 65),  "LGP Novel Physics"),
    (0x09, "CHROMATIC",          range(65, 68),  "LGP Chromatic"),
    (0x0A, "AUDIO_REACTIVE",     range(68, 77),  "Audio Reactive"),
    (0x0B, "PERLIN_REACTIVE",    range(77, 81),  "Perlin Reactive"),
    (0x0C, "PERLIN_AMBIENT",     range(81, 85),  "Perlin Ambient"),
    (0x0D, "PERLIN_TEST",        range(85, 88),  "Perlin Backend Test"),
    (0x0E, "ENHANCED_AUDIO",     range(88, 99),  "Enhanced Audio-Reactive"),
    (0x0F, "DIAGNOSTIC",         range(99, 100), "Diagnostic"),
    (0x10, "AUTO_CYCLE",         range(100, 101),"Palette Auto-Cycle"),
    (0x11, "ES_REFERENCE",       range(101, 106),"ES v1.1 Reference"),
    (0x12, "ES_TUNED",           range(106, 109),"ES Tuned"),
    (0x13, "SB_REFERENCE",       range(109, 110),"SensoryBridge Reference"),
    (0x14, "BEAT_PULSE",         range(110, 122),"Beat Pulse Family"),
    (0x15, "TRANSPORT",          range(122, 124),"Transport / Parity"),
    (0x16, "HOLOGRAPHIC_VAR",    range(124, 134),"Holographic Variants"),
    (0x17, "REACTION_DIFFUSION", range(134, 136),"Reaction Diffusion"),
    (0x18, "SHAPE_BANGERS",      range(136, 147),"Shape Bangers Pack"),
    (0x19, "HOLY_SHIT_BANGERS",  range(147, 152),"Holy Shit Bangers Pack"),
    (0x1A, "EXPERIMENTAL_AUDIO", range(152, 162),"Experimental Audio Pack"),
]

def old_id_to_family(old_id):
    """Find which family block an old sequential ID belongs to."""
    for family_byte, family_name, id_range, _ in FAMILY_BLOCKS:
        if old_id in id_range:
            return family_byte, family_name
    return None, None

def make_constant_name(class_name, display_name):
    """Generate a C++ constant name from the effect class name."""
    # Remove Effect/Ref/Instance suffixes
    name = class_name
    if name.endswith("Effect"):
        name = name[:-6]
    if name.endswith("Ref"):
        name = name[:-3]

    # Split known consecutive abbreviations before CamelCase conversion
    # (e.g., LGPRGBPrism -> LGP_RGB_Prism, LGPDNAHelix -> LGP_DNA_Helix)
    KNOWN_ABBREVS = ['RGB', 'DNA', 'IFS', 'BPM', 'LGP', 'SB', 'ES']
    for abbr in sorted(KNOWN_ABBREVS, key=len, reverse=True):
        # Insert underscore between two adjacent abbreviations
        for other in KNOWN_ABBREVS:
            if other == abbr:
                continue
            name = name.replace(abbr + other, abbr + '_' + other)

    # Convert CamelCase to UPPER_SNAKE_CASE
    result = []
    for i, ch in enumerate(name):
        if ch.isupper() and i > 0:
            prev = name[i - 1]
            if prev == '_':
                pass  # already separated
            elif prev.islower() or prev.isdigit():
                result.append('_')
            elif i + 1 < len(name) and name[i + 1].islower():
                result.append('_')
        result.append(ch.upper())
    snake = ''.join(result)

    # Clean up double underscores and leading/trailing underscores
    while '__' in snake:
        snake = snake.replace('__', '_')
    snake = snake.strip('_')

    return f"EID_{snake}"


def generate(inventory_path):
    with open(inventory_path) as f:
        data = json.load(f)

    effects = data["effects"]
    removed_slots = set(data.get("removed_slots", []))

    # Track per-family sequence counters
    family_seq = defaultdict(int)

    # Build ID assignments
    assignments = []  # (old_id, new_id, const_name, display_name, family_name, class_name, removed)

    for effect in effects:
        old_id = effect["id"]
        class_name = effect["class_name"]
        display_name = effect["display_name"]
        removed = effect.get("removed", False)

        family_byte, family_name = old_id_to_family(old_id)
        if family_byte is None:
            print(f"WARNING: No family for old ID {old_id} ({class_name})", file=sys.stderr)
            continue

        if removed:
            # Retired slot -- still assign a family slot (retired IDs are never reused)
            seq = family_seq[family_byte]
            family_seq[family_byte] += 1
            new_id = (family_byte << 8) | seq
            const_name = f"EID_RETIRED_{old_id:03d}"
            assignments.append((old_id, new_id, const_name, display_name, family_name, class_name, True))
        else:
            seq = family_seq[family_byte]
            family_seq[family_byte] += 1
            new_id = (family_byte << 8) | seq
            const_name = make_constant_name(class_name, display_name)
            assignments.append((old_id, new_id, const_name, display_name, family_name, class_name, False))

    # Print header
    print("""/**
 * @file effect_ids.h
 * @brief Stable namespaced effect IDs -- single source of truth
 *
 * Each effect has a uint16_t ID assigned once at creation and NEVER changed.
 * The high byte encodes family membership, the low byte is the sequence
 * within that family. IDs are never reused, even when effects are removed.
 *
 * Structure: [FAMILY : 8 bits][SEQUENCE : 8 bits]
 *
 * This file is auto-generated from inventory.json by gen_effect_ids.py.
 * Do not edit manually -- regenerate when adding new effects.
 *
 * @author LightwaveOS Team
 * @version 1.0.0
 */

#pragma once

#include <stdint.h>

namespace lightwaveos {

// ============================================================================
// Type Alias
// ============================================================================

using EffectId = uint16_t;
constexpr EffectId INVALID_EFFECT_ID = 0xFFFF;

// ============================================================================
// Family Block Constants
// ============================================================================
""")

    for family_byte, family_name, _, description in FAMILY_BLOCKS:
        print(f"constexpr uint8_t FAMILY_{family_name} = 0x{family_byte:02X};  // {description}")

    print("""
// ============================================================================
// Reserved Family Blocks
// ============================================================================

constexpr uint8_t FAMILY_RESERVED_START = 0x1B;  // 0x1B-0xEF reserved for expansion
constexpr uint8_t FAMILY_OTA_USER       = 0xF0;  // OTA-provisioned / user-uploaded effects

// ============================================================================
// Effect ID Constants
// ============================================================================
// Format: EID_<EFFECT_NAME> = 0xFFSS
//   FF = family byte, SS = sequence within family
//
// These constants are the SINGLE SOURCE OF TRUTH for effect identity.
// They are declared in effect class headers as:
//   static constexpr EffectId kId = EID_FIRE;
// ============================================================================
""")

    current_family = None
    for old_id, new_id, const_name, display_name, family_name, class_name, removed in assignments:
        if family_name != current_family:
            if current_family is not None:
                print()
            # Find the description for this family
            desc = ""
            for _, fn, _, d in FAMILY_BLOCKS:
                if fn == family_name:
                    desc = d
                    break
            print(f"// --- {desc} (0x{new_id >> 8:02X}xx) ---")
            current_family = family_name

        retired_comment = " // RETIRED" if removed else ""
        pad = 40 - len(const_name)
        if pad < 1:
            pad = 1
        print(f"constexpr EffectId {const_name}{' ' * pad}= 0x{new_id:04X};  // old {old_id:3d}: {display_name}{retired_comment}")

    # Count active effects
    active_count = sum(1 for a in assignments if not a[6])

    print(f"""
// Total: {len(assignments)} IDs assigned ({active_count} active, {len(assignments) - active_count} retired)

// ============================================================================
// Migration: Old Sequential ID -> New Stable ID
// ============================================================================
// Used during NVS persistence migration (version 1 -> version 2).
// Index = old uint8_t effect ID, value = new EffectId.
// Retired slots map to INVALID_EFFECT_ID.
// ============================================================================

constexpr EffectId OLD_TO_NEW[{len(assignments)}] = {{""")

    for i, (old_id, new_id, const_name, _, _, _, removed) in enumerate(assignments):
        comma = "," if i < len(assignments) - 1 else ""
        if removed:
            print(f"    INVALID_EFFECT_ID{comma}  // old {old_id:3d}: RETIRED")
        else:
            pad = 40 - len(const_name)
            if pad < 1:
                pad = 1
            print(f"    {const_name}{comma}{' ' * pad}// old {old_id:3d}")

    print("""};

constexpr uint16_t OLD_ID_COUNT = sizeof(OLD_TO_NEW) / sizeof(EffectId);

// ============================================================================
// Migration Helpers
// ============================================================================

/**
 * @brief Convert old sequential uint8_t ID to new stable EffectId
 * @param oldId Old sequential effect ID (0-161)
 * @return New EffectId, or INVALID_EFFECT_ID if out of range or retired
 */
inline EffectId oldIdToNew(uint8_t oldId) {
    if (oldId >= OLD_ID_COUNT) return INVALID_EFFECT_ID;
    return OLD_TO_NEW[oldId];
}

/**
 * @brief Find old sequential index for a new EffectId (reverse lookup)
 * @param newId New stable EffectId
 * @return Old sequential index (0-161), or 0xFF if not found
 *
 * Linear scan -- only used during migration, not in hot paths.
 */
inline uint8_t newIdToOldIndex(EffectId newId) {
    for (uint16_t i = 0; i < OLD_ID_COUNT; i++) {
        if (OLD_TO_NEW[i] == newId) return static_cast<uint8_t>(i);
    }
    return 0xFF;
}

/**
 * @brief Extract family byte from an EffectId
 * @param id Effect ID
 * @return Family byte (high 8 bits)
 */
inline uint8_t effectFamily(EffectId id) {
    return static_cast<uint8_t>(id >> 8);
}

/**
 * @brief Extract sequence number from an EffectId
 * @param id Effect ID
 * @return Sequence within family (low 8 bits)
 */
inline uint8_t effectSequence(EffectId id) {
    return static_cast<uint8_t>(id & 0xFF);
}

} // namespace lightwaveos""")


if __name__ == "__main__":
    inventory = Path(__file__).resolve().parent.parent.parent.parent.parent / \
        ".claude/orchestration/effects-docs/output/inventory.json"
    if len(sys.argv) > 1:
        inventory = Path(sys.argv[1])
    generate(inventory)
