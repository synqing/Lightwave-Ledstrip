// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file ColorspacePalettes.h
 * @brief R Colorspace package palette extern declarations
 *
 * LGP-optimized with Chromatic Luminance Shift (no pure white endpoints).
 *
 * 18 palettes total:
 * - Tier 1 (11): Naturally LGP-safe (viridis, plasma, inferno, magma, cubhelix,
 *                abyss, bathy, ocean, nighttime, seafloor, ibcso)
 * - Tier 2 (7):  Chromatic-shifted (copper, hot, cool, earth, sealand, split, red2green)
 *
 * Source: https://colorspace.r-forge.r-project.org/
 * Actual palette data is defined in Palettes_MasterData.cpp
 */

#ifndef COLORSPACE_PALETTES_H
#define COLORSPACE_PALETTES_H

#include <FastLED.h>

// =============================================================================
// EXTERN DECLARATIONS FOR COLORSPACE PALETTES
// =============================================================================
// Tier 1: Naturally LGP-safe
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

// Tier 2: Chromatic-shifted
extern const TProgmemRGBGradientPalette_byte copper_gp[];
extern const TProgmemRGBGradientPalette_byte hot_gp[];
extern const TProgmemRGBGradientPalette_byte cool_gp[];
extern const TProgmemRGBGradientPalette_byte earth_gp[];
extern const TProgmemRGBGradientPalette_byte sealand_gp[];
extern const TProgmemRGBGradientPalette_byte split_gp[];
extern const TProgmemRGBGradientPalette_byte red2green_gp[];

#endif // COLORSPACE_PALETTES_H
