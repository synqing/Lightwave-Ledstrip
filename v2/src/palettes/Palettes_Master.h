/**
 * @file Palettes_Master.h
 * @brief Master Palette Collection for LightwaveOS v2
 *
 * 75 unique gradient palettes from three collections:
 *
 * CONTENTS:
 *   - 33 cpt-city palettes (artistic gradients) [0-32]
 *   - 24 Crameri palettes (perceptually uniform, CVD-friendly) [33-56]
 *   - 18 R Colorspace palettes (viridis family, LGP-optimized) [57-74]
 *
 * USAGE:
 *   #include "palettes/Palettes_Master.h"
 *   // Access palettes via gMasterPalettes[0..74]
 *   // Access names via MasterPaletteNames[0..74]
 *   // Access metadata via master_palette_flags[], master_palette_avg_Y[], etc.
 */

#pragma once
#ifndef LIGHTWAVEOS_PALETTES_MASTER_H
#define LIGHTWAVEOS_PALETTES_MASTER_H

#include <stdint.h>
#include <FastLED.h>

// Include source palette definitions
#include "Palettes.h"              // cpt-city extern declarations
#include "CrameriPalettes.h"       // Crameri palette definitions
#include "ColorspacePalettes.h"    // R colorspace palettes (viridis, plasma, etc.)

namespace lightwaveos {
namespace palettes {

// =============================================================================
// PALETTE FLAG DEFINITIONS
// =============================================================================
// Bit flags for palette characteristics
constexpr uint8_t PAL_WARM        = 0x01;  // Warm tones (reds, oranges, yellows)
constexpr uint8_t PAL_COOL        = 0x02;  // Cool tones (blues, greens, purples)
constexpr uint8_t PAL_HIGH_SAT    = 0x04;  // High saturation
constexpr uint8_t PAL_WHITE_HEAVY = 0x08;  // Contains significant white/bright regions
constexpr uint8_t PAL_CALM        = 0x10;  // Subtle, calm transitions
constexpr uint8_t PAL_VIVID       = 0x20;  // Vivid, high-contrast transitions
constexpr uint8_t PAL_CVD_FRIENDLY = 0x40; // Colorblind-safe (Crameri/Colorspace)
constexpr uint8_t PAL_EXCLUDED    = 0x80;  // Exclude from random selection (grayscale, pure white)

// =============================================================================
// PALETTE CATEGORY RANGES
// =============================================================================
constexpr uint8_t CPT_CITY_START     = 0;
constexpr uint8_t CPT_CITY_END       = 32;
constexpr uint8_t CRAMERI_START      = 33;
constexpr uint8_t CRAMERI_END        = 56;
constexpr uint8_t COLORSPACE_START   = 57;
constexpr uint8_t COLORSPACE_END     = 74;

// =============================================================================
// MASTER PALETTE ARRAY - 75 UNIQUE PALETTES
// =============================================================================
// Order: cpt-city palettes (0-32), Crameri palettes (33-56), Colorspace (57-74)

extern const TProgmemRGBGradientPaletteRef gMasterPalettes[];

// Palette count (75 total: 33 cpt-city + 24 Crameri + 18 Colorspace)
constexpr uint8_t MASTER_PALETTE_COUNT = 75;

// =============================================================================
// PALETTE NAMES
// =============================================================================

extern const char* const MasterPaletteNames[];

// =============================================================================
// PALETTE METADATA
// =============================================================================
// Flags: PAL_WARM=1, PAL_COOL=2, PAL_HIGH_SAT=4, PAL_WHITE_HEAVY=8, PAL_CALM=16, PAL_VIVID=32, PAL_CVD_FRIENDLY=64

extern const uint8_t master_palette_flags[];
extern const uint8_t master_palette_avg_Y[];
extern const uint8_t master_palette_max_brightness[];

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

// Check if palette has specific flag
inline bool paletteHasFlag(uint8_t paletteIndex, uint8_t flag) {
    if (paletteIndex >= MASTER_PALETTE_COUNT) return false;
    return (master_palette_flags[paletteIndex] & flag) != 0;
}

// Check if palette is warm
inline bool isPaletteWarm(uint8_t paletteIndex) {
    return paletteHasFlag(paletteIndex, PAL_WARM);
}

// Check if palette is cool
inline bool isPaletteCool(uint8_t paletteIndex) {
    return paletteHasFlag(paletteIndex, PAL_COOL);
}

// Check if palette is calm (good for ambient lighting)
inline bool isPaletteCalm(uint8_t paletteIndex) {
    return paletteHasFlag(paletteIndex, PAL_CALM);
}

// Check if palette is vivid (good for attention-grabbing effects)
inline bool isPaletteVivid(uint8_t paletteIndex) {
    return paletteHasFlag(paletteIndex, PAL_VIVID);
}

// Check if palette is CVD-friendly (colorblind-safe)
inline bool isPaletteCVDFriendly(uint8_t paletteIndex) {
    return paletteHasFlag(paletteIndex, PAL_CVD_FRIENDLY);
}

// Get brightness-adjusted max for a palette (for power management)
inline uint8_t getPaletteMaxBrightness(uint8_t paletteIndex) {
    if (paletteIndex >= MASTER_PALETTE_COUNT) return 255;
    return master_palette_max_brightness[paletteIndex];
}

// Check if index is a Crameri (scientific) palette (33-56)
inline bool isCrameriPalette(uint8_t paletteIndex) {
    return paletteIndex >= CRAMERI_START && paletteIndex <= CRAMERI_END;
}

// Check if index is a cpt-city (artistic) palette (0-32)
inline bool isCptCityPalette(uint8_t paletteIndex) {
    return paletteIndex <= CPT_CITY_END;
}

// Check if index is a Colorspace palette (57-74)
inline bool isColorspacePalette(uint8_t paletteIndex) {
    return paletteIndex >= COLORSPACE_START && paletteIndex <= COLORSPACE_END;
}

// Get palette name (safe, returns "Unknown" if out of range)
inline const char* getPaletteName(uint8_t paletteIndex) {
    if (paletteIndex >= MASTER_PALETTE_COUNT) return "Unknown";
    return MasterPaletteNames[paletteIndex];
}

// Get category name
inline const char* getPaletteCategory(uint8_t paletteIndex) {
    if (isCptCityPalette(paletteIndex)) return "Artistic";
    if (isCrameriPalette(paletteIndex)) return "Scientific";
    if (isColorspacePalette(paletteIndex)) return "LGP-Optimized";
    return "Unknown";
}

// Get average brightness (perceived luminance)
inline uint8_t getPaletteAvgBrightness(uint8_t paletteIndex) {
    if (paletteIndex >= MASTER_PALETTE_COUNT) return 128;
    return master_palette_avg_Y[paletteIndex];
}

} // namespace palettes
} // namespace lightwaveos

#endif // LIGHTWAVEOS_PALETTES_MASTER_H
