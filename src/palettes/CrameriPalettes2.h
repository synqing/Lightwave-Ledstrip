// Fabio Crameri Scientific Color Palettes for FastLED
// These palettes are perceptually uniform and color-vision deficiency friendly
// Source: https://www.fabiocrameri.ch/colourmaps/

#ifndef CRAMERI_PALETTES_H
#define CRAMERI_PALETTES_H

#include <FastLED.h>

// Diverging palettes
DEFINE_GRADIENT_PALETTE(Crameri_Vik_gp){
  0,   3, 43, 113, 32,  11, 95, 146, 64,  95, 157, 188, 96,  196, 219, 231,
  128, 235, 237, 233, 160, 194, 165, 105, 191, 154, 107, 20, 255, 115, 48, 0};

DEFINE_GRADIENT_PALETTE(Crameri_Roma_gp){
  0,   146, 66, 14, 32,  178, 132, 42, 64,  213, 200, 91, 96,  229, 235, 173,
  128, 170, 232, 215, 160, 89, 185, 210, 191, 61, 128, 186, 255, 39, 76, 164};

// Sequential palettes
DEFINE_GRADIENT_PALETTE(Crameri_Tokyo_gp){
  0,   52, 22, 66, 32,  105, 50, 98, 64,  134, 91, 120, 96,  142, 124, 131,
  128, 146, 155, 139, 160, 153, 188, 148, 191, 180, 229, 166, 255, 235, 254, 203};

DEFINE_GRADIENT_PALETTE(Crameri_LaJolla_gp){
  0,   253, 244, 173, 32,  247, 219, 110, 64,  237, 178, 84, 96,  229, 137, 81,
  128, 217, 95, 78, 160, 166, 70, 69, 191, 106, 53, 43, 255, 49, 34, 14};

// Multi-sequential palette
DEFINE_GRADIENT_PALETTE(Crameri_Oleron_gp){
  0,   50, 63, 114, 32,  103, 116, 167, 64,  160, 173, 223, 96,  209, 222, 250,
  128, 26, 76, 0, 160, 134, 120, 43, 191, 204, 170, 115, 255, 248, 226, 192};

// Complex diverging palette
DEFINE_GRADIENT_PALETTE(Crameri_Lisbon_gp){
  0,   187, 198, 229, 32,  104, 137, 179, 64,  36, 77, 117, 96,  16, 31, 47,
  128, 23, 25, 25, 160, 97, 91, 57, 191, 160, 152, 101, 255, 224, 220, 175};

// Sequential color palettes
DEFINE_GRADIENT_PALETTE(Crameri_Hawaii_gp){
  0,   144, 29, 99, 32,  149, 62, 73, 64,  153, 94, 51, 96,  157, 129, 31,
  128, 150, 170, 44, 160, 123, 201, 106, 191, 96, 222, 176, 255, 135, 239, 238};

DEFINE_GRADIENT_PALETTE(Crameri_Devon_gp){
  0,   42, 41, 91, 32,  39, 71, 123, 64,  50, 101, 165, 96,  94, 128, 207,
  128, 154, 155, 231, 160, 190, 184, 242, 191, 215, 212, 247, 255, 242, 240, 252};

// Diverging palettes
DEFINE_GRADIENT_PALETTE(Crameri_Cork_gp){
  0,   42, 50, 101, 32,  58, 103, 149, 64,  128, 159, 189, 96,  205, 217, 229,
  128, 231, 239, 237, 160, 140, 188, 144, 191, 76, 147, 79, 255, 65, 97, 23};

DEFINE_GRADIENT_PALETTE(Crameri_Broc_gp){
  0,   42, 50, 101, 32,  59, 104, 150, 64,  131, 160, 190, 96,  209, 220, 231,
  128, 239, 241, 237, 160, 191, 191, 133, 191, 126, 126, 74, 255, 65, 65, 23};

// Special purpose palettes
DEFINE_GRADIENT_PALETTE(Crameri_GrayC_gp){
  0,   237, 237, 237, 32,  202, 202, 202, 64,  167, 167, 167, 96,  134, 134, 134,
  128, 104, 104, 104, 160, 74, 74, 74, 191, 46, 46, 46, 255, 20, 20, 20};

DEFINE_GRADIENT_PALETTE(Crameri_Batlow_gp){
  0,   9, 42, 92, 32,  22, 76, 97, 64,  55, 105, 88, 96,  102, 122, 63,
  128, 158, 137, 46, 160, 223, 150, 82, 191, 252, 169, 148, 255, 252, 192, 215};

// Additional Crameri/related palettes from user-provided sets
DEFINE_GRADIENT_PALETTE(Crameri_Berlin_gp){
  0,   121, 171, 237, 32,  54, 133, 173, 64,  29, 76, 98, 96,  16, 26, 32,
  128, 25, 11, 9,   160, 90, 28, 6,   191, 156, 79, 61, 255, 221, 141, 134};

DEFINE_GRADIENT_PALETTE(Crameri_Bamako_gp){
  0,   11, 70, 71,  32,  32, 83, 58,  64,  54, 98, 45, 96,  81, 115, 29,
  128, 115, 136, 9, 160, 159, 149, 11, 191, 209, 185, 67, 255, 241, 216, 125};

DEFINE_GRADIENT_PALETTE(Crameri_Acton_gp){
  0,   62, 48, 91,  32,  99, 77, 121, 64,  140, 98, 142, 96,  177, 103, 149,
  128, 209, 124, 166, 160, 212, 153, 187, 191, 214, 181, 206, 255, 223, 213, 228};

DEFINE_GRADIENT_PALETTE(Crameri_Bilbao_gp){
  0,   232, 232, 232, 32,  199, 197, 187, 64,  187, 178, 143, 96,  175, 154, 110,
  128, 167, 125, 97,  160, 158, 94,  84,  191, 135, 59,  59,  255, 97,  20,  22};

DEFINE_GRADIENT_PALETTE(Crameri_Buda_gp){
  0,   179, 28, 166, 32,  183, 63, 149, 64,  192, 92, 139, 96,  201, 120, 130,
  128, 209, 146, 123, 160, 216, 174, 116, 191, 223, 203, 109, 255, 237, 236, 103};

DEFINE_GRADIENT_PALETTE(Crameri_Davos_gp){
  0,   41, 47, 98,   32,  40, 90, 146, 64,  72, 121, 195, 96,  107, 144, 186,
  128, 143, 168, 176, 160, 181, 193, 164, 191, 219, 219, 164, 255, 243, 243, 223};

DEFINE_GRADIENT_PALETTE(Crameri_Imola_gp){
  0,   32, 62, 173,  32,  43, 83, 162, 64,  55, 104, 150, 96,  72, 123, 134,
  128, 97, 147, 123, 160, 128, 179, 115, 191, 163, 213, 107, 255, 221, 244, 102};

DEFINE_GRADIENT_PALETTE(Crameri_Lapaz_gp){
  0,   31, 35, 116,  32,  38, 74, 143, 64,  51, 109, 161, 96,  80, 140, 167,
  128, 126, 160, 158, 160, 173, 167, 140, 191, 228, 187, 153, 255, 255, 226, 213};

DEFINE_GRADIENT_PALETTE(Crameri_Nuuk_gp){
  0,   28, 94, 135,  32,  63, 108, 130, 64,  104, 131, 139, 96,  145, 155, 150,
  128, 172, 174, 150, 160, 188, 187, 139, 191, 205, 204, 131, 255, 238, 238, 156};

DEFINE_GRADIENT_PALETTE(Crameri_Oslo_gp){
  0,   9,  16, 28,   32,  16, 45, 72,   64,  29, 77, 123, 96,  56, 108, 177,
  128, 99, 138, 203, 160, 139, 163, 201, 191, 180, 188, 200, 255, 229, 229, 229};

DEFINE_GRADIENT_PALETTE(Crameri_Tofino_gp){
  0,   179, 187, 236, 32,  95, 125, 193, 64,  44, 68, 114, 96,  18, 26, 41,
  128, 14, 22, 18,   160, 40, 87, 44,   191, 80, 147, 80,  255, 172, 205, 131};

DEFINE_GRADIENT_PALETTE(Crameri_Turku_gp){
  0,   23, 23, 23,   32,  58, 57, 51,   64,  94, 90, 73,  96,  133, 124, 92,
  128, 180, 153, 115, 160, 218, 160, 136, 191, 249, 178, 174, 255, 255, 213, 212};

// Array of Crameri gradient palette references (compatible with FastLED 3.6+)
const TProgmemRGBGradientPaletteRef gCrameriPalettes[] = {
  Crameri_Vik_gp,     // Blue-Brown diverging
  Crameri_Roma_gp,    // Brown-Blue diverging
  Crameri_Tokyo_gp,   // Purple-Yellow sequential
  Crameri_LaJolla_gp, // Yellow-Brown sequential
  Crameri_Oleron_gp,  // Multi-sequential
  Crameri_Lisbon_gp,  // Complex diverging
  Crameri_Hawaii_gp,  // Purple-Cyan sequential
  Crameri_Devon_gp,   // Blue-Purple sequential
  Crameri_Cork_gp,    // Blue-Green diverging
  Crameri_Broc_gp,    // Blue-Olive diverging
  Crameri_GrayC_gp,   // Grayscale
  Crameri_Batlow_gp,  // The flagship Crameri palette
  // Added sets
  Crameri_Berlin_gp,
  Crameri_Bamako_gp,
  Crameri_Acton_gp,
  Crameri_Bilbao_gp,
  Crameri_Buda_gp,
  Crameri_Davos_gp,
  Crameri_Imola_gp,
  Crameri_Lapaz_gp,
  Crameri_Nuuk_gp,
  Crameri_Oslo_gp,
  Crameri_Tofino_gp,
  Crameri_Turku_gp
};

// Count of Crameri palettes
const uint8_t gCrameriPaletteCount = sizeof(gCrameriPalettes) / sizeof(TProgmemRGBGradientPaletteRef);

// Human-readable palette names
const char* const CrameriPaletteNames[] = {
  "Vik",
  "Roma",
  "Tokyo",
  "La Jolla",
  "Oleron",
  "Lisbon",
  "Hawaii",
  "Devon",
  "Cork",
  "Broc",
  "GrayC",
  "Batlow",
  // Added sets
  "Berlin",
  "Bamako",
  "Acton",
  "Bilbao",
  "Buda",
  "Davos",
  "Imola",
  "Lapaz",
  "Nuuk",
  "Oslo",
  "Tofino",
  "Turku"
};

#endif // CRAMERI_PALETTES_H
