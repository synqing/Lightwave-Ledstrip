/**
 * @file WaveReactiveEffect.cpp
 * @brief Wave Reactive - Energy-accumulating wave with smooth audio-driven motion
 *
 * Pattern: REACTIVE (Sensory Bridge Kaleidoscope-style)
 * - Motion: ENERGY ACCUMULATION (pos += accumulated energy, not speed = metric)
 * - Audio: RMS -> accumulated energy -> wave speed boost
 * - Flux -> brightness boost on transients
 *
 * This is the "responsive" wave effect. Audio energy accumulates over time
 * and adds to base motion speed, creating smooth audio-driven movement
 * without the jitter of direct audio->speed coupling.
 *
 * For calm time-based wave motion, see WaveAmbientEffect.
 */

#include "WaveReactiveEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

// Energy accumulation constants (Kaleidoscope pattern)
static constexpr float ENERGY_ACCUMULATION_RATE = 0.5f;  // How fast energy accumulates from RMS
static constexpr float ENERGY_DECAY_RATE = 0.98f;        // 2% decay per frame - smooths motion
static constexpr float ENERGY_TO_SPEED_FACTOR = 10.0f;   // How much accumulated energy affects speed

// Flux boost constants
static constexpr float FLUX_BOOST_DECAY = 0.9f;          // Transient brightness decay
static constexpr float FLUX_THRESHOLD = 0.1f;            // Delta threshold for transient detection
static constexpr float FLUX_MIN_LEVEL = 0.2f;            // Minimum flux to trigger boost
static constexpr float FLUX_BOOST_BRIGHTNESS = 50.0f;    // Max brightness boost from flux

WaveReactiveEffect::WaveReactiveEffect()
    : m_waveOffset(0)
    , m_energyAccum(0.0f)
    , m_lastFlux(0.0f)
    , m_fluxBoost(0.0f)
{
}

bool WaveReactiveEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_waveOffset = 0;
    m_energyAccum = 0.0f;
    m_lastFlux = 0.0f;
    m_fluxBoost = 0.0f;
    return true;
}

void WaveReactiveEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN WAVE - Waves propagate from center
    // REACTIVE pattern: energy accumulation drives motion

    // Base speed from user parameter
    float baseSpeed = (float)ctx.speed;
    float waveFreq = 15.0f;  // Fixed wave frequency

#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        // REACTIVE pattern: Audio drives motion through ACCUMULATION
        // This is the key insight from Sensory Bridge Kaleidoscope

        // Step 1: Accumulate energy from RMS
        float rms = ctx.audio.rms();
        m_energyAccum += rms * ENERGY_ACCUMULATION_RATE;

        // Step 2: Decay accumulated energy (prevents runaway, creates smoothness)
        m_energyAccum *= ENERGY_DECAY_RATE;

        // Step 3: Flux transient detection (brightness boost)
        float flux = ctx.audio.flux();
        float fluxDelta = flux - m_lastFlux;
        if (fluxDelta > FLUX_THRESHOLD && flux > FLUX_MIN_LEVEL) {
            m_fluxBoost = fmaxf(m_fluxBoost, flux);
        }
        m_lastFlux = flux;
    }
#endif

    // Update wave offset with energy-augmented speed
    // REACTIVE pattern: accumulated energy adds to base speed, not replaces it
    float effectiveSpeed = baseSpeed + m_energyAccum * ENERGY_TO_SPEED_FACTOR;
    m_waveOffset += (uint32_t)effectiveSpeed;
    if (m_waveOffset > 65535) m_waveOffset = m_waveOffset % 65536;

    // Decay flux boost for transient brightness
    m_fluxBoost *= FLUX_BOOST_DECAY;
    if (m_fluxBoost < 0.01f) m_fluxBoost = 0.0f;

    // Gentle fade for smooth trails
    fadeToBlackBy(ctx.leds, ctx.ledCount, 12);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);

        // Wave propagates outward from center
        // Wave position driven by time + accumulated energy
        uint8_t rawBrightness = sin8((uint16_t)(distFromCenter * waveFreq) + (m_waveOffset >> 4));

        // Add flux boost for transient brightness
        uint8_t brightness = qadd8(rawBrightness, (uint8_t)(m_fluxBoost * FLUX_BOOST_BRIGHTNESS));

        // Color follows wave with subtle motion
        uint8_t colorIndex = (uint8_t)(distFromCenter * 8) + (m_waveOffset >> 6);

        CRGB color = ctx.palette.getColor(colorIndex, brightness);

        // Apply to both strips (CENTER ORIGIN symmetric rendering)
        ctx.leds[i] = color;
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = color;
        }
    }
}

void WaveReactiveEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& WaveReactiveEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Wave Reactive",
        "Energy-accumulating wave with smooth audio-driven motion",
        plugins::EffectCategory::WATER,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
