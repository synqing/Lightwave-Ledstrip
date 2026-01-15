/**
 * @file CascadedSpringEffect.cpp
 * @brief Ultra-smooth audio-reactive wave using cascaded spring physics
 *
 * Implementation of cascaded spring smoothing for audio reactivity.
 * See header for mathematical basis and design rationale.
 */

#include "CascadedSpringEffect.h"
#include "../CoreEffects.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

bool CascadedSpringEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Reset springs to zero with no velocity
    m_spring1.reset(0.0f);
    m_spring2.reset(0.0f);

    // Reset animation state
    m_phase = 0.0f;
    m_lastHopSeq = 0;
    m_targetEnergy = 0.0f;

    return true;
}

void CascadedSpringEffect::render(plugins::EffectContext& ctx) {
    // Get safe delta time for frame-rate independent physics
    float dt = ctx.getSafeDeltaSeconds();

    // ========================================================================
    // Audio Processing: Cascade raw energy through two springs
    // ========================================================================

    float rawEnergy = 0.5f;  // Fallback when no audio

#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        // Hop-based updates: update targets only on new audio hops
        // This prevents redundant updates between audio frames
        bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;

            // Combine RMS and bass for punchy response
            // RMS gives overall energy, bass gives body
            float rms = ctx.audio.rms();
            float bass = ctx.audio.bass();
            m_targetEnergy = rms * 0.6f + bass * 0.4f;
        }

        rawEnergy = m_targetEnergy;
    } else {
        // Fallback: slow sine breathing when no audio
        float fallbackPhase = (float)(ctx.totalTimeMs % 4000) / 4000.0f;
        rawEnergy = 0.4f + 0.2f * sinf(fallbackPhase * 6.283185f);
    }
#else
    // No audio feature: use time-based breathing
    float fallbackPhase = (float)(ctx.totalTimeMs % 4000) / 4000.0f;
    rawEnergy = 0.4f + 0.2f * sinf(fallbackPhase * 6.283185f);
#endif

    // ========================================================================
    // Cascaded Spring Smoothing
    // ========================================================================
    // Raw audio -> Spring 1 (fast) -> Spring 2 (slow) -> Ultra-smooth output
    //
    // Spring 1 catches transients with momentum
    // Spring 2 filters out any remaining jitter while preserving momentum

    float smooth1 = m_spring1.update(rawEnergy, dt);
    float smooth2 = m_spring2.update(smooth1, dt);

    // Ensure minimum visibility (never go completely black)
    float smoothEnergy = fmaxf(0.15f, smooth2);

    // ========================================================================
    // Animation: Speed modulated by smooth energy
    // ========================================================================

    // Speed scales with energy: 0.5x at silence, 2x at full energy
    float speedMod = 0.5f + smoothEnergy * 1.5f;

    // User speed control: ctx.speed is 1-100, normalize to ~0.1-1.0
    float userSpeed = (float)ctx.speed / 50.0f;

    // Accumulate phase (frame-rate independent)
    m_phase += userSpeed * speedMod * dt * 3.0f;

    // Wrap phase to prevent float precision loss over time
    if (m_phase > 1000.0f) {
        m_phase = fmodf(m_phase, 100.0f);
    }

    // ========================================================================
    // Render: CENTER ORIGIN wave pattern
    // ========================================================================

    // Clear buffer
    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));

    // Wave parameters
    float waveFreq = 0.12f;  // Spatial frequency (waves per LED)

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        // CENTER ORIGIN: distance from center pair (LEDs 79/80)
        uint16_t dist = centerPairDistance(i);
        float normalizedDist = (float)dist / (float)HALF_LENGTH;

        // Wave propagates outward from center
        // Phase increases with distance for outward motion
        float wave = sinf((float)dist * waveFreq - m_phase);

        // Wave amplitude modulated by smooth energy
        // Higher energy = taller waves
        float amplitude = 0.3f + smoothEnergy * 0.7f;
        float waveValue = wave * amplitude;

        // Convert wave (-1 to 1) to brightness (0 to 255)
        // Bias positive for visibility
        float brightFloat = (waveValue + 1.0f) * 0.5f;

        // Apply energy as overall brightness multiplier
        brightFloat *= (0.5f + smoothEnergy * 0.5f);

        // Scale by master brightness
        uint8_t brightness = (uint8_t)(brightFloat * (float)ctx.brightness);

        // Color: base hue + distance-based gradient
        // Subtle shift creates depth without violating NO RAINBOWS
        uint8_t hueOffset = (uint8_t)(normalizedDist * 32.0f);
        CRGB color = ctx.palette.getColor(ctx.gHue + hueOffset, brightness);

        // Strip 1: direct index
        ctx.leds[i] = color;

        // Strip 2: +90 hue offset for visual interest
        // Uses same distance-based calculation for symmetry
        if (i + STRIP_LENGTH < ctx.ledCount) {
            CRGB color2 = ctx.palette.getColor(ctx.gHue + hueOffset + 90, brightness);
            ctx.leds[i + STRIP_LENGTH] = color2;
        }
    }
}

void CascadedSpringEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& CascadedSpringEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Cascaded Spring",
        "Ultra-smooth waves via cascaded spring physics",
        plugins::EffectCategory::WATER,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
