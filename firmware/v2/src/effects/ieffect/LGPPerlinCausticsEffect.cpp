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
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:LGPPerlinCausticsEffect
namespace {
constexpr float kLGPPerlinCausticsEffectSpeedScale = 1.0f;
constexpr float kLGPPerlinCausticsEffectOutputGain = 1.0f;
constexpr float kLGPPerlinCausticsEffectCentreBias = 1.0f;

float gLGPPerlinCausticsEffectSpeedScale = kLGPPerlinCausticsEffectSpeedScale;
float gLGPPerlinCausticsEffectOutputGain = kLGPPerlinCausticsEffectOutputGain;
float gLGPPerlinCausticsEffectCentreBias = kLGPPerlinCausticsEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPPerlinCausticsEffectParameters[] = {
    {"lgpperlin_caustics_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPPerlinCausticsEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpperlin_caustics_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPPerlinCausticsEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpperlin_caustics_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPPerlinCausticsEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPPerlinCausticsEffect

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
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPPerlinCausticsEffect
    gLGPPerlinCausticsEffectSpeedScale = kLGPPerlinCausticsEffectSpeedScale;
    gLGPPerlinCausticsEffectOutputGain = kLGPPerlinCausticsEffectOutputGain;
    gLGPPerlinCausticsEffectCentreBias = kLGPPerlinCausticsEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPPerlinCausticsEffect

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
    return true;
}

void LGPPerlinCausticsEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN - Caustic lobes radiating from centre
    const bool hasAudio = ctx.audio.available;
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    // =========================================================================
    // Audio Analysis (hop_seq checking for fresh data)
    // =========================================================================
    float trebleNorm = 0.0f;
    float bassNorm = 0.0f;
    float midNorm = 0.0f;
    float dt = ctx.getSafeRawDeltaSeconds();
    
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
        
        // Smooth toward targets every frame (keeps motion alive between hops)
        float alpha = 1.0f - expf(-dt / 0.15f);  // True exponential, tau=150ms
        m_smoothTreble += (m_targetTreble - m_smoothTreble) * alpha;
        m_smoothBass += (m_targetBass - m_smoothBass) * alpha;
        m_smoothMid += (m_targetMid - m_smoothMid) * alpha;
        m_smoothHihat += (m_targetHihat - m_smoothHihat) * alpha;
        
        // Use smoothed values
        trebleNorm = m_smoothTreble;
        bassNorm = m_smoothBass;
        midNorm = m_smoothMid;
        
        // Hi-hat also contributes to sparkle (using smoothed value)
        trebleNorm = fmaxf(trebleNorm, m_smoothHihat * 0.7f);
    } else {
        // Smooth audio parameters to zero when no audio (true exponential, tau=200ms)
        float alpha = 1.0f - expf(-dt / 0.2f);
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
    float sparkleDensity = 0.8f + trebleNorm * 0.9f; // 0.8-1.7
    float lobeScale = 0.8f + bassNorm * 0.9f;        // 0.8-1.7
    float brightnessMod = 0.75f + midNorm * 0.25f;   // 0.75-1.0

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        // Calculate distance from centre pair
        uint16_t dist = centerPairDistance(i);
        uint8_t dist8 = (uint8_t)dist;
        float distNorm = dist8 / 79.0f; // 0.0 at centre, 1.0 at edges
        
        // Sample multiple octaves of noise for caustic effect.
        // Use coordinate style consistent with existing working effects:
        // inoise8(dist*freq, timeShifted).
        uint8_t n1 = inoise8((uint16_t)(dist * (uint16_t)(14 * lobeScale) + m_noiseX),
                             (uint16_t)(m_time >> 1));
        uint8_t n2 = inoise8((uint16_t)(dist * (uint16_t)(29 * sparkleDensity) + (m_noiseX + 10000u)),
                             (uint16_t)(m_time >> 2));
        uint8_t n3 = inoise8((uint16_t)(dist * 7 + (m_noiseY + 20000u)),
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
        uint8_t paletteIndex = (uint8_t)(caustic * 255.0f) + ctx.gHue;
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


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPPerlinCausticsEffect
uint8_t LGPPerlinCausticsEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPPerlinCausticsEffectParameters) / sizeof(kLGPPerlinCausticsEffectParameters[0]));
}

const plugins::EffectParameter* LGPPerlinCausticsEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPPerlinCausticsEffectParameters[index];
}

bool LGPPerlinCausticsEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpperlin_caustics_effect_speed_scale") == 0) {
        gLGPPerlinCausticsEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpperlin_caustics_effect_output_gain") == 0) {
        gLGPPerlinCausticsEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpperlin_caustics_effect_centre_bias") == 0) {
        gLGPPerlinCausticsEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPPerlinCausticsEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpperlin_caustics_effect_speed_scale") == 0) return gLGPPerlinCausticsEffectSpeedScale;
    if (strcmp(name, "lgpperlin_caustics_effect_output_gain") == 0) return gLGPPerlinCausticsEffectOutputGain;
    if (strcmp(name, "lgpperlin_caustics_effect_centre_bias") == 0) return gLGPPerlinCausticsEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPPerlinCausticsEffect

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

