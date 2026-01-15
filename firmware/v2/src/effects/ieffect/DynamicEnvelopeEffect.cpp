/**
 * @file DynamicEnvelopeEffect.cpp
 * @brief Dynamic envelope effect - brightness follows musical dynamics
 *
 * This effect introduces dynamicSaliency to the visual pipeline - the one audio
 * feature that was at 0% utilization in the codebase.
 *
 * MUSICAL INTELLIGENCE:
 * - dynamicSaliency peaks during crescendos, sforzandos, and sudden quiet passages
 * - It's NOT about absolute volume, but CHANGES in volume over time
 * - This creates visuals that respond to the emotional arc of music
 *
 * CENTER ORIGIN COMPLIANCE:
 * - Base animation radiates from center (LEDs 79/80)
 * - Strip 2 receives +90 hue offset for color differentiation
 */

#include "DynamicEnvelopeEffect.h"
#include "../CoreEffects.h"
#include "../enhancement/SmoothingEngine.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

bool DynamicEnvelopeEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Initialize brightness envelope
    m_targetBrightness = 0.7f;
    m_currentBrightness = 0.7f;

    // Initialize dynamic tracking
    m_smoothedDynamic = 0.0f;
    for (uint8_t i = 0; i < RMS_HISTORY_SIZE; i++) {
        m_rmsHistory[i] = 0.0f;
    }
    m_histIdx = 0;

    // Initialize smoothing followers
    m_dynamicFollower.reset(0.0f);
    m_brightnessFollower.reset(0.7f);

    // Initialize hop tracking
    m_lastHopSeq = 0;
    m_targetDynamic = 0.0f;
    m_targetRms = 0.0f;

    // Initialize animation phase
    m_phase = 0.0f;

    return true;
}

void DynamicEnvelopeEffect::render(plugins::EffectContext& ctx) {
    float dt = ctx.getSafeDeltaSeconds();
    float moodNorm = ctx.getMoodNormalized();

    // ========================================================================
    // PHASE 1: Audio Analysis - Track Dynamic Saliency and RMS Trend
    // ========================================================================

    if (ctx.audio.available) {
        // Hop-based updates: update targets only on new audio hops
        bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;

            // Get the dynamicSaliency - our star feature!
            // This peaks during crescendos, sforzandos, sudden quiet passages
            m_targetDynamic = ctx.audio.dynamicSaliency();

            // Track RMS for trend detection (is volume rising or falling?)
            m_targetRms = ctx.audio.rms();

            // Update RMS history ring buffer
            m_rmsHistory[m_histIdx] = m_targetRms;
            m_histIdx = (m_histIdx + 1) % RMS_HISTORY_SIZE;
        }

        // Smooth dynamic saliency with MOOD-adjusted smoothing
        m_smoothedDynamic = m_dynamicFollower.updateWithMood(m_targetDynamic, dt, moodNorm);

        // Calculate RMS trend: compare old average (first half) vs new average (second half)
        // This tells us if volume is rising (crescendo) or falling (diminuendo)
        float oldAvg = 0.0f;
        float newAvg = 0.0f;
        for (uint8_t i = 0; i < RMS_HISTORY_SIZE / 2; i++) {
            oldAvg += m_rmsHistory[i];
            newAvg += m_rmsHistory[i + RMS_HISTORY_SIZE / 2];
        }
        oldAvg /= (RMS_HISTORY_SIZE / 2);
        newAvg /= (RMS_HISTORY_SIZE / 2);
        float trend = newAvg - oldAvg;  // Positive = crescendo, negative = diminuendo

        // ====================================================================
        // PHASE 2: Brightness Decision Logic
        // ====================================================================

        // Dynamic saliency threshold: only react when dynamics are "interesting"
        constexpr float SALIENCY_THRESHOLD = 0.25f;

        if (m_smoothedDynamic > SALIENCY_THRESHOLD) {
            // Musical dynamics are salient - follow the trend!

            // Scale change rate by saliency (more salient = faster changes)
            // Maximum 60% brightness change per second at full saliency
            float changeRate = m_smoothedDynamic * 0.6f;

            // Trend threshold to avoid noise
            constexpr float TREND_THRESHOLD = 0.008f;

            if (trend > TREND_THRESHOLD) {
                // CRESCENDO: Volume is rising -> brighten!
                m_targetBrightness = fminf(1.0f, m_currentBrightness + changeRate * dt);
            } else if (trend < -TREND_THRESHOLD) {
                // DIMINUENDO: Volume is falling -> dim
                // Minimum brightness of 0.2 ensures effect remains visible
                m_targetBrightness = fmaxf(0.2f, m_currentBrightness - changeRate * dt);
            }
            // No change if trend is near zero (steady state)
        } else {
            // Low saliency: dynamics are steady/uninteresting
            // Gradually return to moderate brightness
            m_targetBrightness = 0.6f;
        }

        // Smooth brightness transition with asymmetric follower
        m_currentBrightness = m_brightnessFollower.updateWithMood(m_targetBrightness, dt, moodNorm);

    } else {
        // ====================================================================
        // Fallback: No audio available - time-based gentle breathing
        // ====================================================================

        float phase = (float)(ctx.totalTimeMs % 5000) / 5000.0f;
        m_currentBrightness = 0.5f + 0.25f * sinf(phase * 6.283185f);
    }

    // ========================================================================
    // PHASE 3: Visual Rendering - CENTER ORIGIN Concentric Waves
    // ========================================================================

    // Clear buffer
    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));

    // Update animation phase based on speed parameter
    // Speed 1-50 maps to slower/faster animation
    float speedFactor = (float)ctx.speed / 50.0f;
    m_phase += speedFactor * 0.08f;
    if (m_phase > 6.283185f) m_phase -= 6.283185f;

    // Render both strips with CENTER ORIGIN pattern
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        // Calculate distance from center (0 at center pair, 79 at edges)
        uint16_t dist = centerPairDistance(i);

        // Create outward-propagating wave from center
        float normalizedDist = (float)dist / (float)HALF_LENGTH;  // 0.0 at center, 1.0 at edge

        // Wave calculation: sinusoidal with distance-based phase offset
        // This creates the "breathing" visual effect
        float wave = sinf(normalizedDist * 3.0f - m_phase);

        // Map wave from [-1, 1] to [0, 1]
        float rawBrightness = (wave + 1.0f) * 0.5f;

        // Apply the dynamic envelope - THIS IS THE KEY INNOVATION
        // When crescendo happens, everything gets brighter
        // When diminuendo happens, everything dims
        float finalBrightness = rawBrightness * m_currentBrightness;

        // Apply master brightness and convert to uint8
        uint8_t brightness = (uint8_t)(finalBrightness * ctx.brightness);

        // Color: palette-based with distance variation for visual interest
        // Slower hue shift based on distance creates depth perception
        uint8_t hue = ctx.gHue + (dist >> 2);  // Gentle hue gradient from center

        // ================================================================
        // Strip 1: Base rendering (indices 0-159)
        // ================================================================
        CRGB color1 = ctx.palette.getColor(hue, brightness);
        SET_STRIP1(ctx, i, color1);

        // ================================================================
        // Strip 2: +90 hue offset (indices 160-319)
        // ================================================================
        CRGB color2 = ctx.palette.getColor(hue + 90, brightness);
        SET_STRIP2_SAFE(ctx, i, color2);
    }
}

void DynamicEnvelopeEffect::cleanup() {
    // No dynamic allocations to free
}

const plugins::EffectMetadata& DynamicEnvelopeEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Dynamic Envelope",
        "Brightness follows musical crescendo/diminuendo",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
