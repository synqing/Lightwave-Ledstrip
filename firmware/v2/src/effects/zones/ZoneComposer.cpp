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
#include "../../palettes/Palettes_Master.h"
#include "../../config/features.h"
#if FEATURE_VALIDATION_PROFILING
#include "../../core/system/ValidationProfiler.h"
#endif

#ifndef LW_AGENT_TRACE
#define LW_AGENT_TRACE 0
#endif

#if LW_AGENT_TRACE
#define LW_AGENT_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define LW_AGENT_PRINTF(...) ((void)0)
#endif

// Phase 2b.1: Audio-reactive zone modulation
// Phase 2b.3: Zone audio band routing
#if FEATURE_AUDIO_SYNC
#include "AudioBandFilter.h"
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
    , m_zoneCount(3)                    // Default to 3 zones (TRIPLE)
    , m_renderer(nullptr)
{
    // Initialize zones to defaults
    for (uint8_t i = 0; i < MAX_ZONES; i++) {
        m_zones[i] = {
            .effectId = 0,
            .brightness = 255,
            .speed = 1,
            .paletteId = 0,
            .blendMode = BlendMode::OVERWRITE,
            .enabled = (i == 0),  // Only zone 0 enabled by default
            .audio = ZoneAudioConfig()  // Default audio config (Phase 2b.1)
        };
    }

    // Initialize default layout (TRIPLE) by copying from compile-time config
    memcpy(m_zoneConfig, ZONE_3_CONFIG, sizeof(ZONE_3_CONFIG));

    // Clear buffers
    memset(m_zoneBuffers, 0, sizeof(m_zoneBuffers));
    memset(m_outputBuffer, 0, sizeof(m_outputBuffer));
    m_totalTimeMs = 0;

    // Phase 2c.3: Initialize cached segment bounds
    memset(m_cachedBounds, 0, sizeof(m_cachedBounds));
    rebuildSegmentCache();
}

// ==================== Initialization ====================

bool ZoneComposer::init(RendererNode* renderer) {
    if (!renderer) {
        Serial.println("[ZoneComposer] ERROR: Null renderer");
        return false;
    }

    m_renderer = renderer;
    m_initialized = true;

    Serial.println("[ZoneComposer] Initialized");
    return true;
}

// ==================== Zone Control ====================

void ZoneComposer::setEnabled(bool enabled) {
    // #region agent log
    LW_AGENT_PRINTF("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A\",\"location\":\"ZoneComposer.cpp:setEnabled\",\"message\":\"setEnabled called\",\"data\":{\"enabled\":%d,\"m_initialized\":%d,\"m_renderer_null\":%d,\"m_zoneCount\":%d},\"timestamp\":%lu}\n",
        enabled, m_initialized, (m_renderer == nullptr), m_zoneCount, millis());
    // #endregion
    m_enabled = enabled;
}

// ==================== Rendering ====================

void ZoneComposer::render(CRGB* leds, uint16_t numLeds, CRGBPalette16* palette,
                          uint8_t hue, uint32_t frameCount, uint32_t deltaTimeMs,
                          const plugins::AudioContext* audioCtx) {
    // #region agent log
    LW_AGENT_PRINTF("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A\",\"location\":\"ZoneComposer.cpp:render\",\"message\":\"render entry\",\"data\":{\"m_initialized\":%d,\"m_enabled\":%d,\"m_renderer_null\":%d,\"m_zoneCount\":%d},\"timestamp\":%lu}\n",
        m_initialized, m_enabled, (m_renderer == nullptr), m_zoneCount, millis());
    // #endregion
    if (!m_initialized || !m_enabled) {
        return;
    }

    // Start total timing
    uint32_t frameStartUs = micros();

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
    // Use real deltaTimeMs for frame-rate independent smooth animation.
    // Clamp to reasonable bounds to prevent physics explosions on frame drops.
    uint32_t safeDeltaMs = deltaTimeMs;
    if (safeDeltaMs < 1) safeDeltaMs = 1;      // Minimum 1ms (1000 FPS cap)
    if (safeDeltaMs > 50) safeDeltaMs = 50;    // Maximum 50ms (20 FPS floor)
    m_zoneContext.deltaTimeMs = safeDeltaMs;
    // Monotonic time accumulator (stable even when safeDeltaMs varies)
    m_totalTimeMs += safeDeltaMs;
    m_zoneContext.totalTimeMs = m_totalTimeMs;

    // Copy audio context once per frame if available (reused for all zones)
    if (audioCtx != nullptr) {
        m_zoneContext.audio = *audioCtx;
    } else {
        // Reset audio context if not available
        m_zoneContext.audio = plugins::AudioContext();
    }

    // Phase 2b.2: Process beat triggers for all zones
    // Beat tracking removed - no beat triggers
#if FEATURE_AUDIO_SYNC
    m_lastBeatTick = false;
#endif

    // Always clear output buffer to prevent stale pixels from previous frames
    // (OVERWRITE blend mode only overwrites zone segments, not the full 320 LEDs)
    memset(m_outputBuffer, 0, sizeof(m_outputBuffer));

    // Render each enabled zone with timing
    uint32_t blendStartUs = 0;
    // #region agent log
    LW_AGENT_PRINTF("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"B\",\"location\":\"ZoneComposer.cpp:render\",\"message\":\"before zone loop\",\"data\":{\"m_zoneCount\":%d,\"MAX_ZONES\":%d},\"timestamp\":%lu}\n",
        m_zoneCount, MAX_ZONES, millis());
    // #endregion
    for (uint8_t z = 0; z < m_zoneCount; z++) {
        if (m_zones[z].enabled) {
            // #region agent log
            LW_AGENT_PRINTF("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"C\",\"location\":\"ZoneComposer.cpp:render\",\"message\":\"rendering zone\",\"data\":{\"zoneId\":%d,\"effectId\":%d},\"timestamp\":%lu}\n",
                z, m_zones[z].effectId, millis());
            // #endregion
            uint32_t zoneStartUs = micros();
            renderZone(z, leds, numLeds, hue, frameCount);
            uint32_t zoneEndUs = micros();
            // Store per-zone render time (handles micros() overflow)
            m_timing.zoneRenderUs[z] = (zoneEndUs >= zoneStartUs)
                ? (zoneEndUs - zoneStartUs)
                : (0xFFFFFFFF - zoneStartUs + zoneEndUs + 1);
        } else {
            m_timing.zoneRenderUs[z] = 0;
        }
    }
    
    // CRITICAL: Log immediately after loop - if this doesn't appear, crash is in loop or stack overflow
    LW_AGENT_PRINTF("ZONE_LOOP_DONE\n");

    // Time the blend/composite step (memcpy to output)
    LW_AGENT_PRINTF("BEFORE_MEMCPY\n");
    blendStartUs = micros();
    // Copy composited output to main buffer
    memcpy(leds, m_outputBuffer, numLeds * sizeof(CRGB));
    LW_AGENT_PRINTF("AFTER_MEMCPY\n");

    uint32_t blendEndUs = micros();
    m_timing.zoneBlendUs = (blendEndUs >= blendStartUs)
        ? (blendEndUs - blendStartUs)
        : (0xFFFFFFFF - blendStartUs + blendEndUs + 1);

    // Calculate total zone system overhead
    uint32_t frameEndUs = micros();
    m_timing.zoneTotalUs = (frameEndUs >= frameStartUs)
        ? (frameEndUs - frameStartUs)
        : (0xFFFFFFFF - frameStartUs + frameEndUs + 1);

    // Update timing metadata
    m_timing.lastUpdateMs = millis();
    m_timing.frameCount++;
    m_timing.cumulativeTotalUs += m_timing.zoneTotalUs;

    // Frame skip detection: if total time exceeds 2000us threshold
    constexpr uint32_t FRAME_SKIP_THRESHOLD_US = 2000;
    if (m_timing.zoneTotalUs > FRAME_SKIP_THRESHOLD_US) {
        m_timing.frameSkipCount++;
    }
    
    // #region agent log
    LW_AGENT_PRINTF("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"F\",\"location\":\"ZoneComposer.cpp:render\",\"message\":\"render complete\",\"data\":{},\"timestamp\":%lu}\n",
        millis());
    // #endregion
}

void ZoneComposer::renderZone(uint8_t zoneId, CRGB* leds, uint16_t numLeds,
                               uint8_t hue, uint32_t frameCount) {
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
    // Phase 2c.3: Use m_cachedBounds[safeZone] instead of m_zoneConfig for iteration
    // (m_zoneConfig still needed for zoneStart/zoneLength context values)
    const ZoneSegment& seg = m_zoneConfig[safeZone];

    // Get the IEffect instance
    // #region agent log
    LW_AGENT_PRINTF("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A\",\"location\":\"ZoneComposer.cpp:renderZone\",\"message\":\"before getEffectInstance\",\"data\":{\"zoneId\":%d,\"effectId\":%d,\"m_renderer_null\":%d},\"timestamp\":%lu}\n",
        zoneId, zone.effectId, (m_renderer == nullptr), millis());
    // #endregion
    plugins::IEffect* effect = m_renderer->getEffectInstance(zone.effectId);
    // #region agent log
    LW_AGENT_PRINTF("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A\",\"location\":\"ZoneComposer.cpp:renderZone\",\"message\":\"after getEffectInstance\",\"data\":{\"zoneId\":%d,\"effectId\":%d,\"effect_null\":%d},\"timestamp\":%lu}\n",
        zoneId, zone.effectId, (effect == nullptr), millis());
    // #endregion
    if (!effect) {
        return;
    }

    // Per-zone persistent render buffer (preserves trails/smoothing and prevents
    // cross-zone contamination).
    CRGB* zoneBuffer = m_zoneBuffers[safeZone];

    // ==================== Phase 2b.1: Audio Tempo Modulation ====================
    // Apply tempo/beat modulation if enabled for this zone
    uint8_t effectiveBrightness = zone.brightness;
    uint8_t effectiveSpeed = zone.speed;

#if FEATURE_AUDIO_SYNC
    if (zone.audio.tempoSync) {
        // Beat tracking removed - no tempo sync or beat modulation
    }
#endif

    // Update zone-specific fields in the reusable context
    // (Base fields were already set in render() once per frame)
    m_zoneContext.leds = zoneBuffer;
    m_zoneContext.brightness = effectiveBrightness;  // Use modulated brightness
    m_zoneContext.speed = effectiveSpeed;             // Use modulated speed
    m_zoneContext.zoneId = zoneId;  // Zone ID for zone-aware effects
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

    // ==================== Phase 2b.3: Zone Audio Band Filtering ====================
    // Apply frequency band filtering to audio context before rendering
    // This allows zones to respond to specific frequency ranges (bass/mid/high)
#if FEATURE_AUDIO_SYNC
    if (zone.audio.audioBand != AudioBands::BAND_FULL) {
        // Apply band filtering in-place to the zone context's audio data
        // This zeros out non-target bands so effects only "see" their assigned frequencies
        AudioBandFilter::applyInPlace(m_zoneContext.audio, zone.audio.audioBand);
    }
#endif

    // Render effect into this zone's persistent buffer
    // #region agent log
    LW_AGENT_PRINTF("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"E\",\"location\":\"ZoneComposer.cpp:renderZone\",\"message\":\"before effect->render\",\"data\":{\"zoneId\":%d,\"effectId\":%d,\"zoneBuffer_null\":%d},\"timestamp\":%lu}\n",
        zoneId, zone.effectId, (zoneBuffer == nullptr), millis());
    // #endregion
    effect->render(m_zoneContext);
    // #region agent log
    LW_AGENT_PRINTF("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"E\",\"location\":\"ZoneComposer.cpp:renderZone\",\"message\":\"after effect->render\",\"data\":{\"zoneId\":%d},\"timestamp\":%lu}\n",
        zoneId, millis());
    // #endregion

    // ==================== Phase 2c.3: Optimized Segment Compositing ====================
    // Uses cached segment bounds and pre-fetched blend function for ~40 us savings

    // Get cached bounds for this zone (avoids repeated struct member access)
    // #region agent log
    LW_AGENT_PRINTF("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"C\",\"location\":\"ZoneComposer.cpp:renderZone\",\"message\":\"before cache access\",\"data\":{\"safeZone\":%d,\"m_zoneCount\":%d},\"timestamp\":%lu}\n",
        safeZone, m_zoneCount, millis());
    // #endregion
    const CachedSegmentBounds& cache = m_cachedBounds[safeZone];
    const BlendFunc blendFn = cache.blendFunc;
    // #region agent log
    LW_AGENT_PRINTF("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"C\",\"location\":\"ZoneComposer.cpp:renderZone\",\"message\":\"after cache access\",\"data\":{\"safeZone\":%d,\"s1LeftStart\":%d,\"s1LeftEnd\":%d,\"blendFn_null\":%d},\"timestamp\":%lu}\n",
        safeZone, cache.s1LeftStart, cache.s1LeftEnd, (blendFn == nullptr), millis());
    // #endregion

    // Pre-clamp bounds to numLeds (rarely changes, but defensive)
    const uint16_t maxS1Idx = (numLeds < STRIP_LENGTH) ? numLeds : STRIP_LENGTH;

    // #region agent log
    LW_AGENT_PRINTF("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"D\",\"location\":\"ZoneComposer.cpp:renderZone\",\"message\":\"before blend loops\",\"data\":{\"safeZone\":%d,\"maxS1Idx\":%d,\"s1LeftStart\":%d,\"s1LeftEnd\":%d,\"s1RightStart\":%d,\"s1RightEnd\":%d,\"zoneBuffer_null\":%d,\"outputBuffer_null\":%d},\"timestamp\":%lu}\n",
        safeZone, maxS1Idx, cache.s1LeftStart, cache.s1LeftEnd, cache.s1RightStart, cache.s1RightEnd, (zoneBuffer == nullptr), (m_outputBuffer == nullptr), millis());
    // #endregion

    // Process Strip 1 left segment using cached bounds
    // Use effectiveBrightness for audio-reactive modulation (Phase 2b.1)
    // #region agent log
    LW_AGENT_PRINTF("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"D\",\"location\":\"ZoneComposer.cpp:renderZone\",\"message\":\"before s1Left loop\",\"data\":{\"safeZone\":%d},\"timestamp\":%lu}\n",
        safeZone, millis());
    // #endregion
    for (uint8_t i = cache.s1LeftStart; i <= cache.s1LeftEnd && i < maxS1Idx; i++) {
        CRGB pixel = zoneBuffer[i];
        pixel.nscale8(effectiveBrightness);
        m_outputBuffer[i] = blendFn(m_outputBuffer[i], pixel);
    }
    // #region agent log
    LW_AGENT_PRINTF("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"D\",\"location\":\"ZoneComposer.cpp:renderZone\",\"message\":\"after s1Left loop\",\"data\":{\"safeZone\":%d},\"timestamp\":%lu}\n",
        safeZone, millis());
    // #endregion

    // Process Strip 1 right segment using cached bounds
    // #region agent log
    LW_AGENT_PRINTF("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"D\",\"location\":\"ZoneComposer.cpp:renderZone\",\"message\":\"before s1Right loop\",\"data\":{\"safeZone\":%d},\"timestamp\":%lu}\n",
        safeZone, millis());
    // #endregion
    for (uint8_t i = cache.s1RightStart; i <= cache.s1RightEnd && i < maxS1Idx; i++) {
        CRGB pixel = zoneBuffer[i];
        pixel.nscale8(effectiveBrightness);
        m_outputBuffer[i] = blendFn(m_outputBuffer[i], pixel);
    }
    // #region agent log
    LW_AGENT_PRINTF("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"D\",\"location\":\"ZoneComposer.cpp:renderZone\",\"message\":\"after s1Right loop\",\"data\":{\"safeZone\":%d},\"timestamp\":%lu}\n",
        safeZone, millis());
    // #endregion

    // Process Strip 2 (indices 160-319) using pre-computed offsets
    // Check numLeds >= STRIP_LENGTH once to avoid per-iteration check
    // #region agent log
    LW_AGENT_PRINTF("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"D\",\"location\":\"ZoneComposer.cpp:renderZone\",\"message\":\"before Strip2 check\",\"data\":{\"safeZone\":%d,\"numLeds\":%d,\"STRIP_LENGTH\":%d},\"timestamp\":%lu}\n",
        safeZone, numLeds, STRIP_LENGTH, millis());
    // #endregion
    if (numLeds > STRIP_LENGTH) {
        // #region agent log
        LW_AGENT_PRINTF("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"D\",\"location\":\"ZoneComposer.cpp:renderZone\",\"message\":\"before Strip2 left loop\",\"data\":{\"safeZone\":%d,\"s1LeftStart\":%d,\"s1LeftEnd\":%d},\"timestamp\":%lu}\n",
            safeZone, cache.s1LeftStart, cache.s1LeftEnd, millis());
        // #endregion
        // Strip 2 left segment
        for (uint8_t i = cache.s1LeftStart; i <= cache.s1LeftEnd; i++) {
            uint16_t s2Idx = static_cast<uint16_t>(i) + STRIP_LENGTH;
            if (s2Idx < numLeds) {
                CRGB pixel = zoneBuffer[s2Idx];
                pixel.nscale8(effectiveBrightness);
                m_outputBuffer[s2Idx] = blendFn(m_outputBuffer[s2Idx], pixel);
            }
        }
        // #region agent log
        LW_AGENT_PRINTF("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"D\",\"location\":\"ZoneComposer.cpp:renderZone\",\"message\":\"after Strip2 left loop\",\"data\":{\"safeZone\":%d},\"timestamp\":%lu}\n",
            safeZone, millis());
        // #endregion

        // Strip 2 right segment
        // #region agent log
        LW_AGENT_PRINTF("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"D\",\"location\":\"ZoneComposer.cpp:renderZone\",\"message\":\"before Strip2 right loop\",\"data\":{\"safeZone\":%d,\"s1RightStart\":%d,\"s1RightEnd\":%d},\"timestamp\":%lu}\n",
            safeZone, cache.s1RightStart, cache.s1RightEnd, millis());
        // #endregion
        for (uint8_t i = cache.s1RightStart; i <= cache.s1RightEnd; i++) {
            uint16_t s2Idx = static_cast<uint16_t>(i) + STRIP_LENGTH;
            if (s2Idx < numLeds) {
                CRGB pixel = zoneBuffer[s2Idx];
                pixel.nscale8(effectiveBrightness);
                m_outputBuffer[s2Idx] = blendFn(m_outputBuffer[s2Idx], pixel);
            }
        }
        // #region agent log
        LW_AGENT_PRINTF("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"D\",\"location\":\"ZoneComposer.cpp:renderZone\",\"message\":\"after Strip2 right loop\",\"data\":{\"safeZone\":%d},\"timestamp\":%lu}\n",
            safeZone, millis());
        // #endregion
    }
    // #region agent log
    LW_AGENT_PRINTF("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"D\",\"location\":\"ZoneComposer.cpp:renderZone\",\"message\":\"renderZone complete\",\"data\":{\"safeZone\":%d},\"timestamp\":%lu}\n",
        safeZone, millis());
    // #endregion
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

    // Phase 2c.3: Rebuild cached segment bounds
    rebuildSegmentCache();

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

// ==================== Zone Reordering (Phase 2c.1) ====================

bool ZoneComposer::reorderZones(const uint8_t* newOrder, uint8_t count) {
    // 1. Validate count matches current zone count
    if (count != m_zoneCount) {
        Serial.printf("[ZoneComposer] reorderZones failed: count mismatch (expected %d, got %d)\n",
                     m_zoneCount, count);
        return false;
    }

    if (count == 0 || count > MAX_ZONES) {
        Serial.printf("[ZoneComposer] reorderZones failed: invalid count %d\n", count);
        return false;
    }

    if (newOrder == nullptr) {
        Serial.println("[ZoneComposer] reorderZones failed: null newOrder array");
        return false;
    }

    // 2. Validate all zone IDs are valid (0 to count-1)
    for (uint8_t i = 0; i < count; i++) {
        if (newOrder[i] >= count) {
            Serial.printf("[ZoneComposer] reorderZones failed: invalid zone ID %d at index %d\n",
                         newOrder[i], i);
            return false;
        }
    }

    // 3. Check for duplicates
    bool seen[MAX_ZONES] = {false};
    for (uint8_t i = 0; i < count; i++) {
        if (seen[newOrder[i]]) {
            Serial.printf("[ZoneComposer] reorderZones failed: duplicate zone ID %d\n", newOrder[i]);
            return false;
        }
        seen[newOrder[i]] = true;
    }

    // 4. CENTER ORIGIN constraint: After reorder, zone 0 must contain LEDs 79/80
    // The new zone 0 will be the segment currently at position newOrder[0]
    const ZoneSegment& newZone0Segment = m_zoneConfig[newOrder[0]];

    // Check if segment contains center LEDs (79 on left side, 80 on right side)
    // Zone 0 must have its left segment include LED 79 or right segment include LED 80
    bool containsCenter = (newZone0Segment.s1LeftEnd >= 79 && newZone0Segment.s1LeftStart <= 79) ||
                          (newZone0Segment.s1RightStart <= 80 && newZone0Segment.s1RightEnd >= 80);

    if (!containsCenter) {
        Serial.printf("[ZoneComposer] reorderZones failed: CENTER ORIGIN violated\n");
        Serial.printf("  New zone 0 would use segment with left=%d-%d, right=%d-%d\n",
                     newZone0Segment.s1LeftStart, newZone0Segment.s1LeftEnd,
                     newZone0Segment.s1RightStart, newZone0Segment.s1RightEnd);
        Serial.println("  Zone 0 MUST contain center LEDs 79/80");
        return false;
    }

    // 5. Perform the reorder - create temp copies
    ZoneSegment tempSegments[MAX_ZONES];
    ZoneState tempStates[MAX_ZONES];
    CRGB tempBuffers[MAX_ZONES][TOTAL_LEDS];

    // Copy current configs to temp in new order
    for (uint8_t i = 0; i < count; i++) {
        uint8_t srcIdx = newOrder[i];

        // Copy segment definition (update zoneId to new position)
        tempSegments[i] = m_zoneConfig[srcIdx];
        tempSegments[i].zoneId = i;

        // Copy zone state
        tempStates[i] = m_zones[srcIdx];

        // Copy zone buffer to preserve any in-progress animations
        memcpy(tempBuffers[i], m_zoneBuffers[srcIdx], sizeof(m_zoneBuffers[srcIdx]));
    }

    // Apply reordered configs
    for (uint8_t i = 0; i < count; i++) {
        m_zoneConfig[i] = tempSegments[i];
        m_zones[i] = tempStates[i];
        memcpy(m_zoneBuffers[i], tempBuffers[i], sizeof(m_zoneBuffers[i]));
    }

    // Rebuild segment cache after reorder (Phase 2c.3 compatibility)
    rebuildSegmentCache();

    Serial.printf("[ZoneComposer] Zones reordered: [");
    for (uint8_t i = 0; i < count; i++) {
        Serial.printf("%d%s", newOrder[i], (i < count - 1) ? ", " : "");
    }
    Serial.println("]");

    // Notify state change for all zones
    for (uint8_t i = 0; i < count; i++) {
        notifyStateChange(i);
    }

    return true;
}

// ==================== State Change Callback ====================

void ZoneComposer::setStateChangeCallback(ZoneStateCallback callback) {
    m_stateCallback = callback;
}

void ZoneComposer::notifyStateChange(uint8_t zoneId) {
    if (!m_stateCallback) return;

    // Validate zone ID
    if (zoneId >= MAX_ZONES) return;

    // Throttle: max 10 broadcasts per second per zone
    uint32_t now = millis();
    if (now - m_lastBroadcastMs[zoneId] < BROADCAST_THROTTLE_MS) {
        return;  // Too soon, skip this notification
    }
    m_lastBroadcastMs[zoneId] = now;

    // Invoke the callback
    m_stateCallback(zoneId);
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

    // Notify listeners of state change
    notifyStateChange(safeZone);
}

void ZoneComposer::setZoneBrightness(uint8_t zone, uint8_t brightness) {
    // DEFENSIVE CHECK: Validate zone ID before array access
    uint8_t safeZone = validateZoneId(zone);
    if (safeZone >= MAX_ZONES || safeZone >= m_zoneCount) {
        return;  // Invalid zone, ignore request
    }
    m_zones[safeZone].brightness = brightness;

    // Notify listeners of state change
    notifyStateChange(safeZone);
}

void ZoneComposer::setZoneSpeed(uint8_t zone, uint8_t speed) {
    // DEFENSIVE CHECK: Validate zone ID before array access
    uint8_t safeZone = validateZoneId(zone);
    if (safeZone >= MAX_ZONES || safeZone >= m_zoneCount) {
        return;  // Invalid zone, ignore request
    }
    m_zones[safeZone].speed = constrain(speed, 1, 100);

    // Notify listeners of state change
    notifyStateChange(safeZone);
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

    // Notify listeners of state change
    notifyStateChange(safeZone);
}

void ZoneComposer::setZoneBlendMode(uint8_t zone, BlendMode mode) {
    // DEFENSIVE CHECK: Validate zone ID before array access
    uint8_t safeZone = validateZoneId(zone);
    if (safeZone >= MAX_ZONES || safeZone >= m_zoneCount) {
        return;  // Invalid zone, ignore request
    }
    // DEFENSIVE CHECK: Validate blend mode before storing
    BlendMode safeMode = validateBlendMode(static_cast<uint8_t>(mode));
    m_zones[safeZone].blendMode = safeMode;

    // Phase 2c.3: Update cached blend function for this zone
    updateBlendFunctionCache(safeZone);

    // Notify listeners of state change
    notifyStateChange(safeZone);
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

    // Notify listeners of state change
    notifyStateChange(safeZone);
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

    // Set layout using segment array (this also calls rebuildSegmentCache)
    if (!setLayout(preset.segments, preset.zoneCount)) {
        Serial.printf("[ZoneComposer] ERROR: Failed to set layout for preset %d\n", presetId);
        return;
    }

    // Copy zone states
    for (uint8_t i = 0; i < MAX_ZONES; i++) {
        m_zones[i] = preset.zones[i];
    }

    // Phase 2c.3: Rebuild cache to pick up blend modes from preset
    rebuildSegmentCache();

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

// ==================== Timing Metrics ====================

void ZoneComposer::resetTimingMetrics() {
    for (uint8_t i = 0; i < MAX_ZONES; i++) {
        m_timing.zoneRenderUs[i] = 0;
    }
    m_timing.zoneBlendUs = 0;
    m_timing.zoneTotalUs = 0;
    m_timing.frameSkipCount = 0;
    m_timing.lastUpdateMs = millis();
    m_timing.frameCount = 0;
    m_timing.cumulativeTotalUs = 0;
}

// ==================== Zone Audio Config (Phase 2b.1) ====================

ZoneAudioConfig ZoneComposer::getZoneAudioConfig(uint8_t zone) const {
    if (zone >= MAX_ZONES) {
        return ZoneAudioConfig();  // Return default if invalid
    }
    return m_zones[zone].audio;
}

void ZoneComposer::setZoneAudioConfig(uint8_t zone, const ZoneAudioConfig& config) {
    uint8_t safeZone = validateZoneId(zone);
    if (safeZone >= MAX_ZONES || safeZone >= m_zoneCount) {
        return;
    }
    m_zones[safeZone].audio = config;
    notifyStateChange(safeZone);
}

void ZoneComposer::setZoneTempoSync(uint8_t zone, bool enabled) {
    uint8_t safeZone = validateZoneId(zone);
    if (safeZone >= MAX_ZONES || safeZone >= m_zoneCount) {
        return;
    }
    m_zones[safeZone].audio.tempoSync = enabled;
    notifyStateChange(safeZone);
}

void ZoneComposer::setZoneBeatModulation(uint8_t zone, uint8_t modulation) {
    uint8_t safeZone = validateZoneId(zone);
    if (safeZone >= MAX_ZONES || safeZone >= m_zoneCount) {
        return;
    }
    m_zones[safeZone].audio.beatModulation = modulation;
    notifyStateChange(safeZone);
}

void ZoneComposer::setZoneTempoSpeedScale(uint8_t zone, uint8_t scale) {
    uint8_t safeZone = validateZoneId(zone);
    if (safeZone >= MAX_ZONES || safeZone >= m_zoneCount) {
        return;
    }
    m_zones[safeZone].audio.tempoSpeedScale = scale;
    notifyStateChange(safeZone);
}

void ZoneComposer::setZoneBeatDecay(uint8_t zone, uint8_t decay) {
    uint8_t safeZone = validateZoneId(zone);
    if (safeZone >= MAX_ZONES || safeZone >= m_zoneCount) {
        return;
    }
    m_zones[safeZone].audio.beatDecay = decay;
    notifyStateChange(safeZone);
}

void ZoneComposer::setZoneAudioBand(uint8_t zone, uint8_t band) {
    uint8_t safeZone = validateZoneId(zone);
    if (safeZone >= MAX_ZONES || safeZone >= m_zoneCount) {
        return;
    }
    // Clamp to valid range (0=full, 1=bass, 2=mid, 3=high)
    m_zones[safeZone].audio.audioBand = band > 3 ? 0 : band;
    notifyStateChange(safeZone);
}

// ==================== Phase 2b.2: Beat Trigger Methods ====================

void ZoneComposer::setZoneBeatTriggerEnabled(uint8_t zone, bool enabled) {
    uint8_t safeZone = validateZoneId(zone);
    if (safeZone >= MAX_ZONES || safeZone >= m_zoneCount) {
        return;
    }
    m_zones[safeZone].audio.beatTriggerEnabled = enabled;
    // Reset beat counter when enabling/disabling
    m_beatCounter[safeZone] = 0;
    notifyStateChange(safeZone);
}

void ZoneComposer::setZoneBeatTriggerInterval(uint8_t zone, uint8_t interval) {
    uint8_t safeZone = validateZoneId(zone);
    if (safeZone >= MAX_ZONES || safeZone >= m_zoneCount) {
        return;
    }
    // Clamp interval to valid range (1-32 beats)
    m_zones[safeZone].audio.beatTriggerInterval = constrain(interval, 1, 32);
    notifyStateChange(safeZone);
}

void ZoneComposer::setZoneBeatTriggerEffectList(uint8_t zone, const uint8_t* effectIds, uint8_t count) {
    uint8_t safeZone = validateZoneId(zone);
    if (safeZone >= MAX_ZONES || safeZone >= m_zoneCount) {
        return;
    }
    if (!effectIds) {
        return;
    }

    // Clamp count to max 8 effects
    uint8_t safeCount = count > 8 ? 8 : count;
    m_zones[safeZone].audio.effectListSize = safeCount;

    // Copy effect IDs
    for (uint8_t i = 0; i < safeCount; i++) {
        m_zones[safeZone].audio.effectList[i] = effectIds[i];
    }

    // Reset current index if out of bounds
    if (m_zones[safeZone].audio.currentEffectIndex >= safeCount) {
        m_zones[safeZone].audio.currentEffectIndex = 0;
    }

    // If list has effects, set current effect to the first one
    if (safeCount > 0 && m_zones[safeZone].audio.beatTriggerEnabled) {
        setZoneEffect(safeZone, m_zones[safeZone].audio.effectList[0]);
    }

    notifyStateChange(safeZone);
}

void ZoneComposer::getZoneBeatTriggerConfig(uint8_t zone, bool& enabled, uint8_t& interval,
                                             uint8_t* effectIds, uint8_t& effectCount,
                                             uint8_t& currentIndex) const {
    if (zone >= MAX_ZONES) {
        enabled = false;
        interval = 4;
        effectCount = 0;
        currentIndex = 0;
        return;
    }

    const ZoneAudioConfig& audio = m_zones[zone].audio;
    enabled = audio.beatTriggerEnabled;
    interval = audio.beatTriggerInterval;
    effectCount = audio.effectListSize;
    currentIndex = audio.currentEffectIndex;

    // Copy effect list if buffer provided
    if (effectIds) {
        for (uint8_t i = 0; i < audio.effectListSize && i < 8; i++) {
            effectIds[i] = audio.effectList[i];
        }
    }
}

void ZoneComposer::processBeatTrigger(uint8_t zoneId, bool beatTick) {
    if (zoneId >= MAX_ZONES || zoneId >= m_zoneCount) {
        return;
    }

    ZoneState& zone = m_zones[zoneId];
    ZoneAudioConfig& audio = zone.audio;

    // Skip if beat trigger not enabled or no effects in list
    if (!audio.beatTriggerEnabled || audio.effectListSize == 0) {
        return;
    }

    // Only process on beat tick
    if (!beatTick) {
        return;
    }

    // Increment beat counter
    m_beatCounter[zoneId]++;

    // Check if we've reached the interval
    if (m_beatCounter[zoneId] >= audio.beatTriggerInterval) {
        // Reset counter
        m_beatCounter[zoneId] = 0;

        // Advance to next effect in list
        audio.currentEffectIndex++;
        if (audio.currentEffectIndex >= audio.effectListSize) {
            audio.currentEffectIndex = 0;  // Wrap around
        }

        // Set the new effect
        uint8_t newEffectId = audio.effectList[audio.currentEffectIndex];
        setZoneEffect(zoneId, newEffectId);

        // Note: setZoneEffect already calls notifyStateChange()
    }
}

// ==================== Phase 2c.3: Cache Management ====================

void ZoneComposer::rebuildSegmentCache() {
    for (uint8_t z = 0; z < MAX_ZONES; z++) {
        if (z < m_zoneCount) {
            const ZoneSegment& seg = m_zoneConfig[z];

            // Cache Strip 1 left segment bounds
            m_cachedBounds[z].s1LeftStart = seg.s1LeftStart;
            m_cachedBounds[z].s1LeftEnd = seg.s1LeftEnd;
            m_cachedBounds[z].s1LeftCount = seg.s1LeftEnd - seg.s1LeftStart + 1;

            // Cache Strip 1 right segment bounds
            m_cachedBounds[z].s1RightStart = seg.s1RightStart;
            m_cachedBounds[z].s1RightEnd = seg.s1RightEnd;
            m_cachedBounds[z].s1RightCount = seg.s1RightEnd - seg.s1RightStart + 1;

            // Pre-compute Strip 2 offsets (saves addition per pixel)
            m_cachedBounds[z].s2LeftStart = static_cast<uint16_t>(seg.s1LeftStart) + STRIP_LENGTH;
            m_cachedBounds[z].s2RightStart = static_cast<uint16_t>(seg.s1RightStart) + STRIP_LENGTH;

            // Pre-fetch blend function for this zone (O(1) dispatch)
            m_cachedBounds[z].blendFunc = getBlendFunction(m_zones[z].blendMode);
        } else {
            // Zero out unused zone cache entries
            memset(&m_cachedBounds[z], 0, sizeof(CachedSegmentBounds));
            m_cachedBounds[z].blendFunc = blendOverwrite;  // Safe default
        }
    }
}

void ZoneComposer::updateBlendFunctionCache(uint8_t zoneId) {
    if (zoneId >= MAX_ZONES) return;

    // Update only the blend function for this zone
    m_cachedBounds[zoneId].blendFunc = getBlendFunction(m_zones[zoneId].blendMode);
}

// ==================== Memory Metrics (Phase 2c.2) ====================

ZoneMemoryStats ZoneComposer::getMemoryStats() const {
    ZoneMemoryStats stats;

    // Calculate config storage: ZoneState struct * MAX_ZONES
    stats.configSize = sizeof(ZoneState) * MAX_ZONES;

    // Calculate buffer size: CRGB (3 bytes) * TOTAL_LEDS * MAX_ZONES
    // Note: m_zoneBuffers[MAX_ZONES][TOTAL_LEDS] + m_outputBuffer[TOTAL_LEDS]
    stats.bufferSize = sizeof(CRGB) * TOTAL_LEDS * MAX_ZONES +  // Per-zone buffers
                       sizeof(CRGB) * TOTAL_LEDS;                // Output buffer

    // ZoneComposer struct size (includes embedded arrays)
    stats.composerOverhead = sizeof(ZoneComposer);

    // Total zone system RAM footprint
    // Note: composerOverhead already includes configSize and bufferSize since they're members
    stats.totalZoneBytes = stats.composerOverhead;

    // Max NVS preset storage estimate
    // ZonePreset struct size * MAX_PRESETS (5)
    // ZonePreset: version(1) + name(32) + zonesEnabled(1) + zoneCount(1) +
    //             ZoneConfig(6)*4 + Segment(4)*4 + checksum(4) = ~83 bytes each
    // Conservative estimate: 256 bytes per preset (with NVS overhead)
    constexpr size_t ZONE_PRESET_SIZE_ESTIMATE = 256;
    constexpr size_t ZONE_PRESET_MAX_COUNT = 5;
    stats.presetStorageMax = ZONE_PRESET_SIZE_ESTIMATE * ZONE_PRESET_MAX_COUNT;

    // Count currently enabled zones
    stats.activeZones = 0;
    for (uint8_t i = 0; i < m_zoneCount && i < MAX_ZONES; i++) {
        if (m_zones[i].enabled) {
            stats.activeZones++;
        }
    }

    // Get ESP heap stats
#ifndef NATIVE_BUILD
    stats.heapFree = ESP.getFreeHeap();
    stats.heapLargestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
#else
    // Native build (testing) - provide dummy values
    stats.heapFree = 0;
    stats.heapLargestBlock = 0;
#endif

    return stats;
}

} // namespace zones
} // namespace lightwaveos
