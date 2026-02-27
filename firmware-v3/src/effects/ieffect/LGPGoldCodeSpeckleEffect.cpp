/**
 * @file LGPGoldCodeSpeckleEffect.cpp
 * @brief LGP Gold-Code Speckle Morph - Phase-plate holographic grain illusion
 *
 * Two 16-bit LFSRs with different seeds produce deterministic pseudo-random
 * bit patterns that index into centre-origin distance to create symmetric
 * holographic speckle. A slow sinusoidal crossfade morphs between the two
 * codes, giving the impression of a rotating phase plate behind the LGP.
 *
 * LFSR polynomial: x^16 + x^14 + x^13 + x^11 + 1
 *   feedback = bit0 ^ bit2 ^ bit3 ^ bit5
 */

#include "LGPGoldCodeSpeckleEffect.h"
#include "ChromaUtils.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:LGPGoldCodeSpeckleEffect
namespace {
constexpr float kLGPGoldCodeSpeckleEffectSpeedScale = 1.0f;
constexpr float kLGPGoldCodeSpeckleEffectOutputGain = 1.0f;
constexpr float kLGPGoldCodeSpeckleEffectCentreBias = 1.0f;

float gLGPGoldCodeSpeckleEffectSpeedScale = kLGPGoldCodeSpeckleEffectSpeedScale;
float gLGPGoldCodeSpeckleEffectOutputGain = kLGPGoldCodeSpeckleEffectOutputGain;
float gLGPGoldCodeSpeckleEffectCentreBias = kLGPGoldCodeSpeckleEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPGoldCodeSpeckleEffectParameters[] = {
    {"lgpgold_code_speckle_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPGoldCodeSpeckleEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpgold_code_speckle_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPGoldCodeSpeckleEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpgold_code_speckle_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPGoldCodeSpeckleEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPGoldCodeSpeckleEffect

namespace lightwaveos {
namespace effects {
namespace ieffect {

// ---------------------------------------------------------------------------
// LFSR helper
// ---------------------------------------------------------------------------

/**
 * @brief Advance a 16-bit LFSR by one step.
 *
 * Polynomial: x^16 + x^14 + x^13 + x^11 + 1
 * Taps at bit positions 0, 2, 3, 5 (counting from LSB).
 */
static inline uint16_t lfsrStep(uint16_t state) {
    uint16_t feedback = ((state >> 0) ^ (state >> 2) ^ (state >> 3) ^ (state >> 5)) & 1u;
    return (state >> 1) | (feedback << 15);
}

/**
 * @brief Advance LFSR by N steps (used for beat-triggered jump).
 */
static inline uint16_t lfsrAdvance(uint16_t state, uint8_t steps) {
    for (uint8_t s = 0; s < steps; ++s) {
        state = lfsrStep(state);
    }
    return state;
}

// ---------------------------------------------------------------------------
// Construction / lifecycle
// ---------------------------------------------------------------------------

LGPGoldCodeSpeckleEffect::LGPGoldCodeSpeckleEffect()
    : m_lfsrA(0xACE1u)
    , m_lfsrB(0xBEEFu)
    , m_lfsrTimer(0.0f)
    , m_mixPhase(0.0f)
    , m_chromaAngle(0.0f)
    , m_timeOffset(0.0f)
    , m_fallbackPhase(0.0f)
#if FEATURE_AUDIO_SYNC
    , m_lastHopSeq(0)
    , m_chromaSmoothed{0}
#endif
{
}

bool LGPGoldCodeSpeckleEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPGoldCodeSpeckleEffect
    gLGPGoldCodeSpeckleEffectSpeedScale = kLGPGoldCodeSpeckleEffectSpeedScale;
    gLGPGoldCodeSpeckleEffectOutputGain = kLGPGoldCodeSpeckleEffectOutputGain;
    gLGPGoldCodeSpeckleEffectCentreBias = kLGPGoldCodeSpeckleEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPGoldCodeSpeckleEffect

    m_lfsrA        = 0xACE1u;
    m_lfsrB        = 0xBEEFu;
    m_lfsrTimer    = 0.0f;
    m_mixPhase     = 0.0f;
    m_chromaAngle  = 0.0f;
    m_timeOffset   = 0.0f;
    m_fallbackPhase = 0.0f;
#if FEATURE_AUDIO_SYNC
    m_lastHopSeq = 0;
    for (uint8_t i = 0; i < 12; ++i) {
        m_chromaSmoothed[i] = 0.0f;
    }
#endif
    return true;
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

void LGPGoldCodeSpeckleEffect::render(plugins::EffectContext& ctx) {
    // =====================================================================
    // Safe delta time
    // =====================================================================
    const float rawDt    = ctx.getSafeRawDeltaSeconds();
    const float dt       = ctx.getSafeDeltaSeconds();
    const float speedNorm = ctx.speed / 50.0f;

    // =====================================================================
    // LFSR stepping (~45 Hz, every ~22 ms of accumulated time)
    // =====================================================================
    const float lfsrInterval = 0.022f;  // ~45 Hz
    m_lfsrTimer += rawDt;
    while (m_lfsrTimer >= lfsrInterval) {
        m_lfsrTimer -= lfsrInterval;
        m_lfsrA = lfsrStep(m_lfsrA);
        m_lfsrB = lfsrStep(m_lfsrB);
    }

    // =====================================================================
    // Crossfade phase: full cycle ~10 s, speed-scaled
    // =====================================================================
    const float mixRate = 0.6283f * speedNorm;  // 2*PI / 10s * speedNorm
    m_mixPhase += mixRate * dt;
    if (m_mixPhase > 6.2831853f) m_mixPhase -= 6.2831853f;

    // mix: 0..255
    const float mixF = sinf(m_mixPhase) * 0.5f + 0.5f;
    const uint8_t mix = (uint8_t)(mixF * 255.0f);

    // =====================================================================
    // Carrier wave time offset (centre-origin travelling wave)
    // =====================================================================
    m_timeOffset += (1.0f + speedNorm) * dt * 40.0f;
    if (m_timeOffset > 65535.0f) m_timeOffset -= 65535.0f;

    // =====================================================================
    // Audio: chroma hue + beat LFSR jump
    // =====================================================================
    uint8_t chromaHueOffset = 0;

#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        // Update chroma targets on new hops
        bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;
            for (uint8_t i = 0; i < 12; ++i) {
                // Smooth toward heavy_chroma using simple exponential approach
                float target = ctx.audio.controlBus.heavy_chroma[i];
                m_chromaSmoothed[i] += (target - m_chromaSmoothed[i]) * 0.3f;
            }
        }

        // Circular weighted mean + circular EMA for smooth, continuous hue
        chromaHueOffset = effects::chroma::circularChromaHueSmoothed(
            m_chromaSmoothed, m_chromaAngle, rawDt, 0.20f);

        // Beat-triggered LFSR jump: advance by 8 steps for sudden speckle shift
        if (ctx.audio.isOnBeat()) {
            m_lfsrA = lfsrAdvance(m_lfsrA, 8);
            m_lfsrB = lfsrAdvance(m_lfsrB, 8);
        }
    } else {
        // No audio: slow fallback animation
        m_fallbackPhase += speedNorm * 0.3f * dt;
        if (m_fallbackPhase > 6.2831853f * 10.0f) {
            m_fallbackPhase -= 6.2831853f * 10.0f;
        }
    }
#else
    // No audio feature: slow fallback animation
    m_fallbackPhase += speedNorm * 0.3f * dt;
    if (m_fallbackPhase > 6.2831853f * 10.0f) {
        m_fallbackPhase -= 6.2831853f * 10.0f;
    }
#endif

    // =====================================================================
    // Fade for persistence / trails
    // =====================================================================
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    // =====================================================================
    // Snapshot LFSR state (read-only in pixel loop)
    // =====================================================================
    const uint16_t lfsrA = m_lfsrA;
    const uint16_t lfsrB = m_lfsrB;
    const uint8_t timeOff8 = (uint8_t)((int)m_timeOffset & 0xFF);

    // =====================================================================
    // RENDER LOOP -- per-strip, centre-origin
    // =====================================================================
    for (uint16_t i = 0; i < STRIP_LENGTH; ++i) {
        const uint16_t d = centerPairDistance(i);

        // ---------------------------------------------------------------
        // Extract bits from each LFSR indexed by distance (symmetric key)
        // Using different offsets for code B to decorrelate the two codes
        // ---------------------------------------------------------------
        const uint8_t bitIdxA = (d) & 15u;
        const uint8_t bitIdxB = (d + 7u) & 15u;

        const uint8_t bitA = (lfsrA >> bitIdxA) & 1u;
        const uint8_t bitB = (lfsrB >> bitIdxB) & 1u;

        // Convert to signed phase: 0 -> -60, 1 -> +60
        const int8_t phaseA = bitA ? 60 : -60;
        const int8_t phaseB = bitB ? 60 : -60;

        // Crossfade between code A and code B
        // p = (phaseA * (255 - mix) + phaseB * mix) / 255
        const int16_t p = ((int16_t)phaseA * (255 - mix) + (int16_t)phaseB * mix) / 255;

        // ---------------------------------------------------------------
        // Carrier wave + phase modulation
        // sin8 input: d * 17 gives ~15-LED wavelength, plus time offset
        // ---------------------------------------------------------------
        const uint8_t carrier = sin8((uint8_t)(d * 17 + timeOff8));

        // Add phase modulation to carrier
        int16_t v16 = (int16_t)carrier + p;
        if (v16 < 0) v16 = 0;
        if (v16 > 255) v16 = 255;
        uint8_t v = (uint8_t)v16;

        // ---------------------------------------------------------------
        // Contrast curve: square response for speckle grain
        // scale8(v, v) ~= v*v/256
        // ---------------------------------------------------------------
        v = scale8(v, v);

        // ---------------------------------------------------------------
        // Specular sparkle where v > 245
        // ---------------------------------------------------------------
        const bool sparkle = (v > 245);
        if (sparkle) {
            v = 255;
        }

        // ---------------------------------------------------------------
        // Colour: strip 1 teal-ish, strip 2 offset +25 hue
        // Max hue shift ~40 units (no rainbow sweeps)
        // ---------------------------------------------------------------
        const uint8_t baseHue = ctx.gHue + chromaHueOffset;

        // Strip 1
        {
            CRGB colour;
            if (sparkle) {
                // Specular highlight: near-white with slight palette tint
                colour = ctx.palette.getColor(baseHue, 255);
                colour += CRGB(60, 60, 60);  // Lift toward white
            } else {
                colour = ctx.palette.getColor(baseHue, v);
            }
            // Blend onto existing (fadeToBlackBy provides persistence)
            ctx.leds[i] |= colour;
        }

        // Strip 2 (hue offset +25)
        {
            const uint16_t j = i + STRIP_LENGTH;
            if (j < ctx.ledCount) {
                const uint8_t hue2 = baseHue + 25;
                CRGB colour2;
                if (sparkle) {
                    colour2 = ctx.palette.getColor(hue2, 255);
                    colour2 += CRGB(60, 60, 60);
                } else {
                    colour2 = ctx.palette.getColor(hue2, v);
                }
                ctx.leds[j] |= colour2;
            }
        }
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPGoldCodeSpeckleEffect
uint8_t LGPGoldCodeSpeckleEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPGoldCodeSpeckleEffectParameters) / sizeof(kLGPGoldCodeSpeckleEffectParameters[0]));
}

const plugins::EffectParameter* LGPGoldCodeSpeckleEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPGoldCodeSpeckleEffectParameters[index];
}

bool LGPGoldCodeSpeckleEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpgold_code_speckle_effect_speed_scale") == 0) {
        gLGPGoldCodeSpeckleEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpgold_code_speckle_effect_output_gain") == 0) {
        gLGPGoldCodeSpeckleEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpgold_code_speckle_effect_centre_bias") == 0) {
        gLGPGoldCodeSpeckleEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPGoldCodeSpeckleEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpgold_code_speckle_effect_speed_scale") == 0) return gLGPGoldCodeSpeckleEffectSpeedScale;
    if (strcmp(name, "lgpgold_code_speckle_effect_output_gain") == 0) return gLGPGoldCodeSpeckleEffectOutputGain;
    if (strcmp(name, "lgpgold_code_speckle_effect_centre_bias") == 0) return gLGPGoldCodeSpeckleEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPGoldCodeSpeckleEffect

void LGPGoldCodeSpeckleEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPGoldCodeSpeckleEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Gold-Code Speckle",
        "Holographic grain drift from LFSR phase-plate crossfade",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
