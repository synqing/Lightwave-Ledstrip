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
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:LGPPerlinVeilEffect
namespace {
constexpr float kLGPPerlinVeilEffectSpeedScale = 1.0f;
constexpr float kLGPPerlinVeilEffectOutputGain = 1.0f;
constexpr float kLGPPerlinVeilEffectCentreBias = 1.0f;

float gLGPPerlinVeilEffectSpeedScale = kLGPPerlinVeilEffectSpeedScale;
float gLGPPerlinVeilEffectOutputGain = kLGPPerlinVeilEffectOutputGain;
float gLGPPerlinVeilEffectCentreBias = kLGPPerlinVeilEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPPerlinVeilEffectParameters[] = {
    {"lgpperlin_veil_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPPerlinVeilEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpperlin_veil_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPPerlinVeilEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpperlin_veil_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPPerlinVeilEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPPerlinVeilEffect

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPPerlinVeilEffect::LGPPerlinVeilEffect()
    : m_noiseX(0)
    , m_noiseY(0)
    , m_noiseZ(0)
    , m_momentum(0.0f)
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
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPPerlinVeilEffect
    gLGPPerlinVeilEffectSpeedScale = kLGPPerlinVeilEffectSpeedScale;
    gLGPPerlinVeilEffectOutputGain = kLGPPerlinVeilEffectOutputGain;
    gLGPPerlinVeilEffectCentreBias = kLGPPerlinVeilEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPPerlinVeilEffect

    m_noiseX = random16();
    m_noiseY = random16();
    m_noiseZ = random16();
    m_momentum = 0.0f;
    m_contrast = 0.5f;
    m_depthVariation = 0.0f;
    m_lastHopSeq = 0;
    m_targetRms = 0.0f;
    m_targetFlux = 0.0f;
    m_targetBeatStrength = 0.0f;
    m_targetBass = 0.0f;
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
    float dt = ctx.getSafeRawDeltaSeconds();
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

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
        
        // Smooth toward targets every frame (keeps motion alive between hops)
        float alpha = 1.0f - expf(-dt / 0.15f);  // True exponential, tau=150ms
        m_smoothRms += (m_targetRms - m_smoothRms) * alpha;
        m_smoothFlux += (m_targetFlux - m_smoothFlux) * alpha;
        m_smoothBeatStrength += (m_targetBeatStrength - m_smoothBeatStrength) * alpha;
        m_smoothBass += (m_targetBass - m_smoothBass) * alpha;
        
        // Flux/beatStrength → advection momentum (like Emotiscope's vu_level push)
        float audioPush = m_smoothFlux * 0.3f + m_smoothBeatStrength * 0.2f;
        audioPush = audioPush * audioPush * audioPush * audioPush; // ^4 for emphasis
        audioPush *= speedNorm * 0.1f;
        
        // Momentum decay and boost (like Emotiscope) - dt-corrected
        m_momentum *= powf(0.99f, dt * 60.0f);
        if (audioPush > m_momentum) {
            m_momentum = audioPush;
        }
        
        // RMS → contrast modulation (using smoothed value) - true exponential, tau=200ms
        float targetContrast = 0.3f + m_smoothRms * 0.7f;
        m_contrast += (targetContrast - m_contrast) * (1.0f - expf(-dt / 0.2f));
        
        // Bass → depth variation (using smoothed value)
        m_depthVariation = m_smoothBass * 0.5f;
    } else {
        // Ambient mode: slow decay (dt-corrected)
        m_momentum *= powf(0.98f, dt * 60.0f);
        m_contrast = 0.4f + 0.2f * sinf(ctx.totalTimeMs * 0.001f); // Slow breathing
        m_depthVariation = 0.0f;
        // Smooth audio parameters to zero when no audio (true exponential, tau=200ms)
        float alpha = 1.0f - expf(-dt / 0.2f);
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
    // No audio: ambient mode (dt-corrected)
    m_momentum *= powf(0.98f, dt * 60.0f);
    m_contrast = 0.4f + 0.2f * sinf(ctx.totalTimeMs * 0.001f);
    m_depthVariation = 0.0f;
#endif

    // =========================================================================
    // Noise Field Advection (centre-origin sampling)
    // =========================================================================
    // Base drift (slow wobble like Emotiscope), but keep coordinates in the
    // same "scale space" as existing working inoise8 usage (i*5, time>>3 etc.)
    float angle = ctx.totalTimeMs * 0.001f;
    float wobble = sinf(angle * 0.12f);

    // Keep coordinate deltas large enough to create visible structure.
    // (The previous >>8 coordinate collapse made the field near-constant.)
    uint16_t advX = (uint16_t)(20 + (uint16_t)(wobble * 12.0f) + (uint16_t)(m_momentum * 900.0f));
    uint16_t advY = (uint16_t)(18 + (uint16_t)(m_momentum * 1200.0f));
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

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        // Calculate distance from centre pair (0 at centre, 79 at edges)
        uint16_t dist = centerPairDistance(i);
        uint8_t dist8 = (uint8_t)dist;

        // Visible "veil" wants broad structures: use low spatial frequency,
        // but still enough delta across 0..79 to show gradients.
        uint16_t x1 = (uint16_t)(m_noiseX + dist * 28);
        uint16_t y1 = (uint16_t)(m_noiseY + (uint16_t)(dist8 << 2));
        uint16_t z1 = m_noiseZ;

        uint16_t x2 = (uint16_t)(m_noiseX + 10000u + dist * 46);
        uint16_t y2 = (uint16_t)(m_noiseY + 5000u + (uint16_t)(dist8 << 3));
        uint16_t z2 = (uint16_t)(m_noiseZ + 25000u);

        // Sample two independent 3D noise fields (time is implicit via m_noise*)
        uint8_t hueNoise = inoise8(x1, y1, z1);
        uint8_t lumNoise = inoise8(x2, y2, z2);

        // Palette index: avoid hue-wheel logic; palette selection defines colour language
        uint8_t paletteIndex = hueNoise + ctx.gHue;

        // Luminance shaping: contrast + centre emphasis (so it "reads" on LGP)
        float lumNorm = lumNoise / 255.0f; // 0..1
        lumNorm = lumNorm * lumNorm;       // bias darker, stronger highlights

        // Contrast: scale around mid-grey
        float c = 0.35f + 0.65f * m_contrast; // 0.35..1.0
        lumNorm = 0.5f + (lumNorm - 0.5f) * (0.7f + 0.6f * c);
        lumNorm = fmaxf(0.0f, fminf(1.0f, lumNorm));

        // Centre emphasis (strongest at centre, fades outward)
        float centreGain = 1.0f - (dist / 79.0f) * 0.35f; // 1.0 -> 0.65
        lumNorm *= centreGain;

        // Brightness range: keep a floor so it doesn't vanish at low noise values
        float brightnessNorm = 0.18f + lumNorm * 0.82f;
        uint8_t brightness = (uint8_t)(brightnessNorm * 255.0f * intensityNorm);

        CRGB color1 = ctx.palette.getColor(paletteIndex, brightness);
        ctx.leds[i] = color1;

        if (i + STRIP_LENGTH < ctx.ledCount) {
            // Second strip offset to encourage LGP interference without "rainbow sweeps"
            uint8_t paletteIndex2 = (uint8_t)(paletteIndex + 24);
            CRGB color2 = ctx.palette.getColor(paletteIndex2, brightness);
            ctx.leds[i + STRIP_LENGTH] = color2;
        }
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPPerlinVeilEffect
uint8_t LGPPerlinVeilEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPPerlinVeilEffectParameters) / sizeof(kLGPPerlinVeilEffectParameters[0]));
}

const plugins::EffectParameter* LGPPerlinVeilEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPPerlinVeilEffectParameters[index];
}

bool LGPPerlinVeilEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpperlin_veil_effect_speed_scale") == 0) {
        gLGPPerlinVeilEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpperlin_veil_effect_output_gain") == 0) {
        gLGPPerlinVeilEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpperlin_veil_effect_centre_bias") == 0) {
        gLGPPerlinVeilEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPPerlinVeilEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpperlin_veil_effect_speed_scale") == 0) return gLGPPerlinVeilEffectSpeedScale;
    if (strcmp(name, "lgpperlin_veil_effect_output_gain") == 0) return gLGPPerlinVeilEffectOutputGain;
    if (strcmp(name, "lgpperlin_veil_effect_centre_bias") == 0) return gLGPPerlinVeilEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPPerlinVeilEffect

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

