/**
 * @file ZoneComposer.cpp
 * @brief Multi-zone effect orchestration implementation
 *
 * LightwaveOS v2 - Zone System
 */

#include "ZoneComposer.h"
#include <Arduino.h>
#include <math.h>
#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../palettes/Palettes_Master.h"
#if FEATURE_VALIDATION_PROFILING
#include "../../core/system/ValidationProfiler.h"
#endif

#ifndef NATIVE_BUILD
#if FEATURE_VALIDATION_PROFILING
extern "C" {
#include <esp_timer.h>
}
#endif
#endif

namespace lightwaveos {
namespace zones {

namespace {
constexpr float kMinSpeedTimeFactor = 0.04f;  // ~3-5 FPS feel at speed 1

float computeSpeedTimeFactor(uint8_t speed) {
    if (lightwaveos::actors::LedConfig::MAX_SPEED <= 1) {
        return 1.0f;
    }
    float norm = 0.0f;
    if (speed > 1) {
        norm = (static_cast<float>(speed - 1) /
                static_cast<float>(lightwaveos::actors::LedConfig::MAX_SPEED - 1));
        if (norm > 1.0f) norm = 1.0f;
    }
    float curved = sqrtf(norm);
    return kMinSpeedTimeFactor + (1.0f - kMinSpeedTimeFactor) * curved;
}
}  // namespace

// ==================== Preset Definitions ====================

struct ZonePreset {
    const char* name;
    const ZoneSegment* segments;  // Pointer to segment array
    uint8_t zoneCount;             // Number of zones in this preset
    ZoneState zones[MAX_ZONES];
};

// 5 Built-in presets
static const ZonePreset PRESETS[] = {
    // Preset 0: Single Zone (unified)
    {
        .name = "Unified",
        .segments = ZONE_3_CONFIG,
        .zoneCount = 3,
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
        .segments = ZONE_3_CONFIG,
        .zoneCount = 3,
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

    // Preset 2: Triple Rings (audio-reactive LGP effects)
    {
        .name = "Triple Rings",
        .segments = ZONE_3_CONFIG,
        .zoneCount = 3,
        .zones = {
            { .effectId = 17, .brightness = 255, .speed = 20, .paletteId = 0,
              .blendMode = BlendMode::OVERWRITE, .enabled = true },   // LGP Wave Collision center (audio: heavyBass)
            { .effectId = 16, .brightness = 220, .speed = 25, .paletteId = 0,
              .blendMode = BlendMode::ADDITIVE, .enabled = true },    // LGP Interference Scanner middle (audio: heavyMid)
            { .effectId = 24, .brightness = 180, .speed = 30, .paletteId = 0,
              .blendMode = BlendMode::ADDITIVE, .enabled = true },    // LGP Star Burst outer (audio: full pipeline)
            { .effectId = 0, .brightness = 255, .speed = 15, .paletteId = 0,
              .blendMode = BlendMode::OVERWRITE, .enabled = false }
        }
    },

    // Preset 3: Quad Active
    {
        .name = "Quad Active",
        .segments = ZONE_4_CONFIG,
        .zoneCount = 4,
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
        .segments = ZONE_3_CONFIG,
        .zoneCount = 3,
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
    , m_zoneCount(2)                    // Default to 2 zones (DUAL)
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
    
    // Initialize default layout (DUAL) by copying from compile-time config
    memcpy(m_zoneConfig, ZONE_2_CONFIG, sizeof(ZONE_2_CONFIG));

    // Clear buffers
    memset(m_zoneBuffers, 0, sizeof(m_zoneBuffers));
    memset(m_outputBuffer, 0, sizeof(m_outputBuffer));
    for (uint8_t i = 0; i < MAX_ZONES; i++) {
        m_zoneTimeSeconds[i] = 0.0f;
        m_zoneFrameAccumulator[i] = 0.0f;
        m_zoneFrameCount[i] = 0;
    }
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
                          uint8_t hue, uint32_t frameCount, uint32_t deltaTimeMs,
                          const plugins::AudioContext* audioCtx) {
    if (!m_initialized || !m_enabled) {
        return;
    }

    // Copy palette once per frame (reused for all zones to avoid stack allocations)
    m_zonePalette = *palette;

    // Initialize zone context once per frame (reused for all zones)
    // Base fields that are the same for all zones (leds set per-zone in renderZone)
    m_zoneContext.ledCount = numLeds;
    m_zoneContext.centerPoint = 79;  // Center point for LGP
    m_zoneContext.palette = plugins::PaletteRef(&m_zonePalette);
    m_zoneContext.gHue = hue;
    m_zoneContext.intensity = 128;  // Default values
    m_zoneContext.saturation = 255;
    m_zoneContext.complexity = 128;
    m_zoneContext.variation = 0;
    m_zoneContext.frameNumber = frameCount;
    // Base delta in seconds for per-zone scaling.
    float deltaSeconds = static_cast<float>(deltaTimeMs) * 0.001f;
    if (deltaSeconds > 0.05f) deltaSeconds = 0.05f;  // Maximum 50ms (20 FPS floor)
    m_zoneContext.deltaTimeSeconds = deltaSeconds;
    m_zoneContext.deltaTimeMs = static_cast<uint32_t>(deltaSeconds * 1000.0f + 0.5f);
    m_zoneContext.totalTimeMs = 0;

    // Copy audio context once per frame if available (reused for all zones)
    if (audioCtx != nullptr) {
        m_zoneContext.audio = *audioCtx;
    } else {
        // Reset audio context if not available
        m_zoneContext.audio = plugins::AudioContext();
    }

    // Always clear output buffer to prevent stale pixels from previous frames
    // (OVERWRITE blend mode only overwrites zone segments, not the full 320 LEDs)
    memset(m_outputBuffer, 0, sizeof(m_outputBuffer));

    // Render each enabled zone
    for (uint8_t z = 0; z < m_zoneCount; z++) {
        if (m_zones[z].enabled) {
            renderZone(z, leds, numLeds, hue, frameCount, deltaSeconds);
        }
    }

    // Copy composited output to main buffer
    memcpy(leds, m_outputBuffer, numLeds * sizeof(CRGB));
}

void ZoneComposer::renderZone(uint8_t zoneId, CRGB* leds, uint16_t numLeds,
                               uint8_t hue, uint32_t frameCount, float deltaSeconds) {
    // Validate against both m_zoneCount and MAX_ZONES for safety
    if (zoneId >= m_zoneCount || zoneId >= MAX_ZONES) return;

    (void)leds;
    (void)numLeds;
    (void)hue;
    (void)frameCount;

    // Validate zoneId to prevent out-of-bounds access
    uint8_t safeZone = validateZoneId(zoneId);
    
    // DEFENSIVE CHECK: Double-verify bounds before array access
    if (safeZone >= MAX_ZONES) {
        safeZone = 0;  // Fallback to zone 0 if still out of bounds
    }
    if (safeZone >= m_zoneCount) {
        safeZone = 0;  // Also check against actual zone count
    }
    
    const ZoneState& zone = m_zones[safeZone];
    const ZoneSegment& seg = m_zoneConfig[safeZone];

    // Get the IEffect instance
    plugins::IEffect* effect = m_renderer->getEffectInstance(zone.effectId);
    if (!effect) {
        return;
    }

    // Per-zone persistent render buffer (preserves trails/smoothing and prevents
    // cross-zone contamination).
    CRGB* zoneBuffer = m_zoneBuffers[safeZone];

    // Update zone-specific fields in the reusable context
    // (Base fields were already set in render() once per frame)
    m_zoneContext.leds = zoneBuffer;
    m_zoneContext.brightness = zone.brightness;
    m_zoneContext.speed = zone.speed;
    m_zoneContext.zoneId = zoneId;  // Zone ID for zone-aware effects
    // Apply speed-scaled time for this zone.
    float speedFactor = computeSpeedTimeFactor(zone.speed);
    float scaledDeltaSeconds = deltaSeconds * speedFactor;

    m_zoneTimeSeconds[safeZone] += scaledDeltaSeconds;
    m_zoneFrameAccumulator[safeZone] += speedFactor;
    if (m_zoneFrameAccumulator[safeZone] >= 1.0f) {
        uint32_t advance = static_cast<uint32_t>(m_zoneFrameAccumulator[safeZone]);
        m_zoneFrameAccumulator[safeZone] -= static_cast<float>(advance);
        m_zoneFrameCount[safeZone] += advance;
    }

    m_zoneContext.deltaTimeSeconds = scaledDeltaSeconds;
    m_zoneContext.deltaTimeMs = static_cast<uint32_t>(scaledDeltaSeconds * 1000.0f + 0.5f);
    m_zoneContext.frameNumber = m_zoneFrameCount[safeZone];
    m_zoneContext.totalTimeMs = static_cast<uint32_t>(m_zoneTimeSeconds[safeZone] * 1000.0f + 0.5f);
    // Provide actual zone boundaries for zone-aware effects (forward compatible)
    m_zoneContext.zoneStart = seg.s1LeftStart;
    m_zoneContext.zoneLength = seg.totalLeds;
    
    // Per-zone palette support: if paletteId == 0, use global palette; otherwise use zone-specific palette
    if (zone.paletteId == 0) {
        // Use global palette (already set in render())
        m_zoneContext.palette = plugins::PaletteRef(&m_zonePalette);
    } else {
        // Load zone-specific palette from master palette collection
        // DEFENSIVE CHECK: Validate palette ID before accessing gMasterPalettes array
        uint8_t safePaletteId = lightwaveos::palettes::validatePaletteId(zone.paletteId);
        
        // DEFENSIVE CHECK: Double-verify bounds before array access
        if (safePaletteId >= lightwaveos::palettes::MASTER_PALETTE_COUNT) {
            safePaletteId = 0;  // Fallback to palette 0 if still out of bounds
        }
        
        // DEFENSIVE CHECK: Verify zone ID is within bounds before accessing m_zonePalettes array
        if (safeZone >= MAX_ZONES) {
            safeZone = 0;  // Fallback to zone 0 if still out of bounds
        }
        
        // Now safe to access arrays with validated indices
        m_zonePalettes[safeZone] = CRGBPalette16(lightwaveos::palettes::gMasterPalettes[safePaletteId]);
        m_zoneContext.palette = plugins::PaletteRef(&m_zonePalettes[safeZone]);
    }

    // Render effect into this zone's persistent buffer
    effect->render(m_zoneContext);

    // Extract zone segment and apply brightness
    // Then composite into output buffer

    // Process Strip 1 left segment
    // DEFENSIVE CHECK: Validate indices against both STRIP_LENGTH and numLeds
    for (uint8_t i = seg.s1LeftStart; i <= seg.s1LeftEnd && i < STRIP_LENGTH && i < numLeds; i++) {
        CRGB pixel = zoneBuffer[i];
        pixel.nscale8(zone.brightness);
        m_outputBuffer[i] = blendPixels(m_outputBuffer[i], pixel, zone.blendMode);
    }

    // Process Strip 1 right segment
    // DEFENSIVE CHECK: Validate indices against both STRIP_LENGTH and numLeds
    for (uint8_t i = seg.s1RightStart; i <= seg.s1RightEnd && i < STRIP_LENGTH && i < numLeds; i++) {
        CRGB pixel = zoneBuffer[i];
        pixel.nscale8(zone.brightness);
        m_outputBuffer[i] = blendPixels(m_outputBuffer[i], pixel, zone.blendMode);
    }

    // Process Strip 2 (indices 160-319, mirrored from Strip 1)
    // DEFENSIVE CHECK: Validate indices against numLeds (not just TOTAL_LEDS constant)
    for (uint8_t i = seg.s1LeftStart; i <= seg.s1LeftEnd && i < STRIP_LENGTH; i++) {
        uint16_t s2Idx = i + STRIP_LENGTH;
        if (s2Idx < numLeds && s2Idx < TOTAL_LEDS) {
            CRGB pixel = zoneBuffer[s2Idx];
            pixel.nscale8(zone.brightness);
            m_outputBuffer[s2Idx] = blendPixels(m_outputBuffer[s2Idx], pixel, zone.blendMode);
        }
    }

    for (uint8_t i = seg.s1RightStart; i <= seg.s1RightEnd && i < STRIP_LENGTH; i++) {
        uint16_t s2Idx = i + STRIP_LENGTH;
        if (s2Idx < numLeds && s2Idx < TOTAL_LEDS) {
            CRGB pixel = zoneBuffer[s2Idx];
            pixel.nscale8(zone.brightness);
            m_outputBuffer[s2Idx] = blendPixels(m_outputBuffer[s2Idx], pixel, zone.blendMode);
        }
    }
}

// ==================== Zone Control ====================

bool ZoneComposer::setLayout(const ZoneSegment* segments, uint8_t count) {
    if (!segments || count == 0 || count > MAX_ZONES) {
        Serial.printf("[ZoneComposer] ERROR: Invalid layout parameters (count=%d)\n", count);
        return false;
    }
    
    // Validate the layout before applying
    if (!validateLayout(segments, count)) {
        Serial.println("[ZoneComposer] ERROR: Layout validation failed");
        return false;
    }
    
    // Copy segments to runtime storage
    memcpy(m_zoneConfig, segments, count * sizeof(ZoneSegment));
    m_zoneCount = count;
    
    // Clear zone buffers when layout changes to prevent residue
    memset(m_zoneBuffers, 0, sizeof(m_zoneBuffers));
    
    Serial.printf("[ZoneComposer] Layout set to %d zones\n", m_zoneCount);
    return true;
}

bool ZoneComposer::validateLayout(const ZoneSegment* segments, uint8_t count) const {
    if (!segments || count == 0 || count > MAX_ZONES) {
        return false;
    }
    
    // Coverage map: track which LEDs are assigned (per strip, 0-159)
    bool coverage[STRIP_LENGTH] = {false};
    
    for (uint8_t i = 0; i < count; i++) {
        const ZoneSegment& seg = segments[i];
        
        // 1. Boundary Range Check
        if (seg.s1LeftStart > seg.s1LeftEnd || seg.s1LeftEnd >= 80) {
            Serial.printf("[ZoneComposer] Validation failed: Zone %d left segment out of range (%d-%d)\n",
                         i, seg.s1LeftStart, seg.s1LeftEnd);
            return false;
        }
        if (seg.s1RightStart < 80 || seg.s1RightStart > seg.s1RightEnd || seg.s1RightEnd >= STRIP_LENGTH) {
            Serial.printf("[ZoneComposer] Validation failed: Zone %d right segment out of range (%d-%d)\n",
                         i, seg.s1RightStart, seg.s1RightEnd);
            return false;
        }
        
        // 2. Minimum Zone Size Check
        uint8_t leftSize = seg.s1LeftEnd - seg.s1LeftStart + 1;
        uint8_t rightSize = seg.s1RightEnd - seg.s1RightStart + 1;
        if (leftSize == 0 || rightSize == 0) {
            Serial.printf("[ZoneComposer] Validation failed: Zone %d has zero-size segment\n", i);
            return false;
        }
        
        // 3. Symmetry Check
        if (leftSize != rightSize) {
            Serial.printf("[ZoneComposer] Validation failed: Zone %d segments not symmetric (left=%d, right=%d)\n",
                         i, leftSize, rightSize);
            return false;
        }
        
        // Check distance from centre
        uint8_t leftDistFromCentre = 79 - seg.s1LeftEnd;
        uint8_t rightDistFromCentre = seg.s1RightStart - 80;
        if (leftDistFromCentre != rightDistFromCentre) {
            Serial.printf("[ZoneComposer] Validation failed: Zone %d not symmetric around centre (left dist=%d, right dist=%d)\n",
                         i, leftDistFromCentre, rightDistFromCentre);
            return false;
        }
        
        // 4. Centre Pair Check (at least one zone must include 79 or 80)
        bool includesCentre = (seg.s1LeftEnd >= 79) || (seg.s1RightStart <= 80);
        if (i == 0 && !includesCentre) {
            Serial.printf("[ZoneComposer] Validation failed: Zone 0 (innermost) must include centre pair\n");
            return false;
        }
        
        // 5. Coverage Check - mark LEDs as used
        for (uint8_t led = seg.s1LeftStart; led <= seg.s1LeftEnd; led++) {
            if (coverage[led]) {
                Serial.printf("[ZoneComposer] Validation failed: Zone %d left segment overlaps at LED %d\n", i, led);
                return false;
            }
            coverage[led] = true;
        }
        for (uint8_t led = seg.s1RightStart; led <= seg.s1RightEnd; led++) {
            if (coverage[led]) {
                Serial.printf("[ZoneComposer] Validation failed: Zone %d right segment overlaps at LED %d\n", i, led);
                return false;
            }
            coverage[led] = true;
        }
    }
    
    // 6. Complete Coverage Check - verify all LEDs 0-159 are covered
    for (uint8_t led = 0; led < STRIP_LENGTH; led++) {
        if (!coverage[led]) {
            Serial.printf("[ZoneComposer] Validation failed: LED %d not covered by any zone\n", led);
            return false;
        }
    }
    
    // 7. Ordering Check - zones must be ordered centre-outward
    // For centre-origin symmetry: zone i (inner) has higher left segment numbers than zone i+1 (outer)
    // Left segments: zone i ends at a higher LED number than zone i+1 starts (zone i closer to centre)
    // Right segments: zone i starts at a lower LED number than zone i+1 starts (zone i closer to centre)
    for (uint8_t i = 0; i < count - 1; i++) {
        const ZoneSegment& seg1 = segments[i];      // Inner zone
        const ZoneSegment& seg2 = segments[i + 1];  // Outer zone
        
        // Left segments: inner zone should end at higher number than outer zone starts
        // (inner zone is closer to centre, so has higher LED indices on left side)
        if (seg1.s1LeftEnd <= seg2.s1LeftStart) {
            Serial.printf("[ZoneComposer] Validation failed: Zones not ordered centre-outward (left: zone %d ends at %d, zone %d starts at %d)\n",
                         i, seg1.s1LeftEnd, i+1, seg2.s1LeftStart);
            return false;
        }
        
        // Right segments: inner zone should start at lower number than outer zone starts
        // (inner zone is closer to centre, so has lower LED indices on right side)
        if (seg1.s1RightStart >= seg2.s1RightStart) {
            Serial.printf("[ZoneComposer] Validation failed: Zones not ordered centre-outward (right: zone %d starts at %d, zone %d starts at %d)\n",
                         i, seg1.s1RightStart, i+1, seg2.s1RightStart);
            return false;
        }
    }
    
    return true;
}

// ==================== Per-Zone Settings ====================

void ZoneComposer::setZoneEffect(uint8_t zone, uint8_t effectId) {
    // DEFENSIVE CHECK: Validate zone ID before array access
    uint8_t safeZone = validateZoneId(zone);
    if (safeZone >= MAX_ZONES || safeZone >= m_zoneCount) {
        return;  // Invalid zone, ignore request
    }
    m_zones[safeZone].effectId = effectId;
    // Clear this zone's buffer when switching effects to avoid ghosting from
    // previous effect state.
    memset(m_zoneBuffers[safeZone], 0, sizeof(m_zoneBuffers[safeZone]));
}

void ZoneComposer::setZoneBrightness(uint8_t zone, uint8_t brightness) {
    if (zone < MAX_ZONES) {
        // DEFENSIVE CHECK: Validate zone ID before array access
        uint8_t safeZone = validateZoneId(zone);
        if (safeZone >= MAX_ZONES || safeZone >= m_zoneCount) {
            return;  // Invalid zone, ignore request
        }
        m_zones[safeZone].brightness = brightness;
    }
}

void ZoneComposer::setZoneSpeed(uint8_t zone, uint8_t speed) {
    // DEFENSIVE CHECK: Validate zone ID before array access
    uint8_t safeZone = validateZoneId(zone);
    if (safeZone >= MAX_ZONES || safeZone >= m_zoneCount) {
        return;  // Invalid zone, ignore request
    }
    m_zones[safeZone].speed = constrain(speed, 1, 100);
}

void ZoneComposer::setZonePalette(uint8_t zone, uint8_t paletteId) {
    // DEFENSIVE CHECK: Validate zone ID before array access
    uint8_t safeZone = validateZoneId(zone);
    if (safeZone >= MAX_ZONES || safeZone >= m_zoneCount) {
        return;  // Invalid zone, ignore request
    }
    // DEFENSIVE CHECK: Validate palette ID before storing
    uint8_t safePaletteId = lightwaveos::palettes::validatePaletteId(paletteId);
    m_zones[safeZone].paletteId = safePaletteId;
}

void ZoneComposer::setZoneBlendMode(uint8_t zone, BlendMode mode) {
    // DEFENSIVE CHECK: Validate zone ID before array access
    uint8_t safeZone = validateZoneId(zone);
    if (safeZone >= MAX_ZONES || safeZone >= m_zoneCount) {
        return;  // Invalid zone, ignore request
    }
    m_zones[safeZone].blendMode = mode;
}

void ZoneComposer::setZoneEnabled(uint8_t zone, bool enabled) {
    // DEFENSIVE CHECK: Validate zone ID before array access
    uint8_t safeZone = validateZoneId(zone);
    if (safeZone >= MAX_ZONES || safeZone >= m_zoneCount) {
        return;  // Invalid zone, ignore request
    }
    m_zones[safeZone].enabled = enabled;
    if (enabled) {
        // Clear on enable so newly-enabled zones don't flash stale pixels.
        memset(m_zoneBuffers[safeZone], 0, sizeof(m_zoneBuffers[safeZone]));
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

// ==================== Zone Audio Config ====================

ZoneAudioConfig ZoneComposer::getZoneAudioConfig(uint8_t zone) const {
    if (zone >= MAX_ZONES) {
        return ZoneAudioConfig();  // Return default config for invalid zone
    }
    return m_zoneAudioConfigs[zone];
}

void ZoneComposer::setZoneAudioConfig(uint8_t zone, const ZoneAudioConfig& config) {
    // DEFENSIVE CHECK: Validate zone ID before array access
    uint8_t safeZone = validateZoneId(zone);
    if (safeZone >= MAX_ZONES || safeZone >= m_zoneCount) {
        return;  // Invalid zone, ignore request
    }
    m_zoneAudioConfigs[safeZone] = config;

    // Notify callback if registered
    if (m_stateChangeCallback) {
        m_stateChangeCallback(safeZone);
    }
}

// ==================== State Change Callback ====================

void ZoneComposer::setStateChangeCallback(std::function<void(uint8_t)> callback) {
    m_stateChangeCallback = callback;
}

// ==================== Presets ====================

void ZoneComposer::loadPreset(uint8_t presetId) {
    if (presetId >= NUM_PRESETS) {
        Serial.printf("[ZoneComposer] Invalid preset %d\n", presetId);
        return;
    }

    const ZonePreset& preset = PRESETS[presetId];

    // Set layout using segment array
    if (!setLayout(preset.segments, preset.zoneCount)) {
        Serial.printf("[ZoneComposer] ERROR: Failed to set layout for preset %d\n", presetId);
        return;
    }

    // Copy zone states
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

/**
 * @brief Validate and clamp zoneId to safe range [0, MAX_ZONES-1]
 * 
 * DEFENSIVE CHECK: Prevents LoadProhibited crashes from corrupted zone ID.
 * 
 * ZoneComposer uses arrays m_zones[MAX_ZONES] and m_zoneConfig[MAX_ZONES] where
 * MAX_ZONES = 4. If zoneId is corrupted (e.g., by memory corruption, invalid
 * input, or uninitialized state), accessing these arrays would cause out-of-bounds
 * access and crash.
 * 
 * This validation ensures we always access valid array indices, returning safe
 * default (zone 0) if corruption is detected.
 * 
 * @param zoneId Zone ID to validate
 * @return Valid zone ID in [0, MAX_ZONES-1], defaults to 0 if out of bounds
 */
uint8_t ZoneComposer::validateZoneId(uint8_t zoneId) const {
#if FEATURE_VALIDATION_PROFILING
#ifndef NATIVE_BUILD
    int64_t start = esp_timer_get_time();
#else
    int64_t start = 0;
#endif
#endif
    // Validate zoneId is within bounds [0, MAX_ZONES-1]
    if (zoneId >= MAX_ZONES) {
#if FEATURE_VALIDATION_PROFILING
#ifndef NATIVE_BUILD
        lightwaveos::core::system::ValidationProfiler::recordCall("validateZoneId", 
                                                                     esp_timer_get_time() - start);
#else
        lightwaveos::core::system::ValidationProfiler::recordCall("validateZoneId", 0);
#endif
#endif
        return 0;  // Return safe default (zone 0)
    }
#if FEATURE_VALIDATION_PROFILING
#ifndef NATIVE_BUILD
    lightwaveos::core::system::ValidationProfiler::recordCall("validateZoneId", 
                                                               esp_timer_get_time() - start);
#else
    lightwaveos::core::system::ValidationProfiler::recordCall("validateZoneId", 0);
#endif
#endif
    return zoneId;
}

void ZoneComposer::printStatus() const {
    Serial.println("\n=== Zone Composer Status ===");
    Serial.printf("Enabled: %s\n", m_enabled ? "YES" : "NO");
    Serial.printf("Zones: %d\n", m_zoneCount);

    for (uint8_t z = 0; z < m_zoneCount; z++) {
        // Validate zone index before access (defensive check)
        if (z >= MAX_ZONES) break;
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
