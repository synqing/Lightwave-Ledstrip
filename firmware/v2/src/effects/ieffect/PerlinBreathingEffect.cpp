/**
 * @file PerlinBreathingEffect.cpp
 * @brief Perlin Breathing - Organic noise field with beat-synchronized breathing
 *
 * Audio-reactive Perlin noise with beatsin16 modulation:
 * - beatsin16 (13 BPM) modulates spatial scale (zooming in/out)
 * - beatsin16 (7 BPM) modulates brightness envelope (slow breathe)
 * - Audio RMS enhances breathing depth
 * - Audio bass modulates color saturation
 *
 * CENTER ORIGIN: All patterns radiate from center pair (79/80)
 */

#include "PerlinBreathingEffect.h"
#include "../CoreEffects.h"
#include "../../config/features.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

PerlinBreathingEffect::PerlinBreathingEffect()
    : m_noiseTime(0)
    , m_noiseX(0)
    , m_noiseY(0)
    , m_lastHopSeq(0)
    , m_targetRms(0.0f)
    , m_targetBass(0.0f)
    , m_targetBeatStrength(0.0f)
    , m_smoothRms(0.0f)
    , m_smoothBass(0.0f)
    , m_smoothBeatStrength(0.0f)
{
}

bool PerlinBreathingEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Initialize noise field with random starting point
    m_noiseX = random16();
    m_noiseY = random16();
    m_noiseTime = 0;

    // Reset audio state
    m_lastHopSeq = 0;
    m_targetRms = 0.0f;
    m_targetBass = 0.0f;
    m_targetBeatStrength = 0.0f;

    // Initialize smoothing followers
    m_rmsFollower.reset(0.0f);
    m_bassFollower.reset(0.0f);
    m_beatFollower.reset(0.0f);
    m_smoothRms = 0.0f;
    m_smoothBass = 0.0f;
    m_smoothBeatStrength = 0.0f;

    return true;
}

void PerlinBreathingEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN - Perlin noise breathing from centre
    const bool hasAudio = ctx.audio.available;
    float dt = ctx.getSafeDeltaSeconds();
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.intensity / 255.0f;
    float complexityNorm = ctx.complexity / 255.0f;

    // =========================================================================
    // Audio Analysis & State Updates (hop_seq checking for fresh data)
    // =========================================================================
#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        // Check for new audio hop (fresh data)
        bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;
            // Update targets only on new hops (fresh audio data)
            m_targetRms = ctx.audio.rms();
            m_targetBass = ctx.audio.bass();
            m_targetBeatStrength = ctx.audio.beatStrength();
        }

        // Smooth toward targets every frame with MOOD-adjusted smoothing
        float moodNorm = ctx.getMoodNormalized();
        m_smoothRms = m_rmsFollower.updateWithMood(m_targetRms, dt, moodNorm);
        m_smoothBass = m_bassFollower.updateWithMood(m_targetBass, dt, moodNorm);
        m_smoothBeatStrength = m_beatFollower.updateWithMood(m_targetBeatStrength, dt, moodNorm);
    } else {
        // No audio: smooth to zero
        float alpha = dt / (0.2f + dt);
        m_targetRms = 0.0f;
        m_targetBass = 0.0f;
        m_targetBeatStrength = 0.0f;
        m_smoothRms += (m_targetRms - m_smoothRms) * alpha;
        m_smoothBass += (m_targetBass - m_smoothBass) * alpha;
        m_smoothBeatStrength += (m_targetBeatStrength - m_smoothBeatStrength) * alpha;
    }
#else
    (void)hasAudio;
#endif

    // =========================================================================
    // Noise Field Advection (time-based to prevent jitter)
    // =========================================================================
    // Advance noise time based on speed parameter
    m_noiseTime += (uint32_t)(80 + (uint32_t)(speedNorm * 120.0f));

    // Slow drift of noise field coordinates
    float angle = ctx.totalTimeMs * 0.0005f;
    float wobbleX = sinf(angle * 0.17f) * 8.0f;
    float wobbleY = cosf(angle * 0.13f) * 8.0f;

    uint16_t advX = (uint16_t)(3 + (uint16_t)fabsf(wobbleX) + (uint16_t)(speedNorm * 5.0f));
    uint16_t advY = (uint16_t)(2 + (uint16_t)fabsf(wobbleY) + (uint16_t)(speedNorm * 4.0f));

    m_noiseX = (uint16_t)(m_noiseX + advX);
    m_noiseY = (uint16_t)(m_noiseY + advY);

    // =========================================================================
    // beatsin16 Modulation (the core breathing mechanism)
    // =========================================================================
    // beatsin16 (13 BPM): modulates spatial scale - noise "zooms" in/out
    // Range 30-80 controls how stretched the noise appears
    uint16_t baseSpatialScale = beatsin16(13, 30, 80);

    // Audio enhancement: RMS increases spatial scale range (more dramatic zoom)
    uint16_t audioBoost = (uint16_t)(m_smoothRms * 40.0f);
    uint16_t spatialScale = baseSpatialScale + audioBoost;

    // beatsin16 (7 BPM): slower brightness envelope - the "breathing"
    // Range 150-255 keeps minimum brightness visible
    uint8_t baseBreathe = beatsin8(7, 150, 255);

    // Beat strength enhances breathing depth
    uint8_t beatBoost = (uint8_t)(m_smoothBeatStrength * 50.0f);
    uint8_t breatheBrightness = qadd8(baseBreathe, beatBoost);

    // =========================================================================
    // Rendering (centre-origin pattern)
    // =========================================================================
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    // Complexity affects noise detail (higher = more fine detail)
    uint16_t noiseDetail = (uint16_t)(8 + complexityNorm * 24.0f);

    // Variation adds secondary texture
    uint16_t variationOffset = (uint16_t)(ctx.variation * 127u);

    // Intensity affects overall brightness scaling
    uint8_t intensityScale = (uint8_t)(128 + intensityNorm * 127.0f);

    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        // Sample Perlin noise at this distance from center
        // The key formula: inoise16(x + dist * spatialScale, y + (noiseTime >> 4))
        uint32_t noiseX = (uint32_t)m_noiseX + (uint32_t)dist * spatialScale;
        uint32_t noiseY = (uint32_t)m_noiseY + (m_noiseTime >> 4);

        uint16_t noiseVal = inoise16(noiseX, noiseY);

        // Secondary noise layer for depth (offset coordinates)
        uint32_t noiseX2 = noiseX + 10000u + variationOffset;
        uint32_t noiseY2 = noiseY + 5000u;
        uint16_t noiseVal2 = inoise16(noiseX2, noiseY2);

        // Blend primary and secondary noise
        uint16_t blendedNoise = (noiseVal + noiseVal2) >> 1;

        // Calculate brightness with breathing modulation
        // scale8(noiseVal >> 8, breatheBrightness) applies the breathing envelope
        uint8_t noiseBright = blendedNoise >> 8;
        uint8_t brightness = scale8(noiseBright, breatheBrightness);
        brightness = scale8(brightness, intensityScale);

        // Center emphasis: brighter at center, fades toward edges
        uint8_t centreGain = (uint8_t)(255 - dist * 2);
        brightness = scale8(brightness, centreGain);

        // Final brightness scaling with master brightness
        brightness = scale8(brightness, ctx.brightness);

        // Calculate hue from noise with palette offset
        uint8_t hue = (uint8_t)(noiseVal >> 10) + ctx.gHue;

        // Bass modulation: shifts hue slightly for tonal variation
        uint8_t bassHueShift = (uint8_t)(m_smoothBass * 16.0f);
        hue = hue + bassHueShift;

        // Get color from palette
        CRGB color1 = ctx.palette.getColor(hue, brightness);

        // Apply to center pair symmetry (Strip 1)
        uint16_t left1 = CENTER_LEFT - dist;
        uint16_t right1 = CENTER_RIGHT + dist;
        if (left1 < ctx.ledCount) {
            ctx.leds[left1] = color1;
        }
        if (right1 < ctx.ledCount) {
            ctx.leds[right1] = color1;
        }

        // Strip 2: +24 hue offset for LGP interference patterns
        // Avoids rainbow sweep, creates complementary interference
        uint8_t hue2 = hue + 24;
        CRGB color2 = ctx.palette.getColor(hue2, brightness);

        uint16_t left2 = left1 + STRIP_LENGTH;
        uint16_t right2 = right1 + STRIP_LENGTH;
        if (left2 < ctx.ledCount) {
            ctx.leds[left2] = color2;
        }
        if (right2 < ctx.ledCount) {
            ctx.leds[right2] = color2;
        }
    }
}

void PerlinBreathingEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& PerlinBreathingEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Perlin Breathing",
        "Organic noise field with beatsin16 breathing modulation",
        plugins::EffectCategory::AMBIENT,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
