/**
 * @file LGPPerlinVeilEffect.cpp
 * @brief LGP Perlin Veil - Slow drifting curtains/fog from centre
 * 
 * Audio-reactive Perlin noise field effect:
 * - Two independent noise fields: hue index and luminance mask
 * - Centre-origin sampling: distance from centre pair (79/80)
 * - Audio drives advection speed (flux/beatStrength) and contrast (RMS)
 * - Bass modulates depth variation
 */

#include "LGPPerlinVeilEffect.h"
#include "../CoreEffects.h"
#include "../../config/features.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPPerlinVeilEffect::LGPPerlinVeilEffect()
    : m_noiseX(0)
    , m_noiseY(0)
    , m_noiseZ(0)
    , m_contrast(0.5f)
    , m_depthVariation(0.0f)
    , m_lastHopSeq(0)
    , m_targetRms(0.0f)
    , m_targetFlux(0.0f)
    , m_targetBeatStrength(0.0f)
    , m_targetBass(0.0f)
    , m_smoothRms(0.0f)
    , m_smoothFlux(0.0f)
    , m_smoothBeatStrength(0.0f)
    , m_smoothBass(0.0f)
    , m_time(0)
{
}

bool LGPPerlinVeilEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_noiseX = random16();
    m_noiseY = random16();
    m_noiseZ = random16();
    m_contrast = 0.5f;
    m_depthVariation = 0.0f;
    m_lastHopSeq = 0;
    m_targetRms = 0.0f;
    m_targetFlux = 0.0f;
    m_targetBeatStrength = 0.0f;
    m_targetBass = 0.0f;
    // Initialize smoothing followers
    m_rmsFollower.reset(0.0f);
    m_fluxFollower.reset(0.0f);
    m_beatFollower.reset(0.0f);
    m_bassFollower.reset(0.0f);
    m_smoothRms = 0.0f;
    m_smoothFlux = 0.0f;
    m_smoothBeatStrength = 0.0f;
    m_smoothBass = 0.0f;
    m_time = 0;
    return true;
}

void LGPPerlinVeilEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN - Perlin noise veils drifting from centre
    const bool hasAudio = ctx.audio.available;
    float dt = ctx.getSafeDeltaSeconds();
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;
    float complexityNorm = ctx.complexity / 255.0f;
    float variationNorm = ctx.variation / 255.0f;

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
            m_targetFlux = ctx.audio.flux();
            m_targetBeatStrength = ctx.audio.beatStrength();
            m_targetBass = ctx.audio.bass();
        }
        
        // Smooth toward targets every frame with MOOD-adjusted smoothing
        float moodNorm = ctx.getMoodNormalized();
        m_smoothRms = m_rmsFollower.updateWithMood(m_targetRms, dt, moodNorm);
        m_smoothFlux = m_fluxFollower.updateWithMood(m_targetFlux, dt, moodNorm);
        m_smoothBeatStrength = m_beatFollower.updateWithMood(m_targetBeatStrength, dt, moodNorm);
        m_smoothBass = m_bassFollower.updateWithMood(m_targetBass, dt, moodNorm);
        
        // Audio Enhancement: Modulate contrast and depth (NOT momentum)
        // Momentum is TIME-BASED to prevent jitter
        
        // RMS → contrast modulation (using smoothed value)
        float targetContrast = 0.3f + m_smoothRms * 0.7f;
        m_contrast += (targetContrast - m_contrast) * (dt / (0.2f + dt)); // ~200ms smoothing
        
        // Bass → depth variation (using smoothed value)
        m_depthVariation = m_smoothBass * 0.5f;
    } else {
        // Ambient mode: time-based momentum
        // Momentum is time-based, not audio-driven
        m_contrast = 0.4f + 0.2f * sinf(ctx.totalTimeMs * 0.001f); // Slow breathing
        m_depthVariation = 0.0f;
        // Smooth audio parameters to zero when no audio
        float alpha = dt / (0.2f + dt);
        m_targetRms = 0.0f;
        m_targetFlux = 0.0f;
        m_targetBeatStrength = 0.0f;
        m_targetBass = 0.0f;
        m_smoothRms += (m_targetRms - m_smoothRms) * alpha;
        m_smoothFlux += (m_targetFlux - m_smoothFlux) * alpha;
        m_smoothBeatStrength += (m_targetBeatStrength - m_smoothBeatStrength) * alpha;
        m_smoothBass += (m_targetBass - m_smoothBass) * alpha;
    }
#else
    // No audio: ambient mode
    // Momentum is time-based, not audio-driven
    m_contrast = 0.4f + 0.2f * sinf(ctx.totalTimeMs * 0.001f);
    m_depthVariation = 0.0f;
#endif

    // =========================================================================
    // Noise Field Advection (centre-origin sampling)
    // Visual Foundation: TIME-BASED advection (prevents jitter)
    // =========================================================================
    // Base drift (slow wobble like Emotiscope), but keep coordinates in the
    // same "scale space" as existing working inoise8 usage (i*5, time>>3 etc.)
    float angle = ctx.totalTimeMs * 0.001f;
    float wobble = sinf(angle * 0.12f);

    // Momentum is TIME-BASED (speedNorm * dt), not audio-driven
    // This prevents jitter from audio→momentum coupling
    float timeBasedMomentum = speedNorm * 0.5f;  // Time-based momentum
    
    // Keep coordinate deltas large enough to create visible structure.
    // (The previous >>8 coordinate collapse made the field near-constant.)
    uint16_t advX = (uint16_t)(20 + (uint16_t)(wobble * 12.0f) + (uint16_t)(timeBasedMomentum * 900.0f));
    uint16_t advY = (uint16_t)(18 + (uint16_t)(timeBasedMomentum * 1200.0f));
    uint16_t advZ = (uint16_t)(4 + (uint16_t)(m_depthVariation * 180.0f));

    // Speed scales the drift rate, but clamp to avoid "teleporting" noise.
    uint16_t speedScale = (uint16_t)(6 + (uint16_t)(speedNorm * 20.0f));
    m_noiseX = (uint16_t)(m_noiseX + advX * speedScale);
    m_noiseY = (uint16_t)(m_noiseY + advY * speedScale);
    m_noiseZ = (uint16_t)(m_noiseZ + advZ * (uint16_t)(1 + (speedScale >> 2)));

    // Time accumulator kept for legacy, but not used for sampling directly.
    m_time = (uint16_t)(m_time + (uint16_t)(1 + (speedScale >> 3)));

    // =========================================================================
    // Rendering (centre-origin pattern)
    // =========================================================================
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);
    uint16_t variationOffset = (uint16_t)(ctx.variation * 197u);
    uint8_t paletteShift = (uint8_t)(variationNorm * 56.0f);
    uint16_t detail1 = (uint16_t)(22 + complexityNorm * 28.0f);
    uint16_t detail2 = (uint16_t)(40 + complexityNorm * 36.0f);
    uint8_t contrast8 = (uint8_t)(64 + (uint8_t)(m_contrast * 191.0f)); // 64..255

    uint16_t x1 = (uint16_t)(m_noiseX + variationOffset);
    uint16_t x2 = (uint16_t)(m_noiseX + 10000u + (variationOffset >> 1));
    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        uint8_t dist8 = (uint8_t)dist;

        // Visible "veil" wants broad structures: use low spatial frequency,
        // but still enough delta across 0..79 to show gradients.
        uint16_t y1 = (uint16_t)(m_noiseY + (variationOffset >> 2) + (uint16_t)(dist8 << 2));
        uint16_t z1 = (uint16_t)(m_noiseZ + (variationOffset >> 3));

        uint16_t y2 = (uint16_t)(m_noiseY + 5000u + (variationOffset >> 3) + (uint16_t)(dist8 << 3));

        // Sample two independent 3D noise fields (time is implicit via m_noise*)
        uint8_t hueNoise = inoise8(x1, y1, z1);
        uint8_t lumNoise = inoise8(x2, y2);

        // Palette index: avoid hue-wheel logic; palette selection defines colour language
        uint8_t paletteIndex = hueNoise + ctx.gHue + paletteShift;

        // Luminance shaping: contrast + centre emphasis (fast, integer math)
        uint8_t lum = scale8(lumNoise, lumNoise); // square for highlights
        lum = scale8(lum, contrast8);
        uint8_t centreGain = (uint8_t)(255 - dist8 * 2); // 255..97
        lum = scale8(lum, centreGain);

        // Brightness range: keep a floor so it doesn't vanish at low values
        uint8_t brightnessBase = qadd8(46, scale8(lum, 209)); // 0.18 + 0.82*lum
        uint8_t brightness = scale8(brightnessBase, ctx.brightness);

        CRGB color1 = ctx.palette.getColor(paletteIndex, brightness);

        uint16_t left1 = CENTER_LEFT - dist;
        uint16_t right1 = CENTER_RIGHT + dist;
        if (left1 < ctx.ledCount) {
            ctx.leds[left1] = color1;
        }
        if (right1 < ctx.ledCount) {
            ctx.leds[right1] = color1;
        }

        // Second strip offset to encourage LGP interference without "rainbow sweeps"
        uint8_t paletteIndex2 = (uint8_t)(paletteIndex + 24);
        CRGB color2 = ctx.palette.getColor(paletteIndex2, brightness);
        uint16_t left2 = left1 + STRIP_LENGTH;
        uint16_t right2 = right1 + STRIP_LENGTH;
        if (left2 < ctx.ledCount) {
            ctx.leds[left2] = color2;
        }
        if (right2 < ctx.ledCount) {
            ctx.leds[right2] = color2;
        }

        x1 = (uint16_t)(x1 + detail1);
        x2 = (uint16_t)(x2 + detail2);
    }
}

void LGPPerlinVeilEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPPerlinVeilEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Perlin Veil",
        "Slow drifting noise curtains from centre, audio-driven advection",
        plugins::EffectCategory::PARTY,  // Audio-reactive, not ambient
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

