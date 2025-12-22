#ifndef ZONE_DEFINITION_H
#define ZONE_DEFINITION_H

#include <stdint.h>

// Zone Definition - Center-Origin Layout
// All zones are symmetric around center LEDs 79/80
// Each zone has left and right segments for dual-strip architecture
struct ZoneDefinition {
    uint8_t zoneId;               // Zone identifier (0-3)

    // Strip 1 LED ranges (symmetric around center 79/80)
    uint8_t strip1StartLeft;      // Left segment start index
    uint8_t strip1EndLeft;        // Left segment end index (inclusive)
    uint8_t strip1StartRight;     // Right segment start index
    uint8_t strip1EndRight;       // Right segment end index (inclusive)

    // Strip 2 LED ranges (same layout)
    uint8_t strip2StartLeft;      // Left segment start index
    uint8_t strip2EndLeft;        // Left segment end index (inclusive)
    uint8_t strip2StartRight;     // Right segment start index
    uint8_t strip2EndRight;       // Right segment end index (inclusive)

    uint8_t totalLEDs;            // Total LEDs in this zone (should be 40)
};

// 3-Zone Configuration (AURA spec: 30+90+40 LEDs)
// Zone 0 (center, 30 LEDs):     65-94 (continuous, no split)
// Zone 1 (middle, 90 LEDs):     20-64 (45) + 95-139 (45)
// Zone 2 (outer, 40 LEDs):      0-19 (20) + 140-159 (20)
static const ZoneDefinition ZONE_CONFIGS_3ZONE[3] = {
    // Zone 0 - Center (continuous, 30 LEDs)
    {
        .zoneId = 0,
        .strip1StartLeft = 65,
        .strip1EndLeft = 94,
        .strip1StartRight = 65,  // Continuous zone, no split
        .strip1EndRight = 94,
        .strip2StartLeft = 65,
        .strip2EndLeft = 94,
        .strip2StartRight = 65,
        .strip2EndRight = 94,
        .totalLEDs = 30
    },
    // Zone 1 - Middle (split, 90 LEDs = 45+45)
    {
        .zoneId = 1,
        .strip1StartLeft = 20,
        .strip1EndLeft = 64,
        .strip1StartRight = 95,
        .strip1EndRight = 139,
        .strip2StartLeft = 20,
        .strip2EndLeft = 64,
        .strip2StartRight = 95,
        .strip2EndRight = 139,
        .totalLEDs = 90
    },
    // Zone 2 - Outer (split, 40 LEDs = 20+20)
    {
        .zoneId = 2,
        .strip1StartLeft = 0,
        .strip1EndLeft = 19,
        .strip1StartRight = 140,
        .strip1EndRight = 159,
        .strip2StartLeft = 0,
        .strip2EndLeft = 19,
        .strip2StartRight = 140,
        .strip2EndRight = 159,
        .totalLEDs = 40
    }
};

// 4-Zone Configuration (Equal 40 LEDs each)
// Zone 0 (innermost, 40 LEDs):  60-79 (20) + 80-99 (20)
// Zone 1 (40 LEDs):             40-59 (20) + 100-119 (20)
// Zone 2 (40 LEDs):             20-39 (20) + 120-139 (20)
// Zone 3 (outermost, 40 LEDs):  0-19 (20) + 140-159 (20)
static const ZoneDefinition ZONE_CONFIGS_4ZONE[4] = {
    // Zone 0 - Innermost ring (center origin)
    {
        .zoneId = 0,
        .strip1StartLeft = 60,
        .strip1EndLeft = 79,
        .strip1StartRight = 80,
        .strip1EndRight = 99,
        .strip2StartLeft = 60,
        .strip2EndLeft = 79,
        .strip2StartRight = 80,
        .strip2EndRight = 99,
        .totalLEDs = 40
    },
    // Zone 1 - Second ring
    {
        .zoneId = 1,
        .strip1StartLeft = 40,
        .strip1EndLeft = 59,
        .strip1StartRight = 100,
        .strip1EndRight = 119,
        .strip2StartLeft = 40,
        .strip2EndLeft = 59,
        .strip2StartRight = 100,
        .strip2EndRight = 119,
        .totalLEDs = 40
    },
    // Zone 2 - Third ring
    {
        .zoneId = 2,
        .strip1StartLeft = 20,
        .strip1EndLeft = 39,
        .strip1StartRight = 120,
        .strip1EndRight = 139,
        .strip2StartLeft = 20,
        .strip2EndLeft = 39,
        .strip2StartRight = 120,
        .strip2EndRight = 139,
        .totalLEDs = 40
    },
    // Zone 3 - Outermost ring
    {
        .zoneId = 3,
        .strip1StartLeft = 0,
        .strip1EndLeft = 19,
        .strip1StartRight = 140,
        .strip1EndRight = 159,
        .strip2StartLeft = 0,
        .strip2EndLeft = 19,
        .strip2StartRight = 140,
        .strip2EndRight = 159,
        .totalLEDs = 40
    }
};

#endif // ZONE_DEFINITION_H
