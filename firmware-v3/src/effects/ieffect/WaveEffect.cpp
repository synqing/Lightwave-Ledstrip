/**
 * @file WaveEffect.cpp
 * @brief Audio-reactive wave effect implementation
 *
 * Audio Integration (Bloom-inspired):
 * - RMS → wave amplitude (louder = taller waves)
 * - Beat phase → wave speed (synced to tempo when confident)
 * - Flux → brightness boost on transients
 *
 * Fallback strategy:
 * - High confidence (>0.5): Beat-synced wave speed
 * - Medium (0.2-0.5): Slow speed + flux brightness boost
 * - Low (<0.2): RMS-modulated speed
 * - No audio: Time-based fallback
 */

#include "WaveEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

// Flux boost decay rate
static constexpr float FLUX_BOOST_DECAY = 0.9f;

WaveEffect::WaveEffect()
    : m_waveOffset(0)
    , m_fallbackPhase(0.0f)
    , m_lastFlux(0.0f)
    , m_fluxBoost(0.0f)
{
}

bool WaveEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_waveOffset = 0;
    m_fallbackPhase = 0.0f;
    m_lastFlux = 0.0f;
    m_fluxBoost = 0.0f;
    return true;
}

void WaveEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN WAVE - Waves propagate from center
    // Now with audio-reactive amplitude and beat-synced speed

    // Default values (no audio)
    float waveSpeed = (float)ctx.speed;
    float amplitude = 1.0f;
    float waveFreq = 15.0f;  // Base wave frequency

#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        float tempoConf = ctx.audio.tempoConfidence();

        // Determine wave speed based on tempo confidence
        if (tempoConf > 0.5f) {
            // HIGH CONFIDENCE: Beat-synced wave speed
            float beatPhase = ctx.audio.beatPhase();
            // Wave speed oscillates with beat (0.8x to 1.2x base)
            float beatMod = sinf(beatPhase * 2.0f * 3.14159f);
            waveSpeed = ctx.speed * (0.8f + 0.4f * (beatMod * 0.5f + 0.5f));
        } else if (tempoConf > 0.2f) {
            // MEDIUM CONFIDENCE: Slower base speed
            waveSpeed = ctx.speed * 0.7f;
        } else {
            // LOW CONFIDENCE: RMS-modulated speed
            float rms = ctx.audio.rms();
            waveSpeed = ctx.speed * (0.5f + 0.8f * rms);
        }

        // RMS ALWAYS drives amplitude (the fundamental audio-visual binding)
        // FIXED: Use sqrt scaling for much more visible low-RMS response
        float rms = ctx.audio.rms();
        float rmsScaled = sqrtf(rms);  // 0.1 RMS -> 0.316 scaled
        amplitude = 0.1f + 0.9f * rmsScaled;  // 10-100% amplitude (DRAMATIC)

        // Flux transient detection
        float flux = ctx.audio.flux();
        float fluxDelta = flux - m_lastFlux;
        if (fluxDelta > 0.1f && flux > 0.2f) {
            m_fluxBoost = fmaxf(m_fluxBoost, flux);
        }
        m_lastFlux = flux;

        // FIXED: Keep wave frequency constant to avoid jitter
        // Wave density was too subtle and caused visual noise
        waveFreq = 15.0f;
    }
#endif

    // Update wave offset
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

        // Apply amplitude modulation from RMS
        uint8_t brightness = (uint8_t)(rawBrightness * amplitude);

        // Add flux boost for transients
        brightness = qadd8(brightness, (uint8_t)(m_fluxBoost * 50.0f));

        // Color follows wave with subtle audio modulation
        uint8_t colorIndex = (uint8_t)(distFromCenter * 8) + (m_waveOffset >> 6);

        CRGB color = ctx.palette.getColor(colorIndex, brightness);

        ctx.leds[i] = color;
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = color;
        }
    }
}

void WaveEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& WaveEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Wave",
        "Audio-reactive sine wave with beat sync and transient boost",
        plugins::EffectCategory::WATER,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
