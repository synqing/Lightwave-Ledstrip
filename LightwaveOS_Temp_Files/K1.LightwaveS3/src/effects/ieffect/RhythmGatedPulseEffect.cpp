/**
 * @file RhythmGatedPulseEffect.cpp
 * @brief Rhythm-gated pulse effect implementation
 *
 * Uses rhythmicSaliency to intelligently gate beat response:
 * - Pulses ONLY during rhythmically salient sections
 * - Fades to ambient pattern during melodic/quiet sections
 * - Smooth crossfade prevents jarring transitions
 */

#include "RhythmGatedPulseEffect.h"
#include "../CoreEffects.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

bool RhythmGatedPulseEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Reset pulse state
    m_pulseIntensity = 0.0f;
    m_lastBeat = false;
    m_lastBeatPhase = 0.0f;

    // Reset ambient state
    m_ambientPhase = 0.0f;

    // Reset saliency follower
    m_saliencyFollower.reset(0.0f);

    // Reset fallback state
    m_fallbackPhase = 0.0f;
    m_lastBeatTimeMs = 0;

    return true;
}

void RhythmGatedPulseEffect::render(plugins::EffectContext& ctx) {
    float dt = ctx.getSafeDeltaSeconds();
    uint32_t nowMs = ctx.totalTimeMs;

    // ========================================================================
    // Step 1: Get rhythmic saliency and smooth it
    // ========================================================================

    float targetSaliency = 0.0f;
    float beatPhase = 0.0f;
    bool beatTick = false;

    if (ctx.audio.available) {
        // Use the rhythmicSaliency feature (0% usage in codebase until now!)
        targetSaliency = ctx.audio.rhythmicSaliency();
        beatPhase = ctx.audio.beatPhase();

        // Robust beat detection: phase wrap (0.95 -> 0.05) OR isOnBeat flag near phase 0
        bool phaseWrap = (beatPhase < 0.15f && m_lastBeatPhase > 0.85f);
        bool beatFlag = ctx.audio.isOnBeat() && beatPhase < 0.20f;
        beatTick = phaseWrap || beatFlag;

        m_lastBeatPhase = beatPhase;
    } else {
        // Fallback: 120 BPM free-running
        m_fallbackPhase += dt * 2.0f;  // 2 Hz = 120 BPM
        if (m_fallbackPhase >= 1.0f) {
            m_fallbackPhase -= 1.0f;
            beatTick = true;
        }
        beatPhase = m_fallbackPhase;
        targetSaliency = 0.5f;  // Moderate saliency in fallback mode
    }

    // Smooth saliency to prevent mode-switching jitter
    float smoothedSaliency = m_saliencyFollower.update(targetSaliency, dt);

    // ========================================================================
    // Step 2: Gate logic - decide if rhythm is active
    // ========================================================================

    bool rhythmActive = smoothedSaliency > SALIENCY_THRESHOLD;

    // ========================================================================
    // Step 3: Update pulse state (only matters when rhythm is active)
    // ========================================================================

    // Debounce beats: minimum 200ms between pulses (300 BPM max)
    constexpr uint32_t MIN_BEAT_INTERVAL_MS = 200;
    uint32_t timeSinceLastBeat = nowMs - m_lastBeatTimeMs;

    if (rhythmActive && beatTick && !m_lastBeat && timeSinceLastBeat >= MIN_BEAT_INTERVAL_MS) {
        // Trigger pulse! Intensity scales with saliency for extra punch when rhythm is strong
        m_pulseIntensity = 0.6f + smoothedSaliency * 0.4f;
        m_lastBeatTimeMs = nowMs;
    }
    m_lastBeat = beatTick;

    // Decay pulse with fast attack, moderate release
    // Fast decay (8 Hz lambda) for punchy pulses when active
    // Slower decay (2 Hz lambda) when transitioning to ambient
    float decayRate = rhythmActive ? 8.0f : 2.0f;
    m_pulseIntensity *= expf(-decayRate * dt);
    if (m_pulseIntensity < 0.01f) {
        m_pulseIntensity = 0.0f;
    }

    // ========================================================================
    // Step 4: Update ambient animation (always runs, but fades when rhythm active)
    // ========================================================================

    // Ambient wave speed scales with ctx.speed parameter
    float ambientSpeed = (float)ctx.speed / 50.0f;
    m_ambientPhase += ambientSpeed * 0.03f;
    if (m_ambientPhase >= 6.28318f) {
        m_ambientPhase -= 6.28318f;
    }

    // Ambient strength is inverse of saliency - fades when rhythm takes over
    float ambientStrength = 1.0f - smoothedSaliency;

    // ========================================================================
    // Step 5: Render to LED buffer (CENTER ORIGIN)
    // ========================================================================

    // Clear buffer first
    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));

    for (uint16_t i = 0; i < STRIP_LENGTH; ++i) {
        uint16_t dist = centerPairDistance(i);
        float normalizedDist = (float)dist / (float)HALF_LENGTH;

        // --- Pulse: bright center, falls off with distance ---
        // Pulse radius is ~40 LEDs (half of strip from center)
        float pulseRadius = 40.0f;
        float pulseFalloff = 1.0f - (float)dist / pulseRadius;
        if (pulseFalloff < 0.0f) pulseFalloff = 0.0f;
        float pulseBrightness = m_pulseIntensity * pulseFalloff;

        // --- Ambient: gentle sine wave pattern ---
        // Creates concentric rings that slowly drift from center
        float ambientWave = (sinf((float)dist * 0.08f - m_ambientPhase) + 1.0f) * 0.15f;
        float ambientBrightness = ambientWave * ambientStrength;

        // Add subtle center glow in ambient mode
        float centerGlow = ambientStrength * 0.1f * (1.0f - normalizedDist);

        // --- Combine all layers ---
        float totalBrightness = pulseBrightness + ambientBrightness + centerGlow;
        if (totalBrightness > 1.0f) totalBrightness = 1.0f;

        uint8_t brightness = (uint8_t)(totalBrightness * (float)ctx.brightness);

        // Hue: base from gHue, shift by distance for depth
        uint8_t hue = ctx.gHue + (uint8_t)(dist >> 2);

        // Strip 1: base color
        CRGB color1 = ctx.palette.getColor(hue, brightness);

        // Strip 2: +90 hue offset for complementary color relationship
        uint8_t hue2 = hue + 90;
        CRGB color2 = ctx.palette.getColor(hue2, brightness);

        // Set LEDs using CENTER PAIR pattern for symmetric rendering
        // Strip 1: LEDs 0-159
        SET_STRIP1(ctx, CENTER_LEFT - dist, color1);
        if (dist > 0) {  // Avoid double-setting center
            SET_STRIP1(ctx, CENTER_RIGHT + dist, color1);
        }

        // Strip 2: LEDs 160-319 (with +90 hue offset)
        SET_STRIP2_SAFE(ctx, CENTER_LEFT - dist, color2);
        if (dist > 0) {
            SET_STRIP2_SAFE(ctx, CENTER_RIGHT + dist, color2);
        }
    }
}

void RhythmGatedPulseEffect::cleanup() {
    // No dynamic allocations to clean up
}

const plugins::EffectMetadata& RhythmGatedPulseEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Rhythm Gated Pulse",
        "Pulses only during rhythmically salient sections",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
