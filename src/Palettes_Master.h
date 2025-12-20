// Palettes_Master.h - Definitive Master Palette Collection
// =============================================================================
// 75 unique gradient palettes from three collections:
//
// CONTENTS:
//   - 33 cpt-city palettes (artistic gradients) [0-32]
//   - 24 Crameri palettes (perceptually uniform, CVD-friendly) [33-56]
//   - 18 R Colorspace palettes (viridis family, LGP-optimized) [57-74]
//
// USAGE:
//   #include "Palettes_Master.h"
//   // Access palettes via gMasterPalettes[0..74]
//   // Access names via MasterPaletteNames[0..74]
//   // Access metadata via master_palette_flags[], master_palette_avg_Y[], etc.
//
// =============================================================================

#pragma once
#ifndef SPECTRASYNC_PALETTES_MASTER_H
#define SPECTRASYNC_PALETTES_MASTER_H

#include <stdint.h>
#include <FastLED.h>

// Include source palette definitions
#include "Palettes.h"                    // cpt-city extern declarations
#include "palettes/CrameriPalettes1.h"   // Crameri palette definitions
#include "palettes/ColorspacePalettes.h" // R colorspace palettes (viridis, plasma, etc.)

// =============================================================================
// PALETTE FLAG DEFINITIONS
// =============================================================================
// Bit flags for palette characteristics
#define PAL_WARM        0x01  // Warm tones (reds, oranges, yellows)
#define PAL_COOL        0x02  // Cool tones (blues, greens, purples)
#define PAL_HIGH_SAT    0x04  // High saturation
#define PAL_WHITE_HEAVY 0x08  // Contains significant white/bright regions
#define PAL_CALM        0x10  // Subtle, calm transitions
#define PAL_VIVID       0x20  // Vivid, high-contrast transitions

// =============================================================================
// MASTER PALETTE ARRAY - 57 UNIQUE PALETTES
// =============================================================================
// Order: cpt-city palettes (0-32), then Crameri palettes (33-56)

const TProgmemRGBGradientPaletteRef gMasterPalettes[] = {
  // -------------------------------------------------------------------------
  // CPT-CITY PALETTES (0-32) - 33 palettes
  // -------------------------------------------------------------------------
  Sunset_Real_gp,                  //  0 - Sunset Real
  es_rivendell_15_gp,              //  1 - Rivendell
  es_ocean_breeze_036_gp,          //  2 - Ocean Breeze 036
  rgi_15_gp,                       //  3 - RGI 15
  retro2_16_gp,                    //  4 - Retro 2
  Analogous_1_gp,                  //  5 - Analogous 1
  es_pinksplash_08_gp,             //  6 - Pink Splash 08
  Coral_reef_gp,                   //  7 - Coral Reef
  es_ocean_breeze_068_gp,          //  8 - Ocean Breeze 068
  es_pinksplash_07_gp,             //  9 - Pink Splash 07
  es_vintage_01_gp,                // 10 - Vintage 01
  departure_gp,                    // 11 - Departure
  es_landscape_64_gp,              // 12 - Landscape 64
  es_landscape_33_gp,              // 13 - Landscape 33
  rainbowsherbet_gp,               // 14 - Rainbow Sherbet
  gr65_hult_gp,                    // 15 - GR65 Hult
  gr64_hult_gp,                    // 16 - GR64 Hult
  GMT_drywet_gp,                   // 17 - GMT Dry Wet
  ib_jul01_gp,                     // 18 - IB Jul01
  es_vintage_57_gp,                // 19 - Vintage 57
  ib15_gp,                         // 20 - IB15
  Fuschia_7_gp,                    // 21 - Fuschia 7
  es_emerald_dragon_08_gp,         // 22 - Emerald Dragon (was missing)
  lava_gp,                         // 23 - Lava
  fire_gp,                         // 24 - Fire
  Colorfull_gp,                    // 25 - Colorful
  Magenta_Evening_gp,              // 26 - Magenta Evening
  Pink_Purple_gp,                  // 27 - Pink Purple (was missing)
  es_autumn_19_gp,                 // 28 - Autumn 19
  BlacK_Blue_Magenta_White_gp,     // 29 - Blue Magenta White
  BlacK_Magenta_Red_gp,            // 30 - Black Magenta Red
  BlacK_Red_Magenta_Yellow_gp,     // 31 - Red Magenta Yellow
  Blue_Cyan_Yellow_gp,             // 32 - Blue Cyan Yellow

  // -------------------------------------------------------------------------
  // CRAMERI SCIENTIFIC PALETTES (33-56) - 24 palettes
  // Perceptually uniform, colorblind-friendly
  // Source: https://www.fabiocrameri.ch/colourmaps/
  // -------------------------------------------------------------------------
  Crameri_Vik_gp,                  // 33 - Vik (diverging blue-brown)
  Crameri_Tokyo_gp,                // 34 - Tokyo (sequential purple-yellow)
  Crameri_Roma_gp,                 // 35 - Roma (diverging brown-blue)
  Crameri_Oleron_gp,               // 36 - Oleron (multi-sequential)
  Crameri_Lisbon_gp,               // 37 - Lisbon (complex diverging)
  Crameri_LaJolla_gp,              // 38 - La Jolla (sequential yellow-brown)
  Crameri_Hawaii_gp,               // 39 - Hawaii (sequential purple-cyan)
  Crameri_Devon_gp,                // 40 - Devon (sequential blue-white)
  Crameri_Cork_gp,                 // 41 - Cork (diverging blue-green)
  Crameri_Broc_gp,                 // 42 - Broc (diverging blue-olive)
  Crameri_Berlin_gp,               // 43 - Berlin (diverging)
  Crameri_Bamako_gp,               // 44 - Bamako (sequential)
  Crameri_Acton_gp,                // 45 - Acton (sequential purple-pink)
  Crameri_Batlow_gp,               // 46 - Batlow (flagship scientific palette)
  Crameri_Bilbao_gp,               // 47 - Bilbao (sequential)
  Crameri_Buda_gp,                 // 48 - Buda (sequential magenta-yellow)
  Crameri_Davos_gp,                // 49 - Davos (sequential blue-cream)
  Crameri_GrayC_gp,                // 50 - GrayC (grayscale)
  Crameri_Imola_gp,                // 51 - Imola (sequential blue-green)
  Crameri_LaPaz_gp,                // 52 - La Paz (sequential)
  Crameri_Nuuk_gp,                 // 53 - Nuuk (sequential)
  Crameri_Oslo_gp,                 // 54 - Oslo (sequential dark-light)
  Crameri_Tofino_gp,               // 55 - Tofino (diverging)
  Crameri_Turku_gp,                // 56 - Turku (sequential warm)

  // -------------------------------------------------------------------------
  // R COLORSPACE PALETTES (57-74) - 18 palettes
  // LGP-optimized with Chromatic Luminance Shift (no pure white endpoints)
  // Source: https://colorspace.r-forge.r-project.org/
  // -------------------------------------------------------------------------
  // Tier 1: Naturally LGP-safe (57-67)
  viridis_gp,                      // 57 - Viridis (gold standard sequential)
  plasma_gp,                       // 58 - Plasma (fire-like, perceptual)
  inferno_gp,                      // 59 - Inferno (high contrast)
  magma_gp,                        // 60 - Magma (subtle, elegant)
  cubhelix_gp,                     // 61 - Cubhelix (grayscale-safe)
  abyss_gp,                        // 62 - Abyss (deep ocean blues)
  bathy_gp,                        // 63 - Bathy (bathymetric)
  ocean_gp,                        // 64 - Ocean (blues throughout)
  nighttime_gp,                    // 65 - Nighttime (dark ambient)
  seafloor_gp,                     // 66 - Seafloor (marine)
  ibcso_gp,                        // 67 - IBCSO (Antarctic ocean)
  // Tier 2: Chromatic-shifted (68-74)
  copper_gp,                       // 68 - Copper (warm tones)
  hot_gp,                          // 69 - Hot (thermal, shifted)
  cool_gp,                         // 70 - Cool (cyan-magenta)
  earth_gp,                        // 71 - Earth (terrain)
  sealand_gp,                      // 72 - Sealand (sea-to-land)
  split_gp,                        // 73 - Split (diverging, gray center)
  red2green_gp                     // 74 - Red2Green (diverging)
};

// Palette count (75 total: 33 cpt-city + 24 Crameri + 18 Colorspace)
constexpr uint8_t gMasterPaletteCount =
  static_cast<uint8_t>(sizeof(gMasterPalettes) / sizeof(TProgmemRGBGradientPaletteRef));

// =============================================================================
// PALETTE NAMES
// =============================================================================

const char* const MasterPaletteNames[] = {
  // cpt-city (0-32)
  /*  0 */ "Sunset Real",
  /*  1 */ "Rivendell",
  /*  2 */ "Ocean Breeze 036",
  /*  3 */ "RGI 15",
  /*  4 */ "Retro 2",
  /*  5 */ "Analogous 1",
  /*  6 */ "Pink Splash 08",
  /*  7 */ "Coral Reef",
  /*  8 */ "Ocean Breeze 068",
  /*  9 */ "Pink Splash 07",
  /* 10 */ "Vintage 01",
  /* 11 */ "Departure",
  /* 12 */ "Landscape 64",
  /* 13 */ "Landscape 33",
  /* 14 */ "Rainbow Sherbet",
  /* 15 */ "GR65 Hult",
  /* 16 */ "GR64 Hult",
  /* 17 */ "GMT Dry Wet",
  /* 18 */ "IB Jul01",
  /* 19 */ "Vintage 57",
  /* 20 */ "IB15",
  /* 21 */ "Fuschia 7",
  /* 22 */ "Emerald Dragon",
  /* 23 */ "Lava",
  /* 24 */ "Fire",
  /* 25 */ "Colorful",
  /* 26 */ "Magenta Evening",
  /* 27 */ "Pink Purple",
  /* 28 */ "Autumn 19",
  /* 29 */ "Blue Magenta White",
  /* 30 */ "Black Magenta Red",
  /* 31 */ "Red Magenta Yellow",
  /* 32 */ "Blue Cyan Yellow",
  // Crameri (33-56)
  /* 33 */ "Vik",
  /* 34 */ "Tokyo",
  /* 35 */ "Roma",
  /* 36 */ "Oleron",
  /* 37 */ "Lisbon",
  /* 38 */ "La Jolla",
  /* 39 */ "Hawaii",
  /* 40 */ "Devon",
  /* 41 */ "Cork",
  /* 42 */ "Broc",
  /* 43 */ "Berlin",
  /* 44 */ "Bamako",
  /* 45 */ "Acton",
  /* 46 */ "Batlow",
  /* 47 */ "Bilbao",
  /* 48 */ "Buda",
  /* 49 */ "Davos",
  /* 50 */ "GrayC",
  /* 51 */ "Imola",
  /* 52 */ "La Paz",
  /* 53 */ "Nuuk",
  /* 54 */ "Oslo",
  /* 55 */ "Tofino",
  /* 56 */ "Turku",
  // R Colorspace (57-74)
  /* 57 */ "Viridis",
  /* 58 */ "Plasma",
  /* 59 */ "Inferno",
  /* 60 */ "Magma",
  /* 61 */ "Cubhelix",
  /* 62 */ "Abyss",
  /* 63 */ "Bathy",
  /* 64 */ "Ocean",
  /* 65 */ "Nighttime",
  /* 66 */ "Seafloor",
  /* 67 */ "IBCSO",
  /* 68 */ "Copper",
  /* 69 */ "Hot",
  /* 70 */ "Cool",
  /* 71 */ "Earth",
  /* 72 */ "Sealand",
  /* 73 */ "Split",
  /* 74 */ "Red2Green"
};

// =============================================================================
// PALETTE METADATA (extern declarations - definitions in Palettes_MasterData.cpp)
// =============================================================================
// Flags: PAL_WARM=1, PAL_COOL=2, PAL_HIGH_SAT=4, PAL_WHITE_HEAVY=8, PAL_CALM=16, PAL_VIVID=32

extern const uint8_t master_palette_flags[];      // 57 entries
extern const uint8_t master_palette_avg_Y[];      // 57 entries - perceived brightness
extern const uint8_t master_palette_max_brightness[];  // 57 entries - power caps

// =============================================================================
// COMPILE-TIME SAFETY CHECKS
// =============================================================================
// Note: Metadata arrays (flags, avg_Y, max_brightness) are extern - checked at link time

static_assert(
  (sizeof(gMasterPalettes) / sizeof(gMasterPalettes[0])) == 75,
  "gMasterPalettes[] must have exactly 75 entries!"
);

static_assert(
  (sizeof(gMasterPalettes) / sizeof(gMasterPalettes[0])) ==
  (sizeof(MasterPaletteNames) / sizeof(MasterPaletteNames[0])),
  "MasterPaletteNames[] must match gMasterPalettes[] size!"
);

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

// Check if palette has specific flag
inline bool paletteHasFlag(uint8_t paletteIndex, uint8_t flag) {
  if (paletteIndex >= gMasterPaletteCount) return false;
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

// Get brightness-adjusted max for a palette (for power management)
inline uint8_t getPaletteMaxBrightness(uint8_t paletteIndex) {
  if (paletteIndex >= gMasterPaletteCount) return 255;
  return master_palette_max_brightness[paletteIndex];
}

// Check if index is a Crameri (scientific) palette (33-56)
inline bool isCrameriPalette(uint8_t paletteIndex) {
  return paletteIndex >= 33 && paletteIndex <= 56;
}

// Check if index is a cpt-city (artistic) palette (0-32)
inline bool isCptCityPalette(uint8_t paletteIndex) {
  return paletteIndex < 33;
}

// Check if index is an R Colorspace palette (57-74)
inline bool isColorspacePalette(uint8_t paletteIndex) {
  return paletteIndex >= 57 && paletteIndex < gMasterPaletteCount;
}

#endif // SPECTRASYNC_PALETTES_MASTER_H
