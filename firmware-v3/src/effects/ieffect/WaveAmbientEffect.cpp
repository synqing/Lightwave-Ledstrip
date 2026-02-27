// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WaveAmbientEffect.cpp
 * @brief Wave Ambient - Time-driven sine wave with audio amplitude modulation
 *
 * Pattern: AMBIENT (Sensory Bridge Bloom-style)
 * - Motion: TIME-BASED (user speed parameter only)
 * - Audio: RMS → amplitude, Flux → brightness boost
 *
 * This is the "calm" wave effect. Speed is controlled by user parameter,
 * audio only affects brightness/amplitude. No jitter from audio metrics.
 *
 * For reactive wave motion, see WaveReactiveEffect.
 */

#include "WaveAmbientEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

// Flux boost decay rate
static constexpr float FLUX_BOOST_DECAY = 0.9f;

WaveAmbientEffect::WaveAmbientEffect()
    : m_waveOffset(0)
    , m_lastFlux(0.0f)
    , m_fluxBoost(0.0f)
{
}

bool WaveAmbientEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_waveOffset = 0;
    m_lastFlux = 0.0f;
    m_fluxBoost = 0.0f;
    return true;
}

void WaveAmbientEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN WAVE - Waves propagate from center
    // AMBIENT pattern: time-based speed, audio→amplitude only

    // Speed is TIME-BASED only (Sensory Bridge Bloom pattern)
    // NO audio→speed coupling - prevents jitter
    float waveSpeed = (float)ctx.speed;
    float amplitude = 1.0f;
    float waveFreq = 15.0f;  // Fixed wave frequency

#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        // Speed remains TIME-BASED - no modification from audio
        // This is the key difference from the old broken pattern

        // RMS drives amplitude (audio→brightness - the valid coupling)
        // sqrt scaling for more visible low-RMS response
        float rms = ctx.audio.rms();
        float rmsScaled = sqrtf(rms);  // 0.1 RMS -> 0.316 scaled
        amplitude = 0.1f + 0.9f * rmsScaled;  // 10-100% amplitude

        // Flux transient detection (brightness boost)
        float flux = ctx.audio.flux();
        float fluxDelta = flux - m_lastFlux;
        if (fluxDelta > 0.1f && flux > 0.2f) {
            m_fluxBoost = fmaxf(m_fluxBoost, flux);
        }
        m_lastFlux = flux;
    }
#endif

    // Update wave offset (time-based only)
    m_waveOffset += (uint32_t)waveSpeed;
    if (m_waveOffset > 65535) m_waveOffset = m_waveOffset % 65536;

    // Decay flux boost
    m_fluxBoost *= FLUX_BOOST_DECAY;
    if (m_fluxBoost < 0.01f) m_fluxBoost = 0.0f;

    // Gentle fade
    fadeToBlackBy(ctx.leds, ctx.ledCount, 12);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);

        // Wave propagates outward from center
        uint8_t rawBrightness = sin8((uint16_t)(distFromCenter * waveFreq) + (m_waveOffset >> 4));

        // Apply amplitude modulation from RMS (audio→brightness)
        uint8_t brightness = (uint8_t)(rawBrightness * amplitude);

        // Add flux boost for transients
        brightness = qadd8(brightness, (uint8_t)(m_fluxBoost * 50.0f));

        // Color follows wave with subtle motion
        uint8_t colorIndex = (uint8_t)(distFromCenter * 8) + (m_waveOffset >> 6);

        CRGB color = ctx.palette.getColor(colorIndex, brightness);

        ctx.leds[i] = color;
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = color;
        }
    }
}

void WaveAmbientEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& WaveAmbientEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Wave Ambient",
        "Time-driven sine wave with audio amplitude modulation",
        plugins::EffectCategory::WATER,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

