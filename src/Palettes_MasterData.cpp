// Palettes_MasterData.cpp - Palette metadata array definitions
// This file provides external linkage for metadata arrays used across translation units

#include <stdint.h>

// Palette flag definitions (must match Palettes_Master.h)
#define PAL_WARM        0x01
#define PAL_COOL        0x02
#define PAL_HIGH_SAT    0x04
#define PAL_WHITE_HEAVY 0x08
#define PAL_CALM        0x10
#define PAL_VIVID       0x20

// =============================================================================
// PALETTE FLAGS - 75 entries
// =============================================================================
extern const uint8_t master_palette_flags[] = {
  // cpt-city (0-32)
  /*  0 Sunset Real        */ PAL_WARM | PAL_VIVID,
  /*  1 Rivendell          */ PAL_COOL | PAL_CALM,
  /*  2 Ocean Breeze 036   */ PAL_COOL | PAL_CALM,
  /*  3 RGI 15             */ PAL_COOL | PAL_HIGH_SAT | PAL_VIVID,
  /*  4 Retro 2            */ PAL_WARM | PAL_VIVID,
  /*  5 Analogous 1        */ PAL_COOL | PAL_HIGH_SAT | PAL_VIVID,
  /*  6 Pink Splash 08     */ PAL_WARM | PAL_HIGH_SAT | PAL_VIVID,
  /*  7 Coral Reef         */ PAL_COOL | PAL_CALM,
  /*  8 Ocean Breeze 068   */ PAL_COOL | PAL_CALM,
  /*  9 Pink Splash 07     */ PAL_WARM | PAL_HIGH_SAT | PAL_VIVID,
  /* 10 Vintage 01         */ PAL_WARM | PAL_CALM,
  /* 11 Departure          */ PAL_WARM | PAL_WHITE_HEAVY | PAL_VIVID,
  /* 12 Landscape 64       */ PAL_COOL | PAL_CALM,
  /* 13 Landscape 33       */ PAL_WARM | PAL_CALM,
  /* 14 Rainbow Sherbet    */ PAL_WARM | PAL_HIGH_SAT | PAL_VIVID,
  /* 15 GR65 Hult          */ PAL_COOL | PAL_HIGH_SAT | PAL_VIVID,
  /* 16 GR64 Hult          */ PAL_COOL | PAL_CALM,
  /* 17 GMT Dry Wet        */ PAL_COOL | PAL_CALM,
  /* 18 IB Jul01           */ PAL_WARM | PAL_HIGH_SAT | PAL_VIVID,
  /* 19 Vintage 57         */ PAL_WARM | PAL_CALM,
  /* 20 IB15               */ PAL_WARM | PAL_HIGH_SAT | PAL_VIVID,
  /* 21 Fuschia 7          */ PAL_COOL | PAL_HIGH_SAT | PAL_VIVID,
  /* 22 Emerald Dragon     */ PAL_COOL | PAL_HIGH_SAT | PAL_VIVID,
  /* 23 Lava               */ PAL_WARM | PAL_WHITE_HEAVY | PAL_VIVID,
  /* 24 Fire               */ PAL_WARM | PAL_WHITE_HEAVY | PAL_VIVID,
  /* 25 Colorful           */ PAL_COOL | PAL_HIGH_SAT | PAL_VIVID,
  /* 26 Magenta Evening    */ PAL_WARM | PAL_HIGH_SAT | PAL_VIVID,
  /* 27 Pink Purple        */ PAL_COOL | PAL_WHITE_HEAVY | PAL_VIVID,
  /* 28 Autumn 19          */ PAL_WARM | PAL_CALM,
  /* 29 Blue Magenta White */ PAL_COOL | PAL_WHITE_HEAVY | PAL_HIGH_SAT | PAL_VIVID,
  /* 30 Black Magenta Red  */ PAL_WARM | PAL_HIGH_SAT | PAL_VIVID,
  /* 31 Red Magenta Yellow */ PAL_WARM | PAL_HIGH_SAT | PAL_VIVID,
  /* 32 Blue Cyan Yellow   */ PAL_COOL | PAL_HIGH_SAT | PAL_VIVID,
  // Crameri (33-56) - Scientific palettes, generally calmer
  /* 33 Vik               */ PAL_COOL | PAL_WARM | PAL_WHITE_HEAVY | PAL_CALM,
  /* 34 Tokyo             */ PAL_COOL | PAL_CALM,
  /* 35 Roma              */ PAL_WARM | PAL_COOL | PAL_CALM,
  /* 36 Oleron            */ PAL_COOL | PAL_WARM | PAL_WHITE_HEAVY | PAL_CALM,
  /* 37 Lisbon            */ PAL_COOL | PAL_CALM,
  /* 38 La Jolla          */ PAL_WARM | PAL_WHITE_HEAVY | PAL_CALM,
  /* 39 Hawaii            */ PAL_COOL | PAL_WARM | PAL_CALM,
  /* 40 Devon             */ PAL_COOL | PAL_WHITE_HEAVY | PAL_CALM,
  /* 41 Cork              */ PAL_COOL | PAL_WHITE_HEAVY | PAL_CALM,
  /* 42 Broc              */ PAL_COOL | PAL_WHITE_HEAVY | PAL_CALM,
  /* 43 Berlin            */ PAL_COOL | PAL_WARM | PAL_CALM,
  /* 44 Bamako            */ PAL_WARM | PAL_CALM,
  /* 45 Acton             */ PAL_COOL | PAL_CALM,
  /* 46 Batlow            */ PAL_COOL | PAL_WARM | PAL_CALM,
  /* 47 Bilbao            */ PAL_WARM | PAL_WHITE_HEAVY | PAL_CALM,
  /* 48 Buda              */ PAL_WARM | PAL_WHITE_HEAVY | PAL_CALM,
  /* 49 Davos             */ PAL_COOL | PAL_WHITE_HEAVY | PAL_CALM,
  /* 50 GrayC             */ PAL_WHITE_HEAVY | PAL_CALM,
  /* 51 Imola             */ PAL_COOL | PAL_CALM,
  /* 52 La Paz            */ PAL_COOL | PAL_WHITE_HEAVY | PAL_CALM,
  /* 53 Nuuk              */ PAL_COOL | PAL_WHITE_HEAVY | PAL_CALM,
  /* 54 Oslo              */ PAL_COOL | PAL_WHITE_HEAVY | PAL_CALM,
  /* 55 Tofino            */ PAL_COOL | PAL_CALM,
  /* 56 Turku             */ PAL_WARM | PAL_WHITE_HEAVY | PAL_CALM,
  // R Colorspace (57-74) - LGP-optimized palettes
  // Tier 1: Naturally LGP-safe (57-67)
  /* 57 Viridis           */ PAL_COOL,
  /* 58 Plasma            */ PAL_WARM | PAL_VIVID,
  /* 59 Inferno           */ PAL_WARM | PAL_VIVID,
  /* 60 Magma             */ PAL_WARM | PAL_CALM,
  /* 61 Cubhelix          */ PAL_CALM,
  /* 62 Abyss             */ PAL_COOL | PAL_CALM,
  /* 63 Bathy             */ PAL_COOL | PAL_CALM,
  /* 64 Ocean             */ PAL_COOL | PAL_CALM,
  /* 65 Nighttime         */ PAL_COOL | PAL_CALM,
  /* 66 Seafloor          */ PAL_COOL | PAL_CALM,
  /* 67 IBCSO             */ PAL_COOL | PAL_CALM,
  // Tier 2: Chromatic-shifted (68-74)
  /* 68 Copper            */ PAL_WARM | PAL_CALM,
  /* 69 Hot               */ PAL_WARM | PAL_VIVID,
  /* 70 Cool              */ PAL_COOL | PAL_VIVID,
  /* 71 Earth             */ PAL_WARM | PAL_CALM,
  /* 72 Sealand           */ PAL_COOL | PAL_CALM,
  /* 73 Split             */ PAL_COOL | PAL_WARM | PAL_CALM,
  /* 74 Red2Green         */ PAL_WARM | PAL_VIVID
};

// =============================================================================
// PALETTE AVERAGE BRIGHTNESS - 75 entries
// =============================================================================
extern const uint8_t master_palette_avg_Y[] = {
  // cpt-city (0-32)
  /*  0 Sunset Real        */ 120,
  /*  1 Rivendell          */  80,
  /*  2 Ocean Breeze 036   */  90,
  /*  3 RGI 15             */ 120,
  /*  4 Retro 2            */ 110,
  /*  5 Analogous 1        */ 130,
  /*  6 Pink Splash 08     */ 140,
  /*  7 Coral Reef         */ 120,
  /*  8 Ocean Breeze 068   */  90,
  /*  9 Pink Splash 07     */ 160,
  /* 10 Vintage 01         */  90,
  /* 11 Departure          */ 160,
  /* 12 Landscape 64       */ 110,
  /* 13 Landscape 33       */  90,
  /* 14 Rainbow Sherbet    */ 170,
  /* 15 GR65 Hult          */ 140,
  /* 16 GR64 Hult          */ 100,
  /* 17 GMT Dry Wet        */ 130,
  /* 18 IB Jul01           */ 140,
  /* 19 Vintage 57         */ 110,
  /* 20 IB15               */ 140,
  /* 21 Fuschia 7          */ 150,
  /* 22 Emerald Dragon     */ 130,
  /* 23 Lava               */ 180,
  /* 24 Fire               */ 200,
  /* 25 Colorful           */ 170,
  /* 26 Magenta Evening    */ 130,
  /* 27 Pink Purple        */ 150,
  /* 28 Autumn 19          */ 110,
  /* 29 Blue Magenta White */ 200,
  /* 30 Black Magenta Red  */ 140,
  /* 31 Red Magenta Yellow */ 160,
  /* 32 Blue Cyan Yellow   */ 180,
  // Crameri (33-56)
  /* 33 Vik               */ 170,
  /* 34 Tokyo             */ 150,
  /* 35 Roma              */ 160,
  /* 36 Oleron            */ 170,
  /* 37 Lisbon            */ 120,
  /* 38 La Jolla          */ 160,
  /* 39 Hawaii            */ 170,
  /* 40 Devon             */ 180,
  /* 41 Cork              */ 170,
  /* 42 Broc              */ 160,
  /* 43 Berlin            */ 140,
  /* 44 Bamako            */ 160,
  /* 45 Acton             */ 170,
  /* 46 Batlow            */ 170,
  /* 47 Bilbao            */ 170,
  /* 48 Buda              */ 170,
  /* 49 Davos             */ 170,
  /* 50 GrayC             */ 160,
  /* 51 Imola             */ 160,
  /* 52 La Paz            */ 160,
  /* 53 Nuuk              */ 160,
  /* 54 Oslo              */ 170,
  /* 55 Tofino            */ 140,
  /* 56 Turku             */ 160,
  // R Colorspace (57-74)
  // Tier 1: Naturally LGP-safe - darker palettes
  /* 57 Viridis           */ 160,
  /* 58 Plasma            */ 150,
  /* 59 Inferno           */ 140,
  /* 60 Magma             */ 130,
  /* 61 Cubhelix          */ 150,
  /* 62 Abyss             */  80,   // Very dark ocean
  /* 63 Bathy             */ 100,
  /* 64 Ocean             */ 110,
  /* 65 Nighttime         */  70,   // Darkest - ambient
  /* 66 Seafloor          */ 100,
  /* 67 IBCSO             */  90,
  // Tier 2: Chromatic-shifted (brighter due to saturation retention)
  /* 68 Copper            */ 140,
  /* 69 Hot               */ 180,
  /* 70 Cool              */ 180,
  /* 71 Earth             */ 140,
  /* 72 Sealand           */ 130,
  /* 73 Split             */ 140,
  /* 74 Red2Green         */ 160
};

// =============================================================================
// PALETTE MAX BRIGHTNESS (power safety caps) - 75 entries
// =============================================================================
extern const uint8_t master_palette_max_brightness[] = {
  // cpt-city (0-32)
  /*  0 Sunset Real        */ 255,
  /*  1 Rivendell          */ 255,
  /*  2 Ocean Breeze 036   */ 255,
  /*  3 RGI 15             */ 255,
  /*  4 Retro 2            */ 255,
  /*  5 Analogous 1        */ 255,
  /*  6 Pink Splash 08     */ 255,
  /*  7 Coral Reef         */ 255,
  /*  8 Ocean Breeze 068   */ 255,
  /*  9 Pink Splash 07     */ 255,
  /* 10 Vintage 01         */ 255,
  /* 11 Departure          */ 200,
  /* 12 Landscape 64       */ 255,
  /* 13 Landscape 33       */ 255,
  /* 14 Rainbow Sherbet    */ 230,
  /* 15 GR65 Hult          */ 255,
  /* 16 GR64 Hult          */ 255,
  /* 17 GMT Dry Wet        */ 255,
  /* 18 IB Jul01           */ 255,
  /* 19 Vintage 57         */ 255,
  /* 20 IB15               */ 255,
  /* 21 Fuschia 7          */ 255,
  /* 22 Emerald Dragon     */ 255,
  /* 23 Lava               */ 180,
  /* 24 Fire               */ 160,
  /* 25 Colorful           */ 230,
  /* 26 Magenta Evening    */ 255,
  /* 27 Pink Purple        */ 230,
  /* 28 Autumn 19          */ 255,
  /* 29 Blue Magenta White */ 180,
  /* 30 Black Magenta Red  */ 255,
  /* 31 Red Magenta Yellow */ 255,
  /* 32 Blue Cyan Yellow   */ 230,
  // Crameri (33-56)
  /* 33 Vik               */ 220,
  /* 34 Tokyo             */ 255,
  /* 35 Roma              */ 255,
  /* 36 Oleron            */ 220,
  /* 37 Lisbon            */ 230,
  /* 38 La Jolla          */ 230,
  /* 39 Hawaii            */ 230,
  /* 40 Devon             */ 220,
  /* 41 Cork              */ 220,
  /* 42 Broc              */ 220,
  /* 43 Berlin            */ 230,
  /* 44 Bamako            */ 255,
  /* 45 Acton             */ 230,
  /* 46 Batlow            */ 230,
  /* 47 Bilbao            */ 230,
  /* 48 Buda              */ 230,
  /* 49 Davos             */ 220,
  /* 50 GrayC             */ 220,
  /* 51 Imola             */ 230,
  /* 52 La Paz            */ 230,
  /* 53 Nuuk              */ 230,
  /* 54 Oslo              */ 220,
  /* 55 Tofino            */ 230,
  /* 56 Turku             */ 230,
  // R Colorspace (57-74)
  // Tier 1: Naturally LGP-safe - no caps needed (dark palettes)
  /* 57 Viridis           */ 255,
  /* 58 Plasma            */ 255,
  /* 59 Inferno           */ 240,   // Cream endpoint slightly bright
  /* 60 Magma             */ 240,   // Cream endpoint slightly bright
  /* 61 Cubhelix          */ 255,
  /* 62 Abyss             */ 255,   // Very dark - no cap
  /* 63 Bathy             */ 255,
  /* 64 Ocean             */ 255,
  /* 65 Nighttime         */ 255,   // Dark ambient
  /* 66 Seafloor          */ 255,
  /* 67 IBCSO             */ 255,
  // Tier 2: Chromatic-shifted - capped for power (bright saturated endpoints)
  /* 68 Copper            */ 200,
  /* 69 Hot               */ 200,
  /* 70 Cool              */ 220,
  /* 71 Earth             */ 220,
  /* 72 Sealand           */ 220,
  /* 73 Split             */ 200,   // Diverging, watch center
  /* 74 Red2Green         */ 200    // Diverging, watch center
};
