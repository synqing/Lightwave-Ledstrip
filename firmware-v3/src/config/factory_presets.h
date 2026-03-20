/**
 * @file factory_presets.h
 * @brief Factory preset definitions for K1 Founders Edition (DEC-009)
 *
 * 8 curated presets, each bundling an effect, palette, and 7 expression
 * parameter defaults. Button cycling iterates these in fixed order.
 *
 * Button order: P1→P2→P3→P4→P5→P6→P7→P8→loop
 * Emotional arc: calm beauty → builds energy → peaks → resolves to minimal
 *
 * Expression defaults are proposals pending hands-on hardware tuning.
 *
 * @see DEC · Multi · Factory Preset Definitions (DEC-009)
 */

#pragma once

#include <cstdint>
#include "effect_ids.h"

namespace lightwaveos {

// ==================== Factory Preset Definition ====================

struct FactoryPreset {
    const char* name;           // User-facing name (Tier 0-A terminology)
    EffectId    effectId;       // Effect ID constant
    uint8_t     paletteIndex;   // Palette index (0-74)
    uint8_t     hue;            // Expression: Hue (0-255)
    uint8_t     saturation;     // Expression: Saturation (0-255)
    uint8_t     mood;           // Expression: Mood (0-255)
    uint8_t     trails;         // Expression: Trails (0-255)
    uint8_t     intensity;      // Expression: Intensity (0-255)
    uint8_t     complexity;     // Expression: Complexity (0-255)
    uint8_t     variation;      // Expression: Variation (0-255)
};

static constexpr uint8_t FACTORY_PRESET_COUNT = 8;

/**
 * DEC-009 locked preset table.
 *
 * | #  | Name     | Effect                        | Palette         | Role                  |
 * |----|----------|-------------------------------|-----------------|-----------------------|
 * | P1 | Prism    | LGP Holographic ES Tuned      | Sunset Real     | Signature / first     |
 * | P2 | Aurora   | LGP Aurora Borealis           | Tofino          | Calm ambient          |
 * | P3 | Current  | LGP Perlin Caustics           | GR65 Hult       | Lush melodic          |
 * | P4 | Ember    | Fire                          | Copper          | Textural organic      |
 * | P5 | Pulse    | Beat Pulse Resonant           | Inferno         | Beat kinetic          |
 * | P6 | Surge    | Ripple Enhanced               | Hot             | High-energy dramatic  |
 * | P7 | Harmonic | BPM Enhanced                  | Rainbow Sherbet | Chroma showcase       |
 * | P8 | Hush     | Sinelon                       | Nighttime       | Late-night minimal    |
 */
static constexpr FactoryPreset FACTORY_PRESETS[FACTORY_PRESET_COUNT] = {
    // P1: Prism — Signature / first impression
    { "Prism",    EID_LGP_HOLOGRAPHIC_ES_TUNED, 0,  128, 180, 128, 180, 140, 160, 100 },
    // P2: Aurora — Calm ambient
    { "Aurora",   EID_LGP_AURORA_BOREALIS,      55, 128, 160,  80, 220,  80, 100,  60 },
    // P3: Current — Lush melodic
    { "Current",  EID_LGP_PERLIN_CAUSTICS,      15, 128, 200, 140, 200, 128, 180, 140 },
    // P4: Ember — Textural organic
    { "Ember",    EID_FIRE,                      68,  64, 180, 100, 160, 128, 120, 100 },
    // P5: Pulse — Beat kinetic
    { "Pulse",    EID_BEAT_PULSE_RESONANT,       59, 128, 220, 160, 100, 200, 128,  80 },
    // P6: Surge — High-energy dramatic
    { "Surge",    EID_RIPPLE_ENHANCED,           69, 128, 240, 200, 140, 220, 180, 160 },
    // P7: Harmonic — Chroma showcase (DEC-006 required)
    { "Harmonic", EID_BPM_ENHANCED,              14, 128, 255, 128, 140, 180, 128, 100 },
    // P8: Hush — Late-night minimal
    { "Hush",     EID_SINELON,                   65, 128, 128,  60, 240,  60,  40,  40 },
};

// Boot default and WDT safe-mode recovery preset
static constexpr uint8_t FACTORY_PRESET_DEFAULT_INDEX = 0; // Prism

} // namespace lightwaveos
