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
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:WaveAmbientEffect
namespace {
constexpr float kWaveAmbientEffectSpeedScale = 1.0f;
constexpr float kWaveAmbientEffectOutputGain = 1.0f;
constexpr float kWaveAmbientEffectCentreBias = 1.0f;

float gWaveAmbientEffectSpeedScale = kWaveAmbientEffectSpeedScale;
float gWaveAmbientEffectOutputGain = kWaveAmbientEffectOutputGain;
float gWaveAmbientEffectCentreBias = kWaveAmbientEffectCentreBias;

const lightwaveos::plugins::EffectParameter kWaveAmbientEffectParameters[] = {
    {"wave_ambient_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kWaveAmbientEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"wave_ambient_effect_output_gain", "Output Gain", 0.25f, 2.0f, kWaveAmbientEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"wave_ambient_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kWaveAmbientEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:WaveAmbientEffect

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
    // AUTO_TUNABLES_BULK_RESET_BEGIN:WaveAmbientEffect
    gWaveAmbientEffectSpeedScale = kWaveAmbientEffectSpeedScale;
    gWaveAmbientEffectOutputGain = kWaveAmbientEffectOutputGain;
    gWaveAmbientEffectCentreBias = kWaveAmbientEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:WaveAmbientEffect


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


// AUTO_TUNABLES_BULK_METHODS_BEGIN:WaveAmbientEffect
uint8_t WaveAmbientEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kWaveAmbientEffectParameters) / sizeof(kWaveAmbientEffectParameters[0]));
}

const plugins::EffectParameter* WaveAmbientEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kWaveAmbientEffectParameters[index];
}

bool WaveAmbientEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "wave_ambient_effect_speed_scale") == 0) {
        gWaveAmbientEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "wave_ambient_effect_output_gain") == 0) {
        gWaveAmbientEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "wave_ambient_effect_centre_bias") == 0) {
        gWaveAmbientEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float WaveAmbientEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "wave_ambient_effect_speed_scale") == 0) return gWaveAmbientEffectSpeedScale;
    if (strcmp(name, "wave_ambient_effect_output_gain") == 0) return gWaveAmbientEffectOutputGain;
    if (strcmp(name, "wave_ambient_effect_centre_bias") == 0) return gWaveAmbientEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:WaveAmbientEffect

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

