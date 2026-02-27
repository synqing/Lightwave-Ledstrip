// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file ZoneDefinition.h
 * @brief Zone layout definitions for CENTER ORIGIN multi-zone system
 *
 * LightwaveOS v2 - Zone System
 *
 * All zones are symmetric around CENTER PAIR (LEDs 79/80), radiating outward.
 * Supports 1-zone, 2-zone, 3-zone and 4-zone configurations.
 */

#pragma once

#include <cstdint>

namespace lightwaveos {
namespace zones {

// ==================== Constants ====================

constexpr uint8_t MAX_ZONES = 4;
constexpr uint16_t STRIP_LENGTH = 160;
constexpr uint16_t TOTAL_LEDS = 320;

// ==================== Zone Segment Definition ====================

/**
 * @brief Defines LED indices for a single zone
 *
 * Each zone has left and right segments on each strip.
 * All zones are CENTER ORIGIN compliant.
 */
struct ZoneSegment {
    uint8_t zoneId;

    // Strip 1 segments
    uint8_t s1LeftStart;    // Left segment start (toward LED 0)
    uint8_t s1LeftEnd;      // Left segment end (inclusive)
    uint8_t s1RightStart;   // Right segment start (toward LED 159)
    uint8_t s1RightEnd;     // Right segment end (inclusive)

    // Strip 2 mirrors Strip 1 (add 160 to indices)
    // s2LeftStart = s1LeftStart + 160, etc.

    uint8_t totalLeds;      // Total LEDs in this zone
};

// ==================== 1-Zone Configuration ====================

/**
 * 1-Zone Layout (Unified):
 * Entire strip is one zone.
 */
constexpr ZoneSegment ZONE_1_CONFIG[1] = {
    { .zoneId = 0,
      .s1LeftStart = 0, .s1LeftEnd = 79,
      .s1RightStart = 80, .s1RightEnd = 159,
      .totalLeds = 160 }
};

// ==================== 2-Zone Configuration ====================

/**
 * 2-Zone Layout (Dual Split):
 * Zone 0 = inner half (near centre), Zone 1 = outer half.
 * Each zone is 40 LEDs per side (80 total).
 */
constexpr ZoneSegment ZONE_2_CONFIG[2] = {
    // Zone 0: INNER (80 LEDs total)
    { .zoneId = 0,
      .s1LeftStart = 40, .s1LeftEnd = 79,
      .s1RightStart = 80, .s1RightEnd = 119,
      .totalLeds = 80 },

    // Zone 1: OUTER (80 LEDs total)
    { .zoneId = 1,
      .s1LeftStart = 0, .s1LeftEnd = 39,
      .s1RightStart = 120, .s1RightEnd = 159,
      .totalLeds = 80 }
};

// ==================== 3-Zone Configuration ====================
// CENTER ORIGIN: Zone 0 at center, Zone 2 at edges

/**
 * 3-Zone Layout (AURA Spec):
 *
 * Zone 0 (CENTER):  LEDs 65-94  (30 LEDs) - Innermost ring
 * Zone 1 (MIDDLE):  LEDs 20-64 + 95-139 (90 LEDs) - Middle ring
 * Zone 2 (OUTER):   LEDs 0-19 + 140-159 (40 LEDs) - Outermost ring
 *
 *    ZONE 2    |   ZONE 1   |  ZONE 0  |   ZONE 1   |    ZONE 2
 *   [0----19]  | [20----64] | [65--94] | [95---139] | [140---159]
 *              |            |  CENTER  |            |
 */
constexpr ZoneSegment ZONE_3_CONFIG[3] = {
    // Zone 0: CENTER (30 LEDs)
    { .zoneId = 0,
      .s1LeftStart = 65, .s1LeftEnd = 79,
      .s1RightStart = 80, .s1RightEnd = 94,
      .totalLeds = 30 },

    // Zone 1: MIDDLE (90 LEDs = 45 left + 45 right)
    { .zoneId = 1,
      .s1LeftStart = 20, .s1LeftEnd = 64,
      .s1RightStart = 95, .s1RightEnd = 139,
      .totalLeds = 90 },

    // Zone 2: OUTER (40 LEDs = 20 left + 20 right)
    { .zoneId = 2,
      .s1LeftStart = 0, .s1LeftEnd = 19,
      .s1RightStart = 140, .s1RightEnd = 159,
      .totalLeds = 40 }
};

// ==================== 4-Zone Configuration ====================
// Equal 40 LEDs per zone, concentric rings from center

/**
 * 4-Zone Layout (Equal Distribution):
 *
 * Zone 0 (INNERMOST):  LEDs 60-79 + 80-99  (40 LEDs)
 * Zone 1 (RING 2):     LEDs 40-59 + 100-119 (40 LEDs)
 * Zone 2 (RING 3):     LEDs 20-39 + 120-139 (40 LEDs)
 * Zone 3 (OUTERMOST):  LEDs 0-19 + 140-159 (40 LEDs)
 *
 *   Z3    |  Z2   |  Z1   |  Z0  |  Z0  |  Z1   |  Z2   |   Z3
 * [0--19] [20-39] [40-59] [60-79|80-99] [100-119] [120-139] [140-159]
 *                         CENTER PAIR
 */
constexpr ZoneSegment ZONE_4_CONFIG[4] = {
    // Zone 0: INNERMOST (40 LEDs)
    { .zoneId = 0,
      .s1LeftStart = 60, .s1LeftEnd = 79,
      .s1RightStart = 80, .s1RightEnd = 99,
      .totalLeds = 40 },

    // Zone 1: RING 2 (40 LEDs)
    { .zoneId = 1,
      .s1LeftStart = 40, .s1LeftEnd = 59,
      .s1RightStart = 100, .s1RightEnd = 119,
      .totalLeds = 40 },

    // Zone 2: RING 3 (40 LEDs)
    { .zoneId = 2,
      .s1LeftStart = 20, .s1LeftEnd = 39,
      .s1RightStart = 120, .s1RightEnd = 139,
      .totalLeds = 40 },

    // Zone 3: OUTERMOST (40 LEDs)
    { .zoneId = 3,
      .s1LeftStart = 0, .s1LeftEnd = 19,
      .s1RightStart = 140, .s1RightEnd = 159,
      .totalLeds = 40 }
};

// ==================== Zone Configuration Type ====================

enum class ZoneLayout : uint8_t {
    SINGLE = 1,     // All LEDs as one zone
    DUAL   = 2,     // 2 zones (inner/outer) (default)
    TRIPLE = 3,     // 3 concentric zones
    QUAD   = 4      // 4 equal zones
};

/**
 * @brief Get zone configuration for a layout
 * @param layout The zone layout type
 * @return Pointer to zone segment array
 */
inline const ZoneSegment* getZoneConfig(ZoneLayout layout) {
    switch (layout) {
        case ZoneLayout::SINGLE: return ZONE_1_CONFIG;
        case ZoneLayout::DUAL:   return ZONE_2_CONFIG;
        case ZoneLayout::TRIPLE: return ZONE_3_CONFIG;
        case ZoneLayout::QUAD:   return ZONE_4_CONFIG;
        default:                 return ZONE_2_CONFIG;
    }
}

/**
 * @brief Get zone count for a layout
 * @param layout The zone layout type
 * @return Number of zones (1, 2, 3, or 4)
 */
inline uint8_t getZoneCount(ZoneLayout layout) {
    return static_cast<uint8_t>(layout);
}

} // namespace zones
} // namespace lightwaveos
