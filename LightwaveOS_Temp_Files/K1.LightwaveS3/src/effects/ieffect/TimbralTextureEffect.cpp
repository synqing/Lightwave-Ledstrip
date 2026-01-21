/**
 * @file TimbralTextureEffect.cpp
 * @brief Timbral Texture - Visual complexity tracks audio texture changes
 *
 * Implementation of multi-octave Perlin noise where octave count (visual
 * complexity) is driven by timbralSaliency(). This is the FIRST effect
 * to utilize timbralSaliency in the codebase.
 *
 * CENTER ORIGIN: All patterns radiate from LED 79/80 (center pair).
 */

#include "TimbralTextureEffect.h"
#include "../CoreEffects.h"
#include "../../config/features.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

TimbralTextureEffect::TimbralTextureEffect()
    : m_noiseTime(0)
    , m_noiseX(0)
    , m_noiseY(0)
    , m_smoothedTimbre(0.0f)
    , m_currentOctaves(1.0f)
    , m_smoothedRms(0.5f)
    , m_lastHopSeq(0)
{
}

bool TimbralTextureEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Initialize noise coordinates with random offset
    m_noiseX = random16();
    m_noiseY = random16();
    m_noiseTime = 0;

    // Initialize timbral tracking
    m_smoothedTimbre = 0.3f;  // Start with moderate complexity
    m_currentOctaves = 1.0f + m_smoothedTimbre * 2.0f;

    // Reset smoothing followers
    m_timbreFollower.reset(0.3f);
    m_rmsFollower.reset(0.5f);
    m_smoothedRms = 0.5f;

    m_lastHopSeq = 0;

    return true;
}

void TimbralTextureEffect::render(plugins::EffectContext& ctx) {
    // =========================================================================
    // Timing and Parameters
    // =========================================================================
    float dt = ctx.getSafeDeltaSeconds();
    float speedNorm = ctx.speed / 50.0f;

    // Advance noise time (TIME-BASED motion, not audio-driven)
    m_noiseTime += (uint32_t)(80 * speedNorm);

    // =========================================================================
    // Audio Analysis: Track Timbral Saliency
    // =========================================================================
    float targetTimbre = 0.3f;  // Default: moderate complexity when no audio
    float targetRms = 0.5f;

#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        // Check for new audio hop (fresh data)
        bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;

            // PRIMARY FEATURE: timbralSaliency drives visual complexity
            // This is the FIRST effect to use timbralSaliency!
            targetTimbre = ctx.audio.timbralSaliency();

            // Secondary: RMS for subtle brightness modulation
            targetRms = ctx.audio.rms();
        }

        // Smooth toward targets with MOOD-adjusted time constants
        float moodNorm = ctx.getMoodNormalized();
        m_smoothedTimbre = m_timbreFollower.updateWithMood(targetTimbre, dt, moodNorm);
        m_smoothedRms = m_rmsFollower.updateWithMood(targetRms, dt, moodNorm);
    } else {
        // No audio: gentle time-based breathing for complexity
        float phase = ctx.totalTimeMs * 0.0005f;  // ~2 second period
        targetTimbre = 0.3f + 0.2f * sinf(phase);
        m_smoothedTimbre = m_timbreFollower.update(targetTimbre, dt);
        m_smoothedRms = m_rmsFollower.update(0.5f, dt);
    }
#else
    // No audio sync compiled: ambient mode
    float phase = ctx.totalTimeMs * 0.0005f;
    targetTimbre = 0.3f + 0.2f * sinf(phase);
    float alpha = 1.0f - expf(-dt / 0.2f);
    m_smoothedTimbre += (targetTimbre - m_smoothedTimbre) * alpha;
    m_smoothedRms = 0.5f;
#endif

    // =========================================================================
    // Map Timbral Saliency to Visual Parameters
    // =========================================================================

    // Octave count: 1.0 (simple) to 3.0 (complex)
    m_currentOctaves = 1.0f + m_smoothedTimbre * 2.0f;
    uint8_t octaves = (uint8_t)m_currentOctaves;
    if (octaves < 1) octaves = 1;
    if (octaves > 3) octaves = 3;

    // Spatial scale: higher timbre = finer detail (smaller scale)
    // Range: 60 (smooth, low timbre) to 30 (detailed, high timbre)
    uint16_t spatialScale = 60 - (uint16_t)(m_smoothedTimbre * 30.0f);

    // Brightness modulation from RMS (subtle energy feedback)
    uint8_t brightnessBase = 180 + (uint8_t)(m_smoothedRms * 75.0f);  // 180-255

    // =========================================================================
    // Fade previous frame (trail effect)
    // =========================================================================
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    // =========================================================================
    // CENTER ORIGIN Rendering
    // =========================================================================
    // Render from center outward, symmetric on both sides

    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        // Noise coordinates based on distance from center
        uint32_t noiseXCoord = m_noiseX + (uint32_t)dist * spatialScale;
        uint32_t noiseYCoord = m_noiseY + (m_noiseTime >> 4);

        // Multi-octave fractal noise
        uint16_t noiseVal = fractalNoise(noiseXCoord, noiseYCoord, octaves);

        // Map noise to brightness (with center emphasis)
        uint8_t noiseBright = noiseVal >> 8;  // 0-255
        uint8_t centerGain = 255 - (dist * 2);  // Brighter at center
        uint8_t brightness = scale8(noiseBright, centerGain);
        brightness = scale8(brightness, brightnessBase);
        brightness = scale8(brightness, ctx.brightness);

        // Hue from noise with global hue offset
        uint8_t hue = ctx.gHue + (noiseVal >> 10);

        // Get color from palette
        CRGB color1 = ctx.palette.getColor(hue, brightness);

        // =====================================================================
        // Strip 1: Center pair expansion (LEDs 0-159)
        // =====================================================================
        uint16_t left1 = CENTER_LEFT - dist;   // 79, 78, 77, ... 0
        uint16_t right1 = CENTER_RIGHT + dist; // 80, 81, 82, ... 159

        if (left1 < ctx.ledCount) {
            ctx.leds[left1] = color1;
        }
        if (right1 < ctx.ledCount) {
            ctx.leds[right1] = color1;
        }

        // =====================================================================
        // Strip 2: +90 hue offset for LGP interference (LEDs 160-319)
        // =====================================================================
        uint8_t hue2 = hue + 90;  // +90 hue offset (NOT rainbow - controlled offset)
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

    // Advance noise coordinates for next frame (slow drift)
    m_noiseX += (uint16_t)(2 + speedNorm * 4);
    m_noiseY += (uint16_t)(1 + speedNorm * 2);
}

uint16_t TimbralTextureEffect::fractalNoise(uint32_t x, uint32_t y, uint8_t octaves) {
    // Fractal Brownian motion (fBm) implementation
    // Each octave adds finer detail at half amplitude

    uint32_t sum = 0;
    uint32_t amplitude = 1 << 15;  // Start at half of 16-bit range

    for (uint8_t i = 0; i < octaves; i++) {
        // Get noise value for this octave
        uint16_t noise = inoise16(x, y);

        // Add weighted contribution
        sum += ((uint32_t)noise * amplitude) >> 16;

        // Next octave: double frequency, halve amplitude
        amplitude >>= 1;
        x <<= 1;
        y <<= 1;
    }

    // Clamp to 16-bit range
    if (sum > 65535) sum = 65535;

    return (uint16_t)sum;
}

void TimbralTextureEffect::cleanup() {
    // No dynamic resources to free
}

const plugins::EffectMetadata& TimbralTextureEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Timbral Texture",
        "Visual complexity tracks audio texture changes via timbralSaliency",
        plugins::EffectCategory::AMBIENT,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
