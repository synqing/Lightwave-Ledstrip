/**
 * @file CrameriPalettes.h
 * @brief Fabio Crameri Scientific Color Palette extern declarations
 *
 * 24 perceptually uniform, colorblind-friendly scientific palettes.
 * These are CVD-safe (Color Vision Deficiency) and suitable for
 * data visualization and ambient lighting.
 *
 * Source: https://www.fabiocrameri.ch/colourmaps/
 * Actual palette data is defined in Palettes_MasterData.cpp
 */

#ifndef CRAMERI_PALETTES_H
#define CRAMERI_PALETTES_H

#include <FastLED.h>

// Extern declarations for Crameri palettes
// Actual definitions are in Palettes_MasterData.cpp
extern const TProgmemRGBGradientPalette_byte Crameri_Vik_gp[];
extern const TProgmemRGBGradientPalette_byte Crameri_Tokyo_gp[];
extern const TProgmemRGBGradientPalette_byte Crameri_Roma_gp[];
extern const TProgmemRGBGradientPalette_byte Crameri_Oleron_gp[];
extern const TProgmemRGBGradientPalette_byte Crameri_Lisbon_gp[];
extern const TProgmemRGBGradientPalette_byte Crameri_LaJolla_gp[];
extern const TProgmemRGBGradientPalette_byte Crameri_Hawaii_gp[];
extern const TProgmemRGBGradientPalette_byte Crameri_Devon_gp[];
extern const TProgmemRGBGradientPalette_byte Crameri_Cork_gp[];
extern const TProgmemRGBGradientPalette_byte Crameri_Broc_gp[];
extern const TProgmemRGBGradientPalette_byte Crameri_Berlin_gp[];
extern const TProgmemRGBGradientPalette_byte Crameri_Bamako_gp[];
extern const TProgmemRGBGradientPalette_byte Crameri_Acton_gp[];
extern const TProgmemRGBGradientPalette_byte Crameri_Batlow_gp[];
extern const TProgmemRGBGradientPalette_byte Crameri_Bilbao_gp[];
extern const TProgmemRGBGradientPalette_byte Crameri_Buda_gp[];
extern const TProgmemRGBGradientPalette_byte Crameri_Davos_gp[];
extern const TProgmemRGBGradientPalette_byte Crameri_GrayC_gp[];
extern const TProgmemRGBGradientPalette_byte Crameri_Imola_gp[];
extern const TProgmemRGBGradientPalette_byte Crameri_LaPaz_gp[];
extern const TProgmemRGBGradientPalette_byte Crameri_Nuuk_gp[];
extern const TProgmemRGBGradientPalette_byte Crameri_Oslo_gp[];
extern const TProgmemRGBGradientPalette_byte Crameri_Tofino_gp[];
extern const TProgmemRGBGradientPalette_byte Crameri_Turku_gp[];

#endif // CRAMERI_PALETTES_H
