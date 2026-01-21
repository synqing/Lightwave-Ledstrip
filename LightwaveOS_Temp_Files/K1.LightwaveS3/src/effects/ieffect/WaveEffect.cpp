/**
 * @file WaveEffect.cpp
 * @brief Audio-reactive wave effect implementation
 *
 * Visual Foundation: Time-based wave propagation from center
 * Audio Enhancement: Audio modulates amplitude/brightness only
 *
 * Audio Integration (Visual-First Pattern):
 * - RMS → wave amplitude (louder = taller waves)
 * - Flux → brightness boost on transients
 * - Speed: TIME-BASED (prevents jitter)
 *
 * Pattern: Follows WaveAmbientEffect design - time-based speed,
 * audio modulates brightness/amplitude only.
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
    m_lastHopSeq = 0;
    m_targetRms = 0.0f;
    m_rmsFollower.reset(0.0f);
    return true;
}

void WaveEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN WAVE - Waves propagate from center
    // Visual Foundation: Time-based wave propagation
    // Audio Enhancement: Audio modulates amplitude/brightness only (prevents jitter)

    // Speed is TIME-BASED only (prevents jitter from audio→speed coupling)
    float waveSpeed = (float)ctx.speed;
    float amplitude = 1.0f;
    float waveFreq = 15.0f;  // Fixed wave frequency

#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        float dt = ctx.getSafeDeltaSeconds();
        float moodNorm = ctx.getMoodNormalized();
        
        // Hop-based updates: update targets only on new hops
        bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;
            m_targetRms = ctx.audio.rms();
        }
        
        // Smooth toward targets every frame with MOOD-adjusted smoothing
        float rms = m_rmsFollower.updateWithMood(m_targetRms, dt, moodNorm);
        
        // Speed remains TIME-BASED - no modification from audio
        // This prevents jitter from noisy audio metrics

        // RMS drives amplitude (audio→brightness - the valid coupling)
        // sqrt scaling for more visible low-RMS response
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
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

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
