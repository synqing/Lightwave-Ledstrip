// ColorspacePalettes.h
// R colorspace package palettes converted for FastLED
// LGP-optimized with Chromatic Luminance Shift (no pure white endpoints)
//
// Source: https://colorspace.r-forge.r-project.org/
// Viridis family from matplotlib via D3.js
//
// 18 palettes total:
// - Tier 1 (11): Naturally LGP-safe (viridis, plasma, inferno, magma, cubhelix,
//                abyss, bathy, ocean, nighttime, seafloor, ibcso)
// - Tier 2 (7):  Chromatic-shifted (copper, hot, cool, earth, sealand, split, red2green)

#ifndef COLORSPACE_PALETTES_H
#define COLORSPACE_PALETTES_H

#include <FastLED.h>

// =============================================================================
// TIER 1: NATURALLY LGP-SAFE (no modification needed)
// =============================================================================

// viridis - The gold standard sequential palette
// Dark purple → teal → green → yellow (monotonic luminance)
DEFINE_GRADIENT_PALETTE(viridis_gp) {
    0,    68,   1,  84,    // Dark purple
   36,    71,  39, 117,    // Purple
   73,    62,  74, 137,    // Blue-purple
  109,    49, 104, 142,    // Teal-blue
  146,    38, 130, 142,    // Teal
  182,    53, 183, 121,    // Green
  219,   144, 215,  67,    // Yellow-green
  255,   253, 231,  37     // Bright yellow (NOT white)
};

// plasma - Fire-like with perceptual uniformity
// Dark blue → purple → pink → orange → yellow
DEFINE_GRADIENT_PALETTE(plasma_gp) {
    0,    13,   8, 135,    // Dark blue
   36,    75,   3, 161,    // Purple
   73,   126,   3, 168,    // Magenta
  109,   168,  34, 150,    // Pink
  146,   203,  70, 121,    // Salmon
  182,   229, 107,  93,    // Orange
  219,   248, 149,  64,    // Light orange
  255,   240, 249,  33     // Bright yellow (NOT white)
};

// inferno - High contrast, dramatic
// Black → dark red → orange → yellow → light yellow
DEFINE_GRADIENT_PALETTE(inferno_gp) {
    0,     0,   0,   4,    // Near black
   36,    22,  11,  57,    // Dark purple
   73,    66,  10,  91,    // Purple
  109,   120,  28,  85,    // Magenta-red
  146,   172,  50,  58,    // Red-orange
  182,   219,  92,  32,    // Orange
  219,   252, 157,  40,    // Yellow-orange
  255,   252, 255, 164     // Light yellow (cream, NOT white)
};

// magma - Subtle, elegant
// Black → dark purple → pink → cream
DEFINE_GRADIENT_PALETTE(magma_gp) {
    0,     0,   0,   4,    // Near black
   36,    18,  13,  51,    // Dark purple
   73,    51,  16,  91,    // Purple
  109,    95,  22, 109,    // Magenta
  146,   147,  37, 103,    // Pink-magenta
  182,   196,  71,  91,    // Salmon
  219,   237, 130,  98,    // Light salmon
  255,   252, 253, 191     // Cream (NOT white)
};

// cubhelix - Monotonic luminance through hue spiral
// Designed by Dave Green for grayscale-safe printing
DEFINE_GRADIENT_PALETTE(cubhelix_gp) {
    0,     0,   0,   0,    // Black
   36,    22,  11,  43,    // Dark purple
   73,    19,  54,  62,    // Teal
  109,    25, 107,  49,    // Green
  146,    89, 135,  55,    // Yellow-green
  182,   175, 130, 107,    // Tan
  219,   210, 157, 193,    // Pink
  255,   232, 232, 232     // Light gray (NOT pure white)
};

// abyss - Deep ocean blues
// Pure dark blues, no light regions
DEFINE_GRADIENT_PALETTE(abyss_gp) {
    0,     0,   0,  10,    // Near black blue
   64,     5,  15,  45,    // Very dark blue
  128,    15,  40,  90,    // Dark blue
  192,    30,  70, 130,    // Medium blue
  255,    50, 100, 160     // Ocean blue (still dark)
};

// bathy - Bathymetric (ocean depth)
// Dark blue → cyan, all cool tones
DEFINE_GRADIENT_PALETTE(bathy_gp) {
    0,     8,   8,  32,    // Very dark blue
   51,    20,  30,  80,    // Dark blue
  102,    35,  60, 120,    // Blue
  153,    50, 100, 150,    // Medium blue
  204,    80, 150, 180,    // Light blue
  255,   120, 200, 220     // Cyan (NOT white)
};

// ocean - Blues throughout
// Classic ocean palette
DEFINE_GRADIENT_PALETTE(ocean_gp) {
    0,     0,  32,  32,    // Dark teal
   64,     0,  64,  96,    // Dark blue-green
  128,     0, 100, 140,    // Ocean blue
  192,    32, 150, 180,    // Light blue
  255,    80, 200, 200     // Cyan (NOT white)
};

// nighttime - Dark purples and blues for ambient
DEFINE_GRADIENT_PALETTE(nighttime_gp) {
    0,     5,   5,  20,    // Near black
   64,    15,  10,  50,    // Very dark purple
  128,    30,  20,  80,    // Dark purple
  192,    50,  35, 100,    // Purple
  255,    80,  60, 130     // Medium purple (still dark)
};

// seafloor - Marine blues to greens
DEFINE_GRADIENT_PALETTE(seafloor_gp) {
    0,    10,  20,  60,    // Dark blue
   64,    20,  50, 100,    // Blue
  128,    30,  90, 120,    // Blue-green
  192,    50, 130, 110,    // Teal
  255,    80, 160, 100     // Sea green (NOT white)
};

// ibcso - Antarctic ocean (International Bathymetric Chart)
// Dark blues throughout
DEFINE_GRADIENT_PALETTE(ibcso_gp) {
    0,     5,   5,  30,    // Near black blue
   64,    15,  25,  70,    // Very dark blue
  128,    30,  50, 110,    // Dark blue
  192,    50,  80, 150,    // Medium blue
  255,    80, 120, 180     // Blue (NOT white)
};

// =============================================================================
// TIER 2: CHROMATIC LUMINANCE SHIFTED (white endpoints replaced)
// =============================================================================

// copper - Warm copper tones
// Original ends in pale peach, shifted to warm peach
DEFINE_GRADIENT_PALETTE(copper_gp) {
    0,     0,   0,   0,    // Black
   51,    50,  25,  12,    // Dark brown
  102,   100,  55,  30,    // Brown
  153,   160,  95,  55,    // Copper
  204,   210, 150,  95,    // Light copper
  255,   255, 200, 160     // Warm peach (SHIFTED from white)
};

// hot - Classic thermal palette
// Original ends in yellow-white, shifted to pure yellow
DEFINE_GRADIENT_PALETTE(hot_gp) {
    0,     10,   0,   0,   // Near black red
   42,    80,   0,   0,    // Dark red
   85,   170,   0,   0,    // Red
  128,   255,  60,   0,    // Orange-red
  170,   255, 150,   0,    // Orange
  212,   255, 220,   0,    // Yellow-orange
  255,   255, 255, 100     // Bright yellow (SHIFTED from white)
};

// cool - Cyan to magenta
// Original has pale endpoint, shifted to bright cyan
DEFINE_GRADIENT_PALETTE(cool_gp) {
    0,     0, 255, 255,    // Cyan
   51,    50, 200, 255,    // Light cyan
  102,   100, 150, 255,    // Cyan-blue
  153,   150, 100, 255,    // Blue-purple
  204,   200,  50, 255,    // Magenta
  255,   255,   0, 255     // Bright magenta (stays saturated)
};

// earth - Terrain/topographic colors
// Cream sections shifted to warm tan
DEFINE_GRADIENT_PALETTE(earth_gp) {
    0,    30,  50,  30,    // Dark olive
   51,    70,  90,  50,    // Olive
  102,   130, 120,  70,    // Tan-green
  153,   170, 150,  90,    // Light tan
  204,   200, 180, 130,    // Cream-tan
  255,   235, 215, 180     // Warm tan (SHIFTED from white)
};

// sealand - Sea to land transition
// Green endpoint kept saturated
DEFINE_GRADIENT_PALETTE(sealand_gp) {
    0,    20,  60, 120,    // Ocean blue
   51,    40, 100, 140,    // Blue
  102,    70, 150, 130,    // Blue-green
  153,   100, 180, 100,    // Green
  204,   150, 200,  80,    // Yellow-green
  255,   180, 220, 100     // Bright green (SHIFTED - kept saturated)
};

// split - Diverging blue ← neutral → red
// WHITE CENTER replaced with warm gray
DEFINE_GRADIENT_PALETTE(split_gp) {
    0,    30,  50, 150,    // Blue
   51,    60,  90, 180,    // Light blue
  102,   100, 140, 200,    // Pale blue
  128,   200, 190, 180,    // Warm gray (SHIFTED from white)
  153,   200, 140, 130,    // Pale red
  204,   200,  80,  70,    // Light red
  255,   180,  40,  40     // Red
};

// red2green - Diverging red ← neutral → green
// Bright yellow center kept saturated (not white)
DEFINE_GRADIENT_PALETTE(red2green_gp) {
    0,    180,  30,  30,   // Red
   51,   220,  80,  50,    // Orange-red
  102,   240, 150,  80,    // Orange
  128,   255, 240, 100,    // Saturated yellow (SHIFTED - center)
  153,   180, 220,  80,    // Yellow-green
  204,   100, 180,  60,    // Green
  255,    40, 140,  40     // Dark green
};

// =============================================================================
// EXTERN DECLARATIONS (for use in Palettes_Master.h)
// =============================================================================

extern const TProgmemRGBGradientPalette_byte viridis_gp[];
extern const TProgmemRGBGradientPalette_byte plasma_gp[];
extern const TProgmemRGBGradientPalette_byte inferno_gp[];
extern const TProgmemRGBGradientPalette_byte magma_gp[];
extern const TProgmemRGBGradientPalette_byte cubhelix_gp[];
extern const TProgmemRGBGradientPalette_byte abyss_gp[];
extern const TProgmemRGBGradientPalette_byte bathy_gp[];
extern const TProgmemRGBGradientPalette_byte ocean_gp[];
extern const TProgmemRGBGradientPalette_byte nighttime_gp[];
extern const TProgmemRGBGradientPalette_byte seafloor_gp[];
extern const TProgmemRGBGradientPalette_byte ibcso_gp[];
extern const TProgmemRGBGradientPalette_byte copper_gp[];
extern const TProgmemRGBGradientPalette_byte hot_gp[];
extern const TProgmemRGBGradientPalette_byte cool_gp[];
extern const TProgmemRGBGradientPalette_byte earth_gp[];
extern const TProgmemRGBGradientPalette_byte sealand_gp[];
extern const TProgmemRGBGradientPalette_byte split_gp[];
extern const TProgmemRGBGradientPalette_byte red2green_gp[];

#endif // COLORSPACE_PALETTES_H
