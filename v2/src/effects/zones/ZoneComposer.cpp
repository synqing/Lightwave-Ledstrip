/**
 * @file ZoneComposer.cpp
 * @brief Multi-zone effect orchestration implementation
 *
 * LightwaveOS v2 - Zone System
 */

#include "ZoneComposer.h"
#include <Arduino.h>
#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace zones {

// ==================== Preset Definitions ====================

struct ZonePreset {
    const char* name;
    ZoneLayout layout;
    ZoneState zones[MAX_ZONES];
};

// 5 Built-in presets
static const ZonePreset PRESETS[] = {
    // Preset 0: Single Zone (unified)
    {
        .name = "Unified",
        .layout = ZoneLayout::TRIPLE,
        .zones = {
            { .effectId = 0, .brightness = 255, .speed = 15, .paletteId = 0,
              .blendMode = BlendMode::OVERWRITE, .enabled = true },
            { .effectId = 0, .brightness = 255, .speed = 15, .paletteId = 0,
              .blendMode = BlendMode::OVERWRITE, .enabled = false },
            { .effectId = 0, .brightness = 255, .speed = 15, .paletteId = 0,
              .blendMode = BlendMode::OVERWRITE, .enabled = false },
            { .effectId = 0, .brightness = 255, .speed = 15, .paletteId = 0,
              .blendMode = BlendMode::OVERWRITE, .enabled = false }
        }
    },

    // Preset 1: Dual Split (center vs outer)
    {
        .name = "Dual Split",
        .layout = ZoneLayout::TRIPLE,
        .zones = {
            { .effectId = 0, .brightness = 255, .speed = 15, .paletteId = 0,
              .blendMode = BlendMode::OVERWRITE, .enabled = true },   // Fire center
            { .effectId = 1, .brightness = 200, .speed = 20, .paletteId = 0,
              .blendMode = BlendMode::ADDITIVE, .enabled = true },    // Ocean middle
            { .effectId = 1, .brightness = 200, .speed = 20, .paletteId = 0,
              .blendMode = BlendMode::ADDITIVE, .enabled = false },   // Outer disabled
            { .effectId = 0, .brightness = 255, .speed = 15, .paletteId = 0,
              .blendMode = BlendMode::OVERWRITE, .enabled = false }
        }
    },

    // Preset 2: Triple Rings (default)
    {
        .name = "Triple Rings",
        .layout = ZoneLayout::TRIPLE,
        .zones = {
            { .effectId = 7, .brightness = 255, .speed = 20, .paletteId = 0,
              .blendMode = BlendMode::OVERWRITE, .enabled = true },   // Wave center
            { .effectId = 10, .brightness = 220, .speed = 25, .paletteId = 0,
              .blendMode = BlendMode::ADDITIVE, .enabled = true },    // Interference middle
            { .effectId = 12, .brightness = 180, .speed = 30, .paletteId = 0,
              .blendMode = BlendMode::ADDITIVE, .enabled = true },    // Pulse outer
            { .effectId = 0, .brightness = 255, .speed = 15, .paletteId = 0,
              .blendMode = BlendMode::OVERWRITE, .enabled = false }
        }
    },

    // Preset 3: Quad Active
    {
        .name = "Quad Active",
        .layout = ZoneLayout::QUAD,
        .zones = {
            { .effectId = 0, .brightness = 255, .speed = 15, .paletteId = 0,
              .blendMode = BlendMode::OVERWRITE, .enabled = true },   // Fire innermost
            { .effectId = 2, .brightness = 230, .speed = 20, .paletteId = 0,
              .blendMode = BlendMode::ADDITIVE, .enabled = true },    // Plasma ring 2
            { .effectId = 7, .brightness = 200, .speed = 25, .paletteId = 0,
              .blendMode = BlendMode::ADDITIVE, .enabled = true },    // Wave ring 3
            { .effectId = 1, .brightness = 170, .speed = 30, .paletteId = 0,
              .blendMode = BlendMode::ADDITIVE, .enabled = true }     // Ocean outermost
        }
    },

    // Preset 4: Heartbeat Focus
    {
        .name = "Heartbeat Focus",
        .layout = ZoneLayout::TRIPLE,
        .zones = {
            { .effectId = 9, .brightness = 255, .speed = 15, .paletteId = 0,
              .blendMode = BlendMode::OVERWRITE, .enabled = true },   // Heartbeat center
            { .effectId = 11, .brightness = 150, .speed = 10, .paletteId = 0,
              .blendMode = BlendMode::ALPHA, .enabled = true },       // Breathing middle
            { .effectId = 11, .brightness = 100, .speed = 8, .paletteId = 0,
              .blendMode = BlendMode::ALPHA, .enabled = true },       // Breathing outer
            { .effectId = 0, .brightness = 255, .speed = 15, .paletteId = 0,
              .blendMode = BlendMode::OVERWRITE, .enabled = false }
        }
    }
};

static const uint8_t NUM_PRESETS = sizeof(PRESETS) / sizeof(PRESETS[0]);

// ==================== Constructor ====================

ZoneComposer::ZoneComposer()
    : m_enabled(false)
    , m_initialized(false)
    , m_layout(ZoneLayout::TRIPLE)     // Default layout (use SINGLE for single-zone perf)
    , m_zoneCount(3)                    // Matches TRIPLE layout
    , m_zoneConfig(ZONE_3_CONFIG)
    , m_renderer(nullptr)
{
    // Initialize zones to defaults
    for (uint8_t i = 0; i < MAX_ZONES; i++) {
        m_zones[i] = {
            .effectId = 0,
            .brightness = 255,
            .speed = 15,
            .paletteId = 0,
            .blendMode = BlendMode::OVERWRITE,
            .enabled = (i == 0)  // Only zone 0 enabled by default
        };
    }

    // Clear buffers
    memset(m_tempBuffer, 0, sizeof(m_tempBuffer));
    memset(m_outputBuffer, 0, sizeof(m_outputBuffer));
}

// ==================== Initialization ====================

bool ZoneComposer::init(RendererActor* renderer) {
    if (!renderer) {
        Serial.println("[ZoneComposer] ERROR: Null renderer");
        return false;
    }

    m_renderer = renderer;
    m_initialized = true;

    Serial.println("[ZoneComposer] Initialized");
    return true;
}

// ==================== Rendering ====================

void ZoneComposer::render(CRGB* leds, uint16_t numLeds, CRGBPalette16* palette,
                          uint8_t hue, uint32_t frameCount) {
    if (!m_initialized || !m_enabled) {
        return;
    }

    // Optimization: Skip buffer clear if first enabled zone uses OVERWRITE blend mode
    // (it will completely overwrite the buffer anyway)
    bool needsClear = true;
    for (uint8_t z = 0; z < m_zoneCount; z++) {
        if (m_zones[z].enabled) {
            if (m_zones[z].blendMode == BlendMode::OVERWRITE) {
                needsClear = false;
            }
            break;  // Only check first enabled zone
        }
    }
    if (needsClear) {
        memset(m_outputBuffer, 0, sizeof(m_outputBuffer));
    }

    // Render each enabled zone
    for (uint8_t z = 0; z < m_zoneCount; z++) {
        if (m_zones[z].enabled) {
            renderZone(z, leds, numLeds, palette, hue, frameCount);
        }
    }

    // Copy composited output to main buffer
    memcpy(leds, m_outputBuffer, numLeds * sizeof(CRGB));
}

void ZoneComposer::renderZone(uint8_t zoneId, CRGB* leds, uint16_t numLeds,
                               CRGBPalette16* palette, uint8_t hue, uint32_t frameCount) {
    if (zoneId >= m_zoneCount) return;

    const ZoneState& zone = m_zones[zoneId];
    const ZoneSegment& seg = m_zoneConfig[zoneId];

    // Get the IEffect instance
    plugins::IEffect* effect = m_renderer->getEffectInstance(zone.effectId);
    if (!effect) {
        return;
    }

    // Optimization: Only clear zone regions in temp buffer (not full 320 LEDs)
    // This saves ~1.9KB memset per zone for non-overlapping zones
    uint16_t s1ClearStart = min(seg.s1LeftStart, seg.s1RightStart);
    uint16_t s1ClearEnd = max(seg.s1LeftEnd, seg.s1RightEnd);
    memset(&m_tempBuffer[s1ClearStart], 0, (s1ClearEnd - s1ClearStart + 1) * sizeof(CRGB));
    // Strip 2 mirror region
    uint16_t s2ClearStart = s1ClearStart + STRIP_LENGTH;
    uint16_t s2ClearEnd = s1ClearEnd + STRIP_LENGTH;
    if (s2ClearEnd < TOTAL_LEDS) {
        memset(&m_tempBuffer[s2ClearStart], 0, (s2ClearEnd - s2ClearStart + 1) * sizeof(CRGB));
    }

    // Create EffectContext for this zone's effect
    // Use zone-specific speed and potentially zone-specific palette
    CRGBPalette16 zonePalette = *palette;  // Copy global palette
    // TODO: Support zone-specific palettes

    plugins::EffectContext ctx;
    ctx.leds = m_tempBuffer;
    ctx.ledCount = numLeds;
    ctx.centerPoint = 79;  // Center point for LGP
    ctx.palette = plugins::PaletteRef(&zonePalette);
    ctx.brightness = zone.brightness;
    ctx.speed = zone.speed;
    ctx.gHue = hue;
    ctx.intensity = 128;  // Default values
    ctx.saturation = 255;
    ctx.complexity = 128;
    ctx.variation = 0;
    ctx.frameNumber = frameCount;
    ctx.totalTimeMs = frameCount * 8;  // ~8ms per frame at 120 FPS
    ctx.deltaTimeMs = 8;  // ~120 FPS
    ctx.zoneId = zoneId;  // Zone ID for zone-aware effects
    // Provide actual zone boundaries for zone-aware effects (forward compatible)
    ctx.zoneStart = seg.s1LeftStart;
    ctx.zoneLength = seg.totalLeds;

    // Render effect to temp buffer
    effect->render(ctx);

    // Extract zone segment and apply brightness
    // Then composite into output buffer

    // Process Strip 1 left segment
    for (uint8_t i = seg.s1LeftStart; i <= seg.s1LeftEnd && i < STRIP_LENGTH; i++) {
        CRGB pixel = m_tempBuffer[i];
        pixel.nscale8(zone.brightness);
        m_outputBuffer[i] = blendPixels(m_outputBuffer[i], pixel, zone.blendMode);
    }

    // Process Strip 1 right segment
    for (uint8_t i = seg.s1RightStart; i <= seg.s1RightEnd && i < STRIP_LENGTH; i++) {
        CRGB pixel = m_tempBuffer[i];
        pixel.nscale8(zone.brightness);
        m_outputBuffer[i] = blendPixels(m_outputBuffer[i], pixel, zone.blendMode);
    }

    // Process Strip 2 (indices 160-319, mirrored from Strip 1)
    for (uint8_t i = seg.s1LeftStart; i <= seg.s1LeftEnd && i < STRIP_LENGTH; i++) {
        uint16_t s2Idx = i + STRIP_LENGTH;
        if (s2Idx < TOTAL_LEDS) {
            CRGB pixel = m_tempBuffer[s2Idx];
            pixel.nscale8(zone.brightness);
            m_outputBuffer[s2Idx] = blendPixels(m_outputBuffer[s2Idx], pixel, zone.blendMode);
        }
    }

    for (uint8_t i = seg.s1RightStart; i <= seg.s1RightEnd && i < STRIP_LENGTH; i++) {
        uint16_t s2Idx = i + STRIP_LENGTH;
        if (s2Idx < TOTAL_LEDS) {
            CRGB pixel = m_tempBuffer[s2Idx];
            pixel.nscale8(zone.brightness);
            m_outputBuffer[s2Idx] = blendPixels(m_outputBuffer[s2Idx], pixel, zone.blendMode);
        }
    }
}

// ==================== Zone Control ====================

void ZoneComposer::setLayout(ZoneLayout layout) {
    m_layout = layout;
    m_zoneCount = lightwaveos::zones::getZoneCount(layout);
    m_zoneConfig = lightwaveos::zones::getZoneConfig(layout);

    Serial.printf("[ZoneComposer] Layout set to %d zones\n", m_zoneCount);
}

// ==================== Per-Zone Settings ====================

void ZoneComposer::setZoneEffect(uint8_t zone, uint8_t effectId) {
    if (zone < MAX_ZONES) {
        m_zones[zone].effectId = effectId;
    }
}

void ZoneComposer::setZoneBrightness(uint8_t zone, uint8_t brightness) {
    if (zone < MAX_ZONES) {
        m_zones[zone].brightness = brightness;
    }
}

void ZoneComposer::setZoneSpeed(uint8_t zone, uint8_t speed) {
    if (zone < MAX_ZONES) {
        m_zones[zone].speed = constrain(speed, 1, 100);
    }
}

void ZoneComposer::setZonePalette(uint8_t zone, uint8_t paletteId) {
    if (zone < MAX_ZONES) {
        m_zones[zone].paletteId = paletteId;
    }
}

void ZoneComposer::setZoneBlendMode(uint8_t zone, BlendMode mode) {
    if (zone < MAX_ZONES) {
        m_zones[zone].blendMode = mode;
    }
}

void ZoneComposer::setZoneEnabled(uint8_t zone, bool enabled) {
    if (zone < MAX_ZONES) {
        m_zones[zone].enabled = enabled;
    }
}

// ==================== Getters ====================

uint8_t ZoneComposer::getZoneEffect(uint8_t zone) const {
    return (zone < MAX_ZONES) ? m_zones[zone].effectId : 0;
}

uint8_t ZoneComposer::getZoneBrightness(uint8_t zone) const {
    return (zone < MAX_ZONES) ? m_zones[zone].brightness : 0;
}

uint8_t ZoneComposer::getZoneSpeed(uint8_t zone) const {
    return (zone < MAX_ZONES) ? m_zones[zone].speed : 0;
}

uint8_t ZoneComposer::getZonePalette(uint8_t zone) const {
    return (zone < MAX_ZONES) ? m_zones[zone].paletteId : 0;
}

BlendMode ZoneComposer::getZoneBlendMode(uint8_t zone) const {
    return (zone < MAX_ZONES) ? m_zones[zone].blendMode : BlendMode::OVERWRITE;
}

bool ZoneComposer::isZoneEnabled(uint8_t zone) const {
    return (zone < MAX_ZONES) ? m_zones[zone].enabled : false;
}

// ==================== Presets ====================

void ZoneComposer::loadPreset(uint8_t presetId) {
    if (presetId >= NUM_PRESETS) {
        Serial.printf("[ZoneComposer] Invalid preset %d\n", presetId);
        return;
    }

    const ZonePreset& preset = PRESETS[presetId];

    setLayout(preset.layout);

    for (uint8_t i = 0; i < MAX_ZONES; i++) {
        m_zones[i] = preset.zones[i];
    }

    Serial.printf("[ZoneComposer] Loaded preset: %s\n", preset.name);
}

const char* ZoneComposer::getPresetName(uint8_t presetId) {
    if (presetId >= NUM_PRESETS) return "Unknown";
    return PRESETS[presetId].name;
}

// ==================== Debug ====================

void ZoneComposer::printStatus() const {
    Serial.println("\n=== Zone Composer Status ===");
    Serial.printf("Enabled: %s\n", m_enabled ? "YES" : "NO");
    Serial.printf("Layout: %d zones\n", m_zoneCount);

    for (uint8_t z = 0; z < m_zoneCount; z++) {
        const ZoneState& zone = m_zones[z];
        const ZoneSegment& seg = m_zoneConfig[z];

        Serial.printf("\nZone %d: %s\n", z, zone.enabled ? "ENABLED" : "disabled");
        Serial.printf("  Effect: %d\n", zone.effectId);
        Serial.printf("  Brightness: %d\n", zone.brightness);
        Serial.printf("  Speed: %d\n", zone.speed);
        Serial.printf("  Blend: %s\n", getBlendModeName(zone.blendMode));
        Serial.printf("  LEDs: %d-%d + %d-%d (%d total)\n",
                      seg.s1LeftStart, seg.s1LeftEnd,
                      seg.s1RightStart, seg.s1RightEnd,
                      seg.totalLeds);
    }
    Serial.println();
}

} // namespace zones
} // namespace lightwaveos
