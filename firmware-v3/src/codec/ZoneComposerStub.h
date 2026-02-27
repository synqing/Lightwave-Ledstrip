// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once

#ifdef NATIVE_BUILD

#include <stdint.h>

namespace lightwaveos {
namespace zones {

struct ZoneSegment {
    uint8_t zoneId;
    uint8_t s1LeftStart;
    uint8_t s1LeftEnd;
    uint8_t s1RightStart;
    uint8_t s1RightEnd;
    uint8_t totalLeds;
};

class ZoneComposer {
public:
    bool isEnabled() const { return enabled; }
    uint8_t getZoneCount() const { return zoneCount; }
    const ZoneSegment* getZoneConfig() const { return zoneConfig; }
    bool isZoneEnabled(uint8_t) const { return true; }
    uint8_t getZoneEffect(uint8_t) const { return zoneEffect; }
    uint8_t getZoneBrightness(uint8_t) const { return zoneBrightness; }
    uint8_t getZoneSpeed(uint8_t) const { return zoneSpeed; }
    uint8_t getZonePalette(uint8_t) const { return zonePalette; }
    uint8_t getZoneBlendMode(uint8_t) const { return zoneBlendMode; }
    static const char* getPresetName(uint8_t) { return "Preset"; }

    bool enabled = true;
    uint8_t zoneCount = 1;
    ZoneSegment zoneConfig[4] = {
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0}
    };
    uint8_t zoneEffect = 7;
    uint8_t zoneBrightness = 140;
    uint8_t zoneSpeed = 33;
    uint8_t zonePalette = 4;
    uint8_t zoneBlendMode = 5;
};

inline const char* getBlendModeName(uint8_t) {
    return "Alpha";
}

} // namespace zones
} // namespace lightwaveos

#endif
