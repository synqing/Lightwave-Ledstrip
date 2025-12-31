#include "../../src/core/state/SystemState.h"
#include <algorithm>

namespace lightwaveos {
namespace state {

// Default constructor with safe initial values
// Matches v1 defaults for backward compatibility
SystemState::SystemState()
    : version(0)
    , currentEffectId(0)
    , currentPaletteId(0)
    , brightness(128)        // Start at 50% brightness
    , speed(15)              // Medium speed
    , gHue(0)                // Start at red
    , intensity(128)         // Medium intensity
    , saturation(255)        // Full saturation
    , complexity(128)        // Medium complexity
    , variation(128)         // Medium variation
    , zoneModeEnabled(false) // Zone mode off by default
    , activeZoneCount(1)     // Single zone
    , transitionActive(false)
    , transitionType(0)
    , transitionProgress(0)
{
    // Initialize all zones with defaults
    for (auto& zone : zones) {
        zone = ZoneState();
    }
}

// ==================== Functional Update Methods ====================

SystemState SystemState::withEffect(uint8_t effectId) const {
    SystemState newState = *this;
    newState.currentEffectId = effectId;
    newState.version++;
    return newState;
}

SystemState SystemState::withBrightness(uint8_t value) const {
    SystemState newState = *this;
    newState.brightness = value;
    newState.version++;
    return newState;
}

SystemState SystemState::withPalette(uint8_t paletteId) const {
    SystemState newState = *this;
    newState.currentPaletteId = paletteId;
    newState.version++;
    return newState;
}

SystemState SystemState::withSpeed(uint8_t value) const {
    SystemState newState = *this;
    // Clamp speed to valid range (1-100)
    newState.speed = std::max(static_cast<uint8_t>(1),
                              std::min(value, static_cast<uint8_t>(100)));
    newState.version++;
    return newState;
}

SystemState SystemState::withZoneEnabled(uint8_t zoneId, bool enabled) const {
    if (zoneId >= MAX_ZONES) {
        return *this;  // Invalid zone, return unchanged
    }

    SystemState newState = *this;
    newState.zones[zoneId].enabled = enabled;
    newState.version++;
    return newState;
}

SystemState SystemState::withZoneEffect(uint8_t zoneId, uint8_t effectId) const {
    if (zoneId >= MAX_ZONES) {
        return *this;  // Invalid zone, return unchanged
    }

    SystemState newState = *this;
    newState.zones[zoneId].effectId = effectId;
    newState.version++;
    return newState;
}

SystemState SystemState::withZonePalette(uint8_t zoneId, uint8_t paletteId) const {
    if (zoneId >= MAX_ZONES) {
        return *this;  // Invalid zone, return unchanged
    }

    SystemState newState = *this;
    newState.zones[zoneId].paletteId = paletteId;
    newState.version++;
    return newState;
}

SystemState SystemState::withZoneBrightness(uint8_t zoneId, uint8_t brightness) const {
    if (zoneId >= MAX_ZONES) {
        return *this;  // Invalid zone, return unchanged
    }

    SystemState newState = *this;
    newState.zones[zoneId].brightness = brightness;
    newState.version++;
    return newState;
}

SystemState SystemState::withZoneSpeed(uint8_t zoneId, uint8_t speed) const {
    if (zoneId >= MAX_ZONES) {
        return *this;  // Invalid zone, return unchanged
    }

    SystemState newState = *this;
    // Clamp speed to valid range (1-100)
    newState.zones[zoneId].speed = std::max(static_cast<uint8_t>(1),
                                             std::min(speed, static_cast<uint8_t>(100)));
    newState.version++;
    return newState;
}

SystemState SystemState::withZoneMode(bool enabled, uint8_t zoneCount) const {
    SystemState newState = *this;
    newState.zoneModeEnabled = enabled;
    // Clamp zone count to valid range (1-4)
    newState.activeZoneCount = std::max(static_cast<uint8_t>(1),
                                         std::min(zoneCount, MAX_ZONES));
    newState.version++;
    return newState;
}

SystemState SystemState::withTransition(uint8_t type, uint8_t progress) const {
    SystemState newState = *this;
    newState.transitionActive = true;
    newState.transitionType = type;
    newState.transitionProgress = progress;
    newState.version++;
    return newState;
}

SystemState SystemState::withTransitionStarted(uint8_t type) const {
    SystemState newState = *this;
    newState.transitionActive = true;
    newState.transitionType = type;
    newState.transitionProgress = 0;
    newState.version++;
    return newState;
}

SystemState SystemState::withTransitionCompleted() const {
    SystemState newState = *this;
    newState.transitionActive = false;
    newState.transitionProgress = 255;
    newState.version++;
    return newState;
}

SystemState SystemState::withIncrementedHue() const {
    SystemState newState = *this;
    newState.gHue = (newState.gHue + 1) & 0xFF;  // Wrap at 255
    newState.version++;
    return newState;
}

SystemState SystemState::withVisualParams(uint8_t intensity, uint8_t saturation,
                                           uint8_t complexity, uint8_t variation) const {
    SystemState newState = *this;
    newState.intensity = intensity;
    newState.saturation = saturation;
    newState.complexity = complexity;
    newState.variation = variation;
    newState.version++;
    return newState;
}

SystemState SystemState::withIntensity(uint8_t value) const {
    SystemState newState = *this;
    newState.intensity = value;
    newState.version++;
    return newState;
}

SystemState SystemState::withSaturation(uint8_t value) const {
    SystemState newState = *this;
    newState.saturation = value;
    newState.version++;
    return newState;
}

SystemState SystemState::withComplexity(uint8_t value) const {
    SystemState newState = *this;
    newState.complexity = value;
    newState.version++;
    return newState;
}

SystemState SystemState::withVariation(uint8_t value) const {
    SystemState newState = *this;
    newState.variation = value;
    newState.version++;
    return newState;
}

} // namespace state
} // namespace lightwaveos
