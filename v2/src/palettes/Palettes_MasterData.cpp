/**
 * @file Palettes_MasterData.cpp
 * @brief Master palette definitions and metadata
 *
 * Contains all 75 palette definitions and the gMasterPalettes array.
 * This is the ONLY file that defines palette gradient data.
 *
 * - 33 CPT-City palettes (artistic gradients)
 * - 24 Crameri palettes (perceptually uniform, CVD-friendly)
 * - 18 Colorspace palettes (LGP-optimized)
 */

#include <FastLED.h>

// =============================================================================
// CPT-CITY PALETTE DEFINITIONS (0-32)
// =============================================================================

// Gradient palette "ib_jul01_gp"
DEFINE_GRADIENT_PALETTE(ib_jul01_gp){
  0, 194, 1, 1,
  94, 1, 29, 18,
  132, 57, 131, 28,
  255, 113, 1, 1
};

// Gradient palette "es_vintage_57_gp"
DEFINE_GRADIENT_PALETTE(es_vintage_57_gp){
  0, 2, 1, 1,
  53, 18, 1, 0,
  104, 69, 29, 1,
  153, 167, 135, 10,
  255, 46, 56, 4
};

// Gradient palette "es_vintage_01_gp"
DEFINE_GRADIENT_PALETTE(es_vintage_01_gp){
  0, 4, 1, 1,
  51, 16, 0, 1,
  76, 97, 104, 3,
  101, 255, 131, 19,
  127, 67, 9, 4,
  153, 16, 0, 1,
  229, 4, 1, 1,
  255, 4, 1, 1
};

// Gradient palette "es_rivendell_15_gp"
DEFINE_GRADIENT_PALETTE(es_rivendell_15_gp){
  0, 1, 14, 5,
  101, 16, 36, 14,
  165, 56, 68, 30,
  242, 150, 156, 99,
  255, 150, 156, 99
};

// Gradient palette "rgi_15_gp"
DEFINE_GRADIENT_PALETTE(rgi_15_gp){
  0, 4, 1, 31,
  31, 55, 1, 16,
  63, 197, 3, 7,
  95, 59, 2, 17,
  127, 6, 2, 34,
  159, 39, 6, 33,
  191, 112, 13, 32,
  223, 56, 9, 35,
  255, 22, 6, 38
};

// Gradient palette "retro2_16_gp"
DEFINE_GRADIENT_PALETTE(retro2_16_gp){
  0, 188, 135, 1,
  255, 46, 7, 1
};

// Gradient palette "Analogous_1_gp"
DEFINE_GRADIENT_PALETTE(Analogous_1_gp){
  0, 3, 0, 255,
  63, 23, 0, 255,
  127, 67, 0, 255,
  191, 142, 0, 45,
  255, 255, 0, 0
};

// Gradient palette "es_pinksplash_08_gp"
DEFINE_GRADIENT_PALETTE(es_pinksplash_08_gp){
  0, 126, 11, 255,
  127, 197, 1, 22,
  175, 210, 157, 172,
  221, 157, 3, 112,
  255, 157, 3, 112
};

// Gradient palette "es_pinksplash_07_gp"
DEFINE_GRADIENT_PALETTE(es_pinksplash_07_gp){
  0, 229, 1, 1,
  61, 242, 4, 63,
  101, 255, 12, 255,
  127, 249, 81, 252,
  153, 255, 11, 235,
  193, 244, 5, 68,
  255, 232, 1, 5
};

// Gradient palette "Coral_reef_gp"
DEFINE_GRADIENT_PALETTE(Coral_reef_gp){
  0, 40, 199, 197,
  50, 10, 152, 155,
  96, 1, 111, 120,
  96, 43, 127, 162,
  139, 10, 73, 111,
  255, 1, 34, 71
};

// Gradient palette "es_ocean_breeze_068_gp"
DEFINE_GRADIENT_PALETTE(es_ocean_breeze_068_gp){
  0, 100, 156, 153,
  51, 1, 99, 137,
  101, 1, 68, 84,
  104, 35, 142, 168,
  178, 0, 63, 117,
  255, 1, 10, 10
};

// Gradient palette "es_ocean_breeze_036_gp"
DEFINE_GRADIENT_PALETTE(es_ocean_breeze_036_gp){
  0, 1, 6, 7,
  89, 1, 99, 111,
  153, 144, 209, 255,
  255, 0, 73, 82
};

// Gradient palette "departure_gp"
DEFINE_GRADIENT_PALETTE(departure_gp){
  0, 8, 3, 0,
  42, 23, 7, 0,
  63, 75, 38, 6,
  84, 169, 99, 38,
  106, 213, 169, 119,
  116, 255, 255, 255,
  138, 135, 255, 138,
  148, 22, 255, 24,
  170, 0, 255, 0,
  191, 0, 136, 0,
  212, 0, 55, 0,
  255, 0, 55, 0
};

// Gradient palette "es_landscape_64_gp"
DEFINE_GRADIENT_PALETTE(es_landscape_64_gp){
  0, 0, 0, 0,
  37, 2, 25, 1,
  76, 15, 115, 5,
  127, 79, 213, 1,
  128, 126, 211, 47,
  130, 188, 209, 247,
  153, 144, 182, 205,
  204, 59, 117, 250,
  255, 1, 37, 192
};

// Gradient palette "es_landscape_33_gp"
DEFINE_GRADIENT_PALETTE(es_landscape_33_gp){
  0, 1, 5, 0,
  19, 32, 23, 1,
  38, 161, 55, 1,
  63, 229, 144, 1,
  66, 39, 142, 74,
  255, 1, 4, 1
};

// Gradient palette "rainbowsherbet_gp"
DEFINE_GRADIENT_PALETTE(rainbowsherbet_gp){
  0, 255, 33, 4,
  43, 255, 68, 25,
  86, 255, 7, 25,
  127, 255, 82, 103,
  170, 255, 255, 242,
  209, 42, 255, 22,
  255, 87, 255, 65
};

// Gradient palette "gr65_hult_gp"
DEFINE_GRADIENT_PALETTE(gr65_hult_gp){
  0, 247, 176, 247,
  48, 255, 136, 255,
  89, 220, 29, 226,
  160, 7, 82, 178,
  216, 1, 124, 109,
  255, 1, 124, 109
};

// Gradient palette "gr64_hult_gp"
DEFINE_GRADIENT_PALETTE(gr64_hult_gp){
  0, 1, 124, 109,
  66, 1, 93, 79,
  104, 52, 65, 1,
  130, 115, 127, 1,
  150, 52, 65, 1,
  201, 1, 86, 72,
  239, 0, 55, 45,
  255, 0, 55, 45
};

// Gradient palette "GMT_drywet_gp"
DEFINE_GRADIENT_PALETTE(GMT_drywet_gp){
  0, 47, 30, 2,
  42, 213, 147, 24,
  84, 103, 219, 52,
  127, 3, 219, 207,
  170, 1, 48, 214,
  212, 1, 1, 111,
  255, 1, 7, 33
};

// Gradient palette "ib15_gp"
DEFINE_GRADIENT_PALETTE(ib15_gp){
  0, 113, 91, 147,
  72, 157, 88, 78,
  89, 208, 85, 33,
  107, 255, 29, 11,
  141, 137, 31, 39,
  255, 59, 33, 89
};

// Gradient palette "Fuschia_7_gp"
DEFINE_GRADIENT_PALETTE(Fuschia_7_gp){
  0, 43, 3, 153,
  63, 100, 4, 103,
  127, 188, 5, 66,
  191, 161, 11, 115,
  255, 135, 20, 182
};

// Gradient palette "es_emerald_dragon_08_gp"
DEFINE_GRADIENT_PALETTE(es_emerald_dragon_08_gp){
  0, 97, 255, 1,
  101, 47, 133, 1,
  178, 13, 43, 1,
  255, 2, 10, 1
};

// Gradient palette "lava_gp"
DEFINE_GRADIENT_PALETTE(lava_gp){
  0, 0, 0, 0,
  46, 18, 0, 0,
  96, 113, 0, 0,
  108, 142, 3, 1,
  119, 175, 17, 1,
  146, 213, 44, 2,
  174, 255, 82, 4,
  188, 255, 115, 4,
  202, 255, 156, 4,
  218, 255, 203, 4,
  234, 255, 255, 4,
  244, 255, 255, 71,
  255, 255, 255, 255
};

// Gradient palette "fire_gp"
DEFINE_GRADIENT_PALETTE(fire_gp){
  0, 1, 1, 0,
  76, 32, 5, 0,
  146, 192, 24, 0,
  197, 220, 105, 5,
  240, 252, 255, 31,
  250, 252, 255, 111,
  255, 255, 255, 255
};

// Gradient palette "Colorfull_gp"
DEFINE_GRADIENT_PALETTE(Colorfull_gp){
  0, 10, 85, 5,
  25, 29, 109, 18,
  60, 59, 138, 42,
  93, 83, 99, 52,
  106, 110, 66, 64,
  109, 123, 49, 65,
  113, 139, 35, 66,
  116, 192, 117, 98,
  124, 255, 255, 137,
  168, 100, 180, 155,
  255, 22, 121, 174
};

// Gradient palette "Magenta_Evening_gp"
DEFINE_GRADIENT_PALETTE(Magenta_Evening_gp){
  0, 71, 27, 39,
  31, 130, 11, 51,
  63, 213, 2, 64,
  70, 232, 1, 66,
  76, 252, 1, 69,
  108, 123, 2, 51,
  255, 46, 9, 35
};

// Gradient palette "Pink_Purple_gp"
DEFINE_GRADIENT_PALETTE(Pink_Purple_gp){
  0, 19, 2, 39,
  25, 26, 4, 45,
  51, 33, 6, 52,
  76, 68, 62, 125,
  102, 118, 187, 240,
  109, 163, 215, 247,
  114, 217, 244, 255,
  122, 159, 149, 221,
  149, 113, 78, 188,
  183, 128, 57, 155,
  255, 146, 40, 123
};

// Gradient palette "Sunset_Real_gp"
DEFINE_GRADIENT_PALETTE(Sunset_Real_gp){
  0, 120, 0, 0,
  22, 179, 22, 0,
  51, 255, 104, 0,
  85, 167, 22, 18,
  135, 100, 0, 103,
  198, 16, 0, 130,
  255, 0, 0, 160
};

// Gradient palette "es_autumn_19_gp"
DEFINE_GRADIENT_PALETTE(es_autumn_19_gp){
  0, 26, 1, 1,
  51, 67, 4, 1,
  84, 118, 14, 1,
  104, 137, 152, 52,
  112, 113, 65, 1,
  122, 133, 149, 59,
  124, 137, 152, 52,
  135, 113, 65, 1,
  142, 139, 154, 46,
  163, 113, 13, 1,
  204, 55, 3, 1,
  249, 17, 1, 1,
  255, 17, 1, 1
};

// Gradient palette "BlacK_Blue_Magenta_White_gp"
DEFINE_GRADIENT_PALETTE(BlacK_Blue_Magenta_White_gp){
  0, 0, 0, 0,
  42, 0, 0, 45,
  84, 0, 0, 255,
  127, 42, 0, 255,
  170, 255, 0, 255,
  212, 255, 55, 255,
  255, 255, 255, 255
};

// Gradient palette "BlacK_Magenta_Red_gp"
DEFINE_GRADIENT_PALETTE(BlacK_Magenta_Red_gp){
  0, 0, 0, 0,
  63, 42, 0, 45,
  127, 255, 0, 255,
  191, 255, 0, 45,
  255, 255, 0, 0
};

// Gradient palette "BlacK_Red_Magenta_Yellow_gp"
DEFINE_GRADIENT_PALETTE(BlacK_Red_Magenta_Yellow_gp){
  0, 0, 0, 0,
  42, 42, 0, 0,
  84, 255, 0, 0,
  127, 255, 0, 45,
  170, 255, 0, 255,
  212, 255, 55, 45,
  255, 255, 255, 0
};

// Gradient palette "Blue_Cyan_Yellow_gp"
DEFINE_GRADIENT_PALETTE(Blue_Cyan_Yellow_gp){
  0, 0, 0, 255,
  63, 0, 55, 255,
  127, 0, 255, 255,
  191, 42, 255, 45,
  255, 255, 255, 0
};

// =============================================================================
// CRAMERI PALETTE DEFINITIONS (33-56)
// =============================================================================

DEFINE_GRADIENT_PALETTE(Crameri_Vik_gp){
  0,   3, 43, 113,
  36,  11, 95, 146,
  73,  95, 157, 188,
  109, 196, 219, 231,
  146, 235, 237, 233,
  182, 194, 165, 105,
  219, 154, 107, 20,
  255, 115, 48, 0
};

DEFINE_GRADIENT_PALETTE(Crameri_Tokyo_gp){
  0,   52, 22, 66,
  36,  105, 50, 98,
  73,  134, 91, 120,
  109, 142, 124, 131,
  146, 146, 155, 139,
  182, 153, 188, 148,
  219, 180, 229, 166,
  255, 235, 254, 203
};

DEFINE_GRADIENT_PALETTE(Crameri_Roma_gp){
  0,   146, 66, 14,
  36,  178, 132, 42,
  73,  213, 200, 91,
  109, 229, 235, 173,
  146, 170, 232, 215,
  182, 89, 185, 210,
  219, 61, 128, 186,
  255, 39, 76, 164
};

DEFINE_GRADIENT_PALETTE(Crameri_Oleron_gp){
  0,   50, 63, 114,
  36,  103, 116, 167,
  73,  160, 173, 223,
  109, 209, 222, 250,
  146, 26, 76, 0,
  182, 134, 120, 43,
  219, 204, 170, 115,
  255, 248, 226, 192
};

DEFINE_GRADIENT_PALETTE(Crameri_Lisbon_gp){
  0,   187, 198, 229,
  36,  104, 137, 179,
  73,  36, 77, 117,
  109, 16, 31, 47,
  146, 23, 25, 25,
  182, 97, 91, 57,
  219, 160, 152, 101,
  255, 224, 220, 175
};

DEFINE_GRADIENT_PALETTE(Crameri_LaJolla_gp){
  0,   253, 244, 173,
  36,  247, 219, 110,
  73,  237, 178, 84,
  109, 229, 137, 81,
  146, 217, 95, 78,
  182, 166, 70, 69,
  219, 106, 53, 43,
  255, 49, 34, 14
};

DEFINE_GRADIENT_PALETTE(Crameri_Hawaii_gp){
  0,   144, 29, 99,
  36,  149, 62, 73,
  73,  153, 94, 51,
  109, 157, 129, 31,
  146, 150, 170, 44,
  182, 123, 201, 106,
  219, 96, 222, 176,
  255, 135, 239, 238
};

DEFINE_GRADIENT_PALETTE(Crameri_Devon_gp){
  0,   42, 41, 91,
  36,  39, 71, 123,
  73,  50, 101, 165,
  109, 94, 128, 207,
  146, 154, 155, 231,
  182, 190, 184, 242,
  219, 215, 212, 247,
  255, 242, 240, 252
};

DEFINE_GRADIENT_PALETTE(Crameri_Cork_gp){
  0,   42, 50, 101,
  36,  58, 103, 149,
  73,  128, 159, 189,
  109, 205, 217, 229,
  146, 231, 239, 237,
  182, 140, 188, 144,
  219, 76, 147, 79,
  255, 65, 97, 23
};

DEFINE_GRADIENT_PALETTE(Crameri_Broc_gp){
  0,   42, 50, 101,
  36,  59, 104, 150,
  73,  131, 160, 190,
  109, 209, 220, 231,
  146, 239, 241, 237,
  182, 191, 191, 133,
  219, 126, 126, 74,
  255, 65, 65, 23
};

DEFINE_GRADIENT_PALETTE(Crameri_Berlin_gp){
  0,   121, 171, 237,
  36,  54, 133, 173,
  73,  29, 76, 98,
  109, 16, 26, 32,
  146, 25, 11, 9,
  182, 90, 28, 6,
  219, 156, 79, 61,
  255, 221, 141, 134
};

DEFINE_GRADIENT_PALETTE(Crameri_Bamako_gp){
  0,   11, 70, 71,
  36,  32, 83, 58,
  73,  54, 98, 45,
  109, 81, 115, 29,
  146, 115, 136, 9,
  182, 159, 149, 11,
  219, 209, 185, 67,
  255, 241, 216, 125
};

DEFINE_GRADIENT_PALETTE(Crameri_Acton_gp){
  0,   62, 48, 91,
  36,  99, 77, 121,
  73,  140, 98, 142,
  109, 177, 103, 149,
  146, 209, 124, 166,
  182, 212, 153, 187,
  219, 214, 181, 206,
  255, 223, 213, 228
};

DEFINE_GRADIENT_PALETTE(Crameri_Batlow_gp){
  0,   9, 42, 92,
  36,  22, 76, 97,
  73,  55, 105, 88,
  109, 102, 122, 63,
  146, 158, 137, 46,
  182, 223, 150, 82,
  219, 252, 169, 148,
  255, 252, 192, 215
};

DEFINE_GRADIENT_PALETTE(Crameri_Bilbao_gp){
  0,   232, 232, 232,
  36,  199, 197, 187,
  73,  187, 178, 143,
  109, 175, 154, 110,
  146, 167, 125, 97,
  182, 158, 94, 84,
  219, 135, 59, 59,
  255, 97, 20, 22
};

DEFINE_GRADIENT_PALETTE(Crameri_Buda_gp){
  0,   179, 28, 166,
  36,  183, 63, 149,
  73,  192, 92, 139,
  109, 201, 120, 130,
  146, 209, 146, 123,
  182, 216, 174, 116,
  219, 223, 203, 109,
  255, 237, 236, 103
};

DEFINE_GRADIENT_PALETTE(Crameri_Davos_gp){
  0,   41, 47, 98,
  36,  40, 90, 146,
  73,  72, 121, 195,
  109, 107, 144, 186,
  146, 143, 168, 176,
  182, 181, 193, 164,
  219, 219, 219, 164,
  255, 243, 243, 223
};

DEFINE_GRADIENT_PALETTE(Crameri_GrayC_gp){
  0,   237, 237, 237,
  36,  202, 202, 202,
  73,  167, 167, 167,
  109, 134, 134, 134,
  146, 104, 104, 104,
  182, 74, 74, 74,
  219, 46, 46, 46,
  255, 20, 20, 20
};

DEFINE_GRADIENT_PALETTE(Crameri_Imola_gp){
  0,   32, 62, 173,
  36,  43, 83, 162,
  73,  55, 104, 150,
  109, 72, 123, 134,
  146, 97, 147, 123,
  182, 128, 179, 115,
  219, 163, 213, 107,
  255, 221, 244, 102
};

DEFINE_GRADIENT_PALETTE(Crameri_LaPaz_gp){
  0,   31, 35, 116,
  36,  38, 74, 143,
  73,  51, 109, 161,
  109, 80, 140, 167,
  146, 126, 160, 158,
  182, 173, 167, 140,
  219, 228, 187, 153,
  255, 255, 226, 213
};

DEFINE_GRADIENT_PALETTE(Crameri_Nuuk_gp){
  0,   28, 94, 135,
  36,  63, 108, 130,
  73,  104, 131, 139,
  109, 145, 155, 150,
  146, 172, 174, 150,
  182, 188, 187, 139,
  219, 205, 204, 131,
  255, 238, 238, 156
};

DEFINE_GRADIENT_PALETTE(Crameri_Oslo_gp){
  0,   9, 16, 28,
  36,  16, 45, 72,
  73,  29, 77, 123,
  109, 56, 108, 177,
  146, 99, 138, 203,
  182, 139, 163, 201,
  219, 180, 188, 200,
  255, 229, 229, 229
};

DEFINE_GRADIENT_PALETTE(Crameri_Tofino_gp){
  0,   179, 187, 236,
  36,  95, 125, 193,
  73,  44, 68, 114,
  109, 18, 26, 41,
  146, 14, 22, 18,
  182, 40, 87, 44,
  219, 80, 147, 80,
  255, 172, 205, 131
};

DEFINE_GRADIENT_PALETTE(Crameri_Turku_gp){
  0,   23, 23, 23,
  36,  58, 57, 51,
  73,  94, 90, 73,
  109, 133, 124, 92,
  146, 180, 153, 115,
  182, 218, 160, 136,
  219, 249, 178, 174,
  255, 255, 213, 212
};

// =============================================================================
// COLORSPACE PALETTE DEFINITIONS (57-74)
// =============================================================================

// viridis - The gold standard sequential palette
DEFINE_GRADIENT_PALETTE(viridis_gp) {
    0,    68,   1,  84,
   36,    71,  39, 117,
   73,    62,  74, 137,
  109,    49, 104, 142,
  146,    38, 130, 142,
  182,    53, 183, 121,
  219,   144, 215,  67,
  255,   253, 231,  37
};

// plasma - Fire-like with perceptual uniformity
DEFINE_GRADIENT_PALETTE(plasma_gp) {
    0,    13,   8, 135,
   36,    75,   3, 161,
   73,   126,   3, 168,
  109,   168,  34, 150,
  146,   203,  70, 121,
  182,   229, 107,  93,
  219,   248, 149,  64,
  255,   240, 249,  33
};

// inferno - High contrast, dramatic
DEFINE_GRADIENT_PALETTE(inferno_gp) {
    0,     0,   0,   4,
   36,    22,  11,  57,
   73,    66,  10,  91,
  109,   120,  28,  85,
  146,   172,  50,  58,
  182,   219,  92,  32,
  219,   252, 157,  40,
  255,   252, 255, 164
};

// magma - Subtle, elegant
DEFINE_GRADIENT_PALETTE(magma_gp) {
    0,     0,   0,   4,
   36,    18,  13,  51,
   73,    51,  16,  91,
  109,    95,  22, 109,
  146,   147,  37, 103,
  182,   196,  71,  91,
  219,   237, 130,  98,
  255,   252, 253, 191
};

// cubhelix - Monotonic luminance through hue spiral
DEFINE_GRADIENT_PALETTE(cubhelix_gp) {
    0,     0,   0,   0,
   36,    22,  11,  43,
   73,    19,  54,  62,
  109,    25, 107,  49,
  146,    89, 135,  55,
  182,   175, 130, 107,
  219,   210, 157, 193,
  255,   232, 232, 232
};

// abyss - Deep ocean blues
DEFINE_GRADIENT_PALETTE(abyss_gp) {
    0,     0,   0,  10,
   64,     5,  15,  45,
  128,    15,  40,  90,
  192,    30,  70, 130,
  255,    50, 100, 160
};

// bathy - Bathymetric (ocean depth)
DEFINE_GRADIENT_PALETTE(bathy_gp) {
    0,     8,   8,  32,
   51,    20,  30,  80,
  102,    35,  60, 120,
  153,    50, 100, 150,
  204,    80, 150, 180,
  255,   120, 200, 220
};

// ocean - Blues throughout
DEFINE_GRADIENT_PALETTE(ocean_gp) {
    0,     0,  32,  32,
   64,     0,  64,  96,
  128,     0, 100, 140,
  192,    32, 150, 180,
  255,    80, 200, 200
};

// nighttime - Dark purples and blues for ambient
DEFINE_GRADIENT_PALETTE(nighttime_gp) {
    0,     5,   5,  20,
   64,    15,  10,  50,
  128,    30,  20,  80,
  192,    50,  35, 100,
  255,    80,  60, 130
};

// seafloor - Marine blues to greens
DEFINE_GRADIENT_PALETTE(seafloor_gp) {
    0,    10,  20,  60,
   64,    20,  50, 100,
  128,    30,  90, 120,
  192,    50, 130, 110,
  255,    80, 160, 100
};

// ibcso - Antarctic ocean
DEFINE_GRADIENT_PALETTE(ibcso_gp) {
    0,     5,   5,  30,
   64,    15,  25,  70,
  128,    30,  50, 110,
  192,    50,  80, 150,
  255,    80, 120, 180
};

// copper - Warm copper tones
DEFINE_GRADIENT_PALETTE(copper_gp) {
    0,     0,   0,   0,
   51,    50,  25,  12,
  102,   100,  55,  30,
  153,   160,  95,  55,
  204,   210, 150,  95,
  255,   255, 200, 160
};

// hot - Classic thermal palette
DEFINE_GRADIENT_PALETTE(hot_gp) {
    0,    10,   0,   0,
   42,    80,   0,   0,
   85,   170,   0,   0,
  128,   255,  60,   0,
  170,   255, 150,   0,
  212,   255, 220,   0,
  255,   255, 255, 100
};

// cool - Cyan to magenta
DEFINE_GRADIENT_PALETTE(cool_gp) {
    0,     0, 255, 255,
   51,    50, 200, 255,
  102,   100, 150, 255,
  153,   150, 100, 255,
  204,   200,  50, 255,
  255,   255,   0, 255
};

// earth - Terrain/topographic colors
DEFINE_GRADIENT_PALETTE(earth_gp) {
    0,    30,  50,  30,
   51,    70,  90,  50,
  102,   130, 120,  70,
  153,   170, 150,  90,
  204,   200, 180, 130,
  255,   235, 215, 180
};

// sealand - Sea to land transition
DEFINE_GRADIENT_PALETTE(sealand_gp) {
    0,    20,  60, 120,
   51,    40, 100, 140,
  102,    70, 150, 130,
  153,   100, 180, 100,
  204,   150, 200,  80,
  255,   180, 220, 100
};

// split - Diverging blue <- neutral -> red
DEFINE_GRADIENT_PALETTE(split_gp) {
    0,    30,  50, 150,
   51,    60,  90, 180,
  102,   100, 140, 200,
  128,   200, 190, 180,
  153,   200, 140, 130,
  204,   200,  80,  70,
  255,   180,  40,  40
};

// red2green - Diverging red <- neutral -> green
DEFINE_GRADIENT_PALETTE(red2green_gp) {
    0,   180,  30,  30,
   51,   220,  80,  50,
  102,   240, 150,  80,
  128,   255, 240, 100,
  153,   180, 220,  80,
  204,   100, 180,  60,
  255,    40, 140,  40
};

// =============================================================================
// Now include the master header for extern declarations
// =============================================================================

#include "Palettes_Master.h"

namespace lightwaveos {
namespace palettes {

// =============================================================================
// MASTER PALETTE ARRAY - 75 UNIQUE PALETTES
// =============================================================================

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
    es_emerald_dragon_08_gp,         // 22 - Emerald Dragon
    lava_gp,                         // 23 - Lava
    fire_gp,                         // 24 - Fire
    Colorfull_gp,                    // 25 - Colorful
    Magenta_Evening_gp,              // 26 - Magenta Evening
    Pink_Purple_gp,                  // 27 - Pink Purple
    es_autumn_19_gp,                 // 28 - Autumn 19
    BlacK_Blue_Magenta_White_gp,     // 29 - Blue Magenta White
    BlacK_Magenta_Red_gp,            // 30 - Black Magenta Red
    BlacK_Red_Magenta_Yellow_gp,     // 31 - Red Magenta Yellow
    Blue_Cyan_Yellow_gp,             // 32 - Blue Cyan Yellow

    // -------------------------------------------------------------------------
    // CRAMERI SCIENTIFIC PALETTES (33-56) - 24 palettes
    // -------------------------------------------------------------------------
    Crameri_Vik_gp,                  // 33 - Vik
    Crameri_Tokyo_gp,                // 34 - Tokyo
    Crameri_Roma_gp,                 // 35 - Roma
    Crameri_Oleron_gp,               // 36 - Oleron
    Crameri_Lisbon_gp,               // 37 - Lisbon
    Crameri_LaJolla_gp,              // 38 - La Jolla
    Crameri_Hawaii_gp,               // 39 - Hawaii
    Crameri_Devon_gp,                // 40 - Devon
    Crameri_Cork_gp,                 // 41 - Cork
    Crameri_Broc_gp,                 // 42 - Broc
    Crameri_Berlin_gp,               // 43 - Berlin
    Crameri_Bamako_gp,               // 44 - Bamako
    Crameri_Acton_gp,                // 45 - Acton
    Crameri_Batlow_gp,               // 46 - Batlow
    Crameri_Bilbao_gp,               // 47 - Bilbao
    Crameri_Buda_gp,                 // 48 - Buda
    Crameri_Davos_gp,                // 49 - Davos
    Crameri_GrayC_gp,                // 50 - GrayC
    Crameri_Imola_gp,                // 51 - Imola
    Crameri_LaPaz_gp,                // 52 - La Paz
    Crameri_Nuuk_gp,                 // 53 - Nuuk
    Crameri_Oslo_gp,                 // 54 - Oslo
    Crameri_Tofino_gp,               // 55 - Tofino
    Crameri_Turku_gp,                // 56 - Turku

    // -------------------------------------------------------------------------
    // R COLORSPACE PALETTES (57-74) - 18 palettes
    // -------------------------------------------------------------------------
    viridis_gp,                      // 57 - Viridis
    plasma_gp,                       // 58 - Plasma
    inferno_gp,                      // 59 - Inferno
    magma_gp,                        // 60 - Magma
    cubhelix_gp,                     // 61 - Cubhelix
    abyss_gp,                        // 62 - Abyss
    bathy_gp,                        // 63 - Bathy
    ocean_gp,                        // 64 - Ocean
    nighttime_gp,                    // 65 - Nighttime
    seafloor_gp,                     // 66 - Seafloor
    ibcso_gp,                        // 67 - IBCSO
    copper_gp,                       // 68 - Copper
    hot_gp,                          // 69 - Hot
    cool_gp,                         // 70 - Cool
    earth_gp,                        // 71 - Earth
    sealand_gp,                      // 72 - Sealand
    split_gp,                        // 73 - Split
    red2green_gp                     // 74 - Red2Green
};

// =============================================================================
// PALETTE NAMES
// =============================================================================

const char* const MasterPaletteNames[] = {
    // cpt-city (0-32)
    "Sunset Real",
    "Rivendell",
    "Ocean Breeze 036",
    "RGI 15",
    "Retro 2",
    "Analogous 1",
    "Pink Splash 08",
    "Coral Reef",
    "Ocean Breeze 068",
    "Pink Splash 07",
    "Vintage 01",
    "Departure",
    "Landscape 64",
    "Landscape 33",
    "Rainbow Sherbet",
    "GR65 Hult",
    "GR64 Hult",
    "GMT Dry Wet",
    "IB Jul01",
    "Vintage 57",
    "IB15",
    "Fuschia 7",
    "Emerald Dragon",
    "Lava",
    "Fire",
    "Colorful",
    "Magenta Evening",
    "Pink Purple",
    "Autumn 19",
    "Blue Magenta White",
    "Black Magenta Red",
    "Red Magenta Yellow",
    "Blue Cyan Yellow",
    // Crameri (33-56)
    "Vik",
    "Tokyo",
    "Roma",
    "Oleron",
    "Lisbon",
    "La Jolla",
    "Hawaii",
    "Devon",
    "Cork",
    "Broc",
    "Berlin",
    "Bamako",
    "Acton",
    "Batlow",
    "Bilbao",
    "Buda",
    "Davos",
    "GrayC",
    "Imola",
    "La Paz",
    "Nuuk",
    "Oslo",
    "Tofino",
    "Turku",
    // R Colorspace (57-74)
    "Viridis",
    "Plasma",
    "Inferno",
    "Magma",
    "Cubhelix",
    "Abyss",
    "Bathy",
    "Ocean",
    "Nighttime",
    "Seafloor",
    "IBCSO",
    "Copper",
    "Hot",
    "Cool",
    "Earth",
    "Sealand",
    "Split",
    "Red2Green"
};

// =============================================================================
// PALETTE FLAGS - 75 entries
// =============================================================================

const uint8_t master_palette_flags[] = {
    // cpt-city (0-32)
    PAL_WARM | PAL_VIVID,           //  0 Sunset Real
    PAL_COOL | PAL_CALM,            //  1 Rivendell
    PAL_COOL | PAL_CALM,            //  2 Ocean Breeze 036
    PAL_COOL | PAL_HIGH_SAT | PAL_VIVID, //  3 RGI 15
    PAL_WARM | PAL_VIVID,           //  4 Retro 2
    PAL_COOL | PAL_HIGH_SAT | PAL_VIVID, //  5 Analogous 1
    PAL_WARM | PAL_HIGH_SAT | PAL_VIVID, //  6 Pink Splash 08
    PAL_COOL | PAL_CALM,            //  7 Coral Reef
    PAL_COOL | PAL_CALM,            //  8 Ocean Breeze 068
    PAL_WARM | PAL_HIGH_SAT | PAL_VIVID, //  9 Pink Splash 07
    PAL_WARM | PAL_CALM,            // 10 Vintage 01
    PAL_WARM | PAL_WHITE_HEAVY | PAL_VIVID, // 11 Departure
    PAL_COOL | PAL_CALM,            // 12 Landscape 64
    PAL_WARM | PAL_CALM,            // 13 Landscape 33
    PAL_WARM | PAL_HIGH_SAT | PAL_VIVID, // 14 Rainbow Sherbet
    PAL_COOL | PAL_HIGH_SAT | PAL_VIVID, // 15 GR65 Hult
    PAL_COOL | PAL_CALM,            // 16 GR64 Hult
    PAL_COOL | PAL_CALM,            // 17 GMT Dry Wet
    PAL_WARM | PAL_HIGH_SAT | PAL_VIVID, // 18 IB Jul01
    PAL_WARM | PAL_CALM,            // 19 Vintage 57
    PAL_WARM | PAL_HIGH_SAT | PAL_VIVID, // 20 IB15
    PAL_COOL | PAL_HIGH_SAT | PAL_VIVID, // 21 Fuschia 7
    PAL_COOL | PAL_HIGH_SAT | PAL_VIVID, // 22 Emerald Dragon
    PAL_WARM | PAL_WHITE_HEAVY | PAL_VIVID, // 23 Lava
    PAL_WARM | PAL_WHITE_HEAVY | PAL_VIVID, // 24 Fire
    PAL_COOL | PAL_HIGH_SAT | PAL_VIVID, // 25 Colorful
    PAL_WARM | PAL_HIGH_SAT | PAL_VIVID, // 26 Magenta Evening
    PAL_COOL | PAL_WHITE_HEAVY | PAL_VIVID, // 27 Pink Purple
    PAL_WARM | PAL_CALM,            // 28 Autumn 19
    PAL_COOL | PAL_WHITE_HEAVY | PAL_HIGH_SAT | PAL_VIVID, // 29 Blue Magenta White
    PAL_WARM | PAL_HIGH_SAT | PAL_VIVID, // 30 Black Magenta Red
    PAL_WARM | PAL_HIGH_SAT | PAL_VIVID, // 31 Red Magenta Yellow
    PAL_COOL | PAL_HIGH_SAT | PAL_VIVID, // 32 Blue Cyan Yellow
    // Crameri (33-56)
    PAL_COOL | PAL_WARM | PAL_WHITE_HEAVY | PAL_CALM | PAL_CVD_FRIENDLY, // 33 Vik
    PAL_COOL | PAL_CALM | PAL_CVD_FRIENDLY, // 34 Tokyo
    PAL_WARM | PAL_COOL | PAL_CALM | PAL_CVD_FRIENDLY, // 35 Roma
    PAL_COOL | PAL_WARM | PAL_WHITE_HEAVY | PAL_CALM | PAL_CVD_FRIENDLY, // 36 Oleron
    PAL_COOL | PAL_CALM | PAL_CVD_FRIENDLY, // 37 Lisbon
    PAL_WARM | PAL_WHITE_HEAVY | PAL_CALM | PAL_CVD_FRIENDLY, // 38 La Jolla
    PAL_COOL | PAL_WARM | PAL_CALM | PAL_CVD_FRIENDLY, // 39 Hawaii
    PAL_COOL | PAL_WHITE_HEAVY | PAL_CALM | PAL_CVD_FRIENDLY, // 40 Devon
    PAL_COOL | PAL_WHITE_HEAVY | PAL_CALM | PAL_CVD_FRIENDLY, // 41 Cork
    PAL_COOL | PAL_WHITE_HEAVY | PAL_CALM | PAL_CVD_FRIENDLY, // 42 Broc
    PAL_COOL | PAL_WARM | PAL_CALM | PAL_CVD_FRIENDLY, // 43 Berlin
    PAL_WARM | PAL_CALM | PAL_CVD_FRIENDLY, // 44 Bamako
    PAL_COOL | PAL_CALM | PAL_CVD_FRIENDLY, // 45 Acton
    PAL_COOL | PAL_WARM | PAL_CALM | PAL_CVD_FRIENDLY, // 46 Batlow
    PAL_WARM | PAL_WHITE_HEAVY | PAL_CALM | PAL_CVD_FRIENDLY, // 47 Bilbao
    PAL_WARM | PAL_WHITE_HEAVY | PAL_CALM | PAL_CVD_FRIENDLY, // 48 Buda
    PAL_COOL | PAL_WHITE_HEAVY | PAL_CALM | PAL_CVD_FRIENDLY, // 49 Davos
    PAL_WHITE_HEAVY | PAL_CALM | PAL_CVD_FRIENDLY | PAL_EXCLUDED, // 50 GrayC (grayscale - excluded)
    PAL_COOL | PAL_CALM | PAL_CVD_FRIENDLY, // 51 Imola
    PAL_COOL | PAL_WHITE_HEAVY | PAL_CALM | PAL_CVD_FRIENDLY, // 52 La Paz
    PAL_COOL | PAL_WHITE_HEAVY | PAL_CALM | PAL_CVD_FRIENDLY, // 53 Nuuk
    PAL_COOL | PAL_WHITE_HEAVY | PAL_CALM | PAL_CVD_FRIENDLY, // 54 Oslo
    PAL_COOL | PAL_CALM | PAL_CVD_FRIENDLY, // 55 Tofino
    PAL_WARM | PAL_WHITE_HEAVY | PAL_CALM | PAL_CVD_FRIENDLY, // 56 Turku
    // R Colorspace (57-74)
    PAL_COOL | PAL_CVD_FRIENDLY,    // 57 Viridis
    PAL_WARM | PAL_VIVID | PAL_CVD_FRIENDLY, // 58 Plasma
    PAL_WARM | PAL_VIVID | PAL_CVD_FRIENDLY, // 59 Inferno
    PAL_WARM | PAL_CALM | PAL_CVD_FRIENDLY, // 60 Magma
    PAL_CALM | PAL_CVD_FRIENDLY,    // 61 Cubhelix
    PAL_COOL | PAL_CALM | PAL_CVD_FRIENDLY, // 62 Abyss
    PAL_COOL | PAL_CALM | PAL_CVD_FRIENDLY, // 63 Bathy
    PAL_COOL | PAL_CALM | PAL_CVD_FRIENDLY, // 64 Ocean
    PAL_COOL | PAL_CALM | PAL_CVD_FRIENDLY, // 65 Nighttime
    PAL_COOL | PAL_CALM | PAL_CVD_FRIENDLY, // 66 Seafloor
    PAL_COOL | PAL_CALM | PAL_CVD_FRIENDLY, // 67 IBCSO
    PAL_WARM | PAL_CALM | PAL_CVD_FRIENDLY, // 68 Copper
    PAL_WARM | PAL_VIVID | PAL_CVD_FRIENDLY, // 69 Hot
    PAL_COOL | PAL_VIVID | PAL_CVD_FRIENDLY, // 70 Cool
    PAL_WARM | PAL_CALM | PAL_CVD_FRIENDLY, // 71 Earth
    PAL_COOL | PAL_CALM | PAL_CVD_FRIENDLY, // 72 Sealand
    PAL_COOL | PAL_WARM | PAL_CALM | PAL_CVD_FRIENDLY, // 73 Split
    PAL_WARM | PAL_VIVID | PAL_CVD_FRIENDLY  // 74 Red2Green
};

// =============================================================================
// PALETTE AVERAGE BRIGHTNESS - 75 entries
// =============================================================================

const uint8_t master_palette_avg_Y[] = {
    120, 80, 90, 120, 110, 130, 140, 120, 90, 160,  // 0-9
    90, 160, 110, 90, 170, 140, 100, 130, 140, 110,  // 10-19
    140, 150, 130, 180, 200, 170, 130, 150, 110, 200, // 20-29
    140, 160, 180,  // 30-32
    170, 150, 160, 170, 120, 160, 170, 180, 170, 160, // 33-42
    140, 160, 170, 170, 170, 170, 170, 160, 160, 160, // 43-52
    160, 170, 140, 160,  // 53-56
    160, 150, 140, 130, 150, 80, 100, 110, 70, 100,   // 57-66
    90, 140, 180, 180, 140, 130, 140, 160  // 67-74
};

// =============================================================================
// PALETTE MAX BRIGHTNESS (power safety caps) - 75 entries
// =============================================================================

const uint8_t master_palette_max_brightness[] = {
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, // 0-9
    255, 200, 255, 255, 230, 255, 255, 255, 255, 255, // 10-19
    255, 255, 255, 180, 160, 230, 255, 230, 255, 180, // 20-29
    255, 255, 230,  // 30-32
    220, 255, 255, 220, 230, 230, 230, 220, 220, 220, // 33-42
    230, 255, 230, 230, 230, 230, 220, 220, 230, 230, // 43-52
    230, 220, 230, 230,  // 53-56
    255, 255, 240, 240, 255, 255, 255, 255, 255, 255, // 57-66
    255, 200, 200, 220, 220, 220, 200, 200  // 67-74
};

// =============================================================================
// COMPILE-TIME SAFETY CHECKS
// =============================================================================

static_assert(
    (sizeof(gMasterPalettes) / sizeof(gMasterPalettes[0])) == MASTER_PALETTE_COUNT,
    "gMasterPalettes[] must have exactly 75 entries!"
);

static_assert(
    (sizeof(MasterPaletteNames) / sizeof(MasterPaletteNames[0])) == MASTER_PALETTE_COUNT,
    "MasterPaletteNames[] must have exactly 75 entries!"
);

static_assert(
    (sizeof(master_palette_flags) / sizeof(master_palette_flags[0])) == MASTER_PALETTE_COUNT,
    "master_palette_flags[] must have exactly 75 entries!"
);

static_assert(
    (sizeof(master_palette_avg_Y) / sizeof(master_palette_avg_Y[0])) == MASTER_PALETTE_COUNT,
    "master_palette_avg_Y[] must have exactly 75 entries!"
);

static_assert(
    (sizeof(master_palette_max_brightness) / sizeof(master_palette_max_brightness[0])) == MASTER_PALETTE_COUNT,
    "master_palette_max_brightness[] must have exactly 75 entries!"
);

} // namespace palettes
} // namespace lightwaveos
