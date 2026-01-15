/**
 * @file LGPPerlinCausticsEffect.cpp
 * @brief LGP Perlin Caustics - Sparkling caustic lobes
 * 
 * Audio-reactive Perlin caustics effect:
 * - Multiple octaves of noise create caustic-like patterns
 * - Treble/hi-hat → increases sparkle density (higher frequency detail)
 * - Bass → increases lobe scale (larger features)
 * - Mid → brightness modulation
 */

#include "LGPPerlinCausticsEffect.h"
#include "../CoreEffects.h"
#include "../../config/features.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPPerlinCausticsEffect::LGPPerlinCausticsEffect()
    : m_noiseX(0)
    , m_noiseY(0)
    , m_noiseZ(0)
    , m_lastHopSeq(0)
    , m_targetTreble(0.0f)
    , m_targetBass(0.0f)
    , m_targetMid(0.0f)
    , m_targetHihat(0.0f)
    , m_smoothTreble(0.0f)
    , m_smoothBass(0.0f)
    , m_smoothMid(0.0f)
    , m_smoothHihat(0.0f)
    , m_time(0)
{
}

bool LGPPerlinCausticsEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_noiseX = random16();
    m_noiseY = random16();
    m_noiseZ = random16();
    m_lastHopSeq = 0;
    m_targetTreble = 0.0f;
    m_targetBass = 0.0f;
    m_targetMid = 0.0f;
    m_targetHihat = 0.0f;
    m_smoothTreble = 0.0f;
    m_smoothBass = 0.0f;
    m_smoothMid = 0.0f;
    m_smoothHihat = 0.0f;
    m_time = 0;
    
    // Reset AsymmetricFollower instances for mood integration
    m_trebleFollower.reset(0.0f);
    m_bassFollower.reset(0.0f);
    m_midFollower.reset(0.0f);
    m_hihatFollower.reset(0.0f);
    
    return true;
}

void LGPPerlinCausticsEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN - Caustic lobes radiating from centre
    const bool hasAudio = ctx.audio.available;
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;
    float complexityNorm = ctx.complexity / 255.0f;
    float variationNorm = ctx.variation / 255.0f;

    // =========================================================================
    // Audio Analysis (hop_seq checking for fresh data)
    // =========================================================================
    float trebleNorm = 0.0f;
    float bassNorm = 0.0f;
    float midNorm = 0.0f;
    float dt = ctx.getSafeDeltaSeconds();
    
#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        // Check for new audio hop (fresh data)
        bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;
            // Update targets only on new hops (fresh audio data)
            m_targetTreble = ctx.audio.treble();
            m_targetBass = ctx.audio.bass();
            m_targetMid = ctx.audio.mid();
            m_targetHihat = ctx.audio.hihat();
        }
        
        // Smooth toward targets every frame with MOOD-adjusted smoothing
        float moodNorm = ctx.getMoodNormalized();
        m_smoothTreble = m_trebleFollower.updateWithMood(m_targetTreble, dt, moodNorm);
        m_smoothBass = m_bassFollower.updateWithMood(m_targetBass, dt, moodNorm);
        m_smoothMid = m_midFollower.updateWithMood(m_targetMid, dt, moodNorm);
        m_smoothHihat = m_hihatFollower.updateWithMood(m_targetHihat, dt, moodNorm);
        
        // Use smoothed values
        trebleNorm = m_smoothTreble;
        bassNorm = m_smoothBass;
        midNorm = m_smoothMid;
        
        // Hi-hat also contributes to sparkle (using smoothed value)
        trebleNorm = fmaxf(trebleNorm, m_smoothHihat * 0.7f);
    } else {
        // Smooth audio parameters to zero when no audio
        float alpha = dt / (0.2f + dt);
        m_targetTreble = 0.0f;
        m_targetBass = 0.0f;
        m_targetMid = 0.0f;
        m_targetHihat = 0.0f;
        m_smoothTreble += (m_targetTreble - m_smoothTreble) * alpha;
        m_smoothBass += (m_targetBass - m_smoothBass) * alpha;
        m_smoothMid += (m_targetMid - m_smoothMid) * alpha;
        m_smoothHihat += (m_targetHihat - m_smoothHihat) * alpha;
    }
#endif

    // =========================================================================
    // Noise Field Updates
    // =========================================================================
    // Keep coordinate steps large enough to avoid flat sampling.
    uint16_t tStep = (uint16_t)(8 + (uint16_t)(speedNorm * 28.0f));
    m_time = (uint16_t)(m_time + tStep);
    m_noiseX = (uint16_t)(m_noiseX + (uint16_t)(17 + (tStep >> 1)));
    m_noiseY = (uint16_t)(m_noiseY + (uint16_t)(11 + (tStep >> 2)));
    m_noiseZ = (uint16_t)(m_noiseZ + (uint16_t)(5 + (tStep >> 3)));

    // =========================================================================
    // Rendering (centre-origin pattern with caustic lobes)
    // =========================================================================
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    // Audio-modulated parameters (kept conservative to avoid strobing)
    float sparkleDensity = (0.7f + trebleNorm * 0.9f) * (0.7f + complexityNorm * 0.8f);
    float lobeScale = (0.7f + bassNorm * 0.9f) * (0.75f + complexityNorm * 0.6f);
    float brightnessMod = 0.75f + midNorm * 0.25f;   // 0.75-1.0
    uint16_t variationOffset = (uint16_t)(ctx.variation * 257u);
    uint8_t paletteShift = (uint8_t)(variationNorm * 64.0f);
    uint16_t freq1 = (uint16_t)(12 + complexityNorm * 16.0f);
    uint16_t freq2 = (uint16_t)(20 + complexityNorm * 20.0f);
    uint16_t freq3 = (uint16_t)(6 + complexityNorm * 8.0f);

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        // Calculate distance from centre pair
        uint16_t dist = centerPairDistance(i);
        uint8_t dist8 = (uint8_t)dist;
        float distNorm = dist8 / 79.0f; // 0.0 at centre, 1.0 at edges
        
        // Sample multiple octaves of noise for caustic effect.
        // Use coordinate style consistent with existing working effects:
        // inoise8(dist*freq, timeShifted).
        uint8_t n1 = inoise8((uint16_t)(dist * (uint16_t)(freq1 * lobeScale) + (uint16_t)(m_noiseX + variationOffset)),
                             (uint16_t)(m_time >> 1));
        uint8_t n2 = inoise8((uint16_t)(dist * (uint16_t)(freq2 * sparkleDensity) + (uint16_t)(m_noiseX + 10000u + (variationOffset >> 1))),
                             (uint16_t)(m_time >> 2));
        uint8_t n3 = inoise8((uint16_t)(dist * freq3 + (uint16_t)(m_noiseY + 20000u + (variationOffset >> 2))),
                             (uint16_t)(m_time >> 3));

        // Caustic focus: multiplicative highlight shaping (cheap, punchy)
        uint16_t mul = (uint16_t)n1 * (uint16_t)n2;         // 0..65025
        uint8_t caustic8 = (uint8_t)(mul >> 8);             // 0..255
        caustic8 = qadd8((uint8_t)(caustic8 >> 1), (uint8_t)(n3 >> 2)); // add mild depth texture
        caustic8 = scale8(caustic8, caustic8);              // square for highlights
        float caustic = caustic8 / 255.0f;
        
        // Centre-focused falloff (caustics are brighter near centre)
        float centreFalloff = 1.0f - distNorm * 0.3f; // 1.0 at centre, 0.7 at edges
        caustic *= centreFalloff;
        
        // Map to palette and brightness
        uint8_t paletteIndex = (uint8_t)(caustic * 255.0f) + ctx.gHue + paletteShift;
        uint8_t brightness = (uint8_t)((0.3f + caustic * 0.7f) * brightnessMod * 255.0f * intensityNorm);
        
        CRGB color = ctx.palette.getColor(paletteIndex, brightness);
        
        // Apply to strip 1
        ctx.leds[i] = color;
        
        // Apply to strip 2 (mirrored, spectral offset for interference)
        if (i + STRIP_LENGTH < ctx.ledCount) {
            uint8_t paletteIndex2 = (uint8_t)(paletteIndex + 48);
            CRGB color2 = ctx.palette.getColor(paletteIndex2, brightness);
            ctx.leds[i + STRIP_LENGTH] = color2;
        }
    }
}

void LGPPerlinCausticsEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPPerlinCausticsEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Perlin Caustics",
        "Sparkling caustic lobes, treble→sparkle, bass→scale",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

