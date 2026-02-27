// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file ZoneDefinition.h
 * @brief Zone layout definitions for Tab5.encoder
 *
 * Matches LightwaveOS v2 ZoneSegment structure for compatibility.
 * All zones are symmetric around CENTER PAIR (LEDs 79/80), radiating outward.
 */

#pragma once

#include <cstdint>

namespace zones {

// ==================== Constants ====================

constexpr uint8_t MAX_ZONES = 4;
constexpr uint8_t CENTER_LEFT = 79;
constexpr uint8_t CENTER_RIGHT = 80;
constexpr uint8_t MAX_LED = 159;

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

    uint8_t totalLeds;      // Total LEDs in this zone
};

} // namespace zones

