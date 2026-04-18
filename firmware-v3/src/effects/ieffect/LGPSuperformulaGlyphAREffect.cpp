/**
 * @file LGPSuperformulaGlyphAREffect.cpp
 * @brief Superformula Living Glyph (5-Layer AR) — REWRITTEN
 *
 * Morphing organic glyph using Superformula mathematics.
 * Parameters morph with audio. Brightness driven directly.
 * Centre-origin compliant. Dual-strip mirrored.
 */

#include "LGPSuperformulaGlyphAREffect.h"
#include "LGPFilmPost.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

static constexpr float kTwoPi = 6.28318530717958647692f;
static constexpr float kPi    = 3.14159265358979323846f;
static constexpr float kBassTau   = 0.050f;
static constexpr float kMidTau    = 0.055f;
static constexpr float kChromaTau = 0.300f;
static constexpr float kFollowerAttackTau = 0.058f;
static constexpr float kFollowerDecayTau  = 0.500f;
static constexpr float kFollowerFloor     = 0.04f;
static constexpr float kImpactDecayTau    = 0.180f;

static inline float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}
static inline float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

// Superformula: r(phi) = (|cos(m*phi/4)/a|^n2 + |sin(m*phi/4)/b|^n3)^(-1/n1)
static float evalSuperformula(float phi, float m, float n1, float n2, float n3) {
    float mPhi4 = m * phi * 0.25f;
    float cosT = fabsf(cosf(mPhi4));
    float sinT = fabsf(sinf(mPhi4));
    if (cosT < 1e-6f && sinT < 1e-6f) return 0.5f;
    float term1 = powf(cosT, n2);
    float term2 = powf(sinT, n3);
    float sum = term1 + term2;
    if (sum < 1e-6f) return 0.5f;
    return clampf(powf(sum, -1.0f / n1) * 0.35f, 0.0f, 1.0f);
}

LGPSuperformulaGlyphAREffect::LGPSuperformulaGlyphAREffect() = default;

bool LGPSuperformulaGlyphAREffect::init(plugins::EffectContext& ctx) {
    for (uint8_t zi = 0; zi < kMaxZones; ++zi) {
        m_t[zi] = 0.0f;
        m_bass[zi] = 0.0f;
        m_mid[zi] = 0.0f;
        m_chromaAngle[zi] = 0.0f;
        m_bassMax[zi] = 0.15f;
        m_midMax[zi] = 0.15f;
        m_impact[zi] = 0.0f;
        m_param_m[zi] = 6.0f;
        m_param_n1[zi] = 1.0f;
        m_param_n2[zi] = 1.5f;
        m_param_n3[zi] = 1.5f;
    }
    lightwaveos::effects::cinema::reset();
    return true;
}

void LGPSuperformulaGlyphAREffect::render(plugins::EffectContext& ctx) {
    const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;
    const float dt = ctx.getSafeRawDeltaSeconds();
    const float dtVis = ctx.getSafeDeltaSeconds();
    const float speedNorm = ctx.speed / 50.0f;

    const float rawBass = ctx.audio.available ? ctx.audio.bass() : 0.0f;
    const float rawMid = ctx.audio.available ? ctx.audio.mid() : 0.0f;
    const float beatStr = ctx.audio.available ? ctx.audio.beatStrength() : 0.0f;
    const float silScale = ctx.audio.available ? ctx.audio.silentScale() : 0.0f;
    const float* chroma = ctx.audio.available ? ctx.audio.chroma() : nullptr;

    m_bass[z] += (rawBass - m_bass[z]) * (1.0f - expf(-dt / kBassTau));
    m_mid[z] += (rawMid - m_mid[z]) * (1.0f - expf(-dt / kMidTau));

    if (chroma) {
        float sx = 0.0f, sy = 0.0f;
        for (int i = 0; i < 12; i++) {
            float angle = static_cast<float>(i) * (kTwoPi / 12.0f);
            sx += chroma[i] * cosf(angle);
            sy += chroma[i] * sinf(angle);
        }
        if (sx * sx + sy * sy > 0.0001f) {
            float target = atan2f(sy, sx);
            if (target < 0.0f) target += kTwoPi;
            float delta = target - m_chromaAngle[z];
            while (delta > kPi) delta -= kTwoPi;
            while (delta < -kPi) delta += kTwoPi;
            m_chromaAngle[z] += delta * (1.0f - expf(-dt / kChromaTau));
            if (m_chromaAngle[z] < 0.0f) m_chromaAngle[z] += kTwoPi;
            if (m_chromaAngle[z] >= kTwoPi) m_chromaAngle[z] -= kTwoPi;
        }
    }

    {
        float aA = 1.0f - expf(-dt / kFollowerAttackTau);
        float dA = 1.0f - expf(-dt / kFollowerDecayTau);
        if (m_bass[z] > m_bassMax[z]) m_bassMax[z] += (m_bass[z] - m_bassMax[z]) * aA;
        else m_bassMax[z] += (m_bass[z] - m_bassMax[z]) * dA;
        if (m_bassMax[z] < kFollowerFloor) m_bassMax[z] = kFollowerFloor;
        if (m_mid[z] > m_midMax[z]) m_midMax[z] += (m_mid[z] - m_midMax[z]) * aA;
        else m_midMax[z] += (m_mid[z] - m_midMax[z]) * dA;
        if (m_midMax[z] < kFollowerFloor) m_midMax[z] = kFollowerFloor;
    }
    const float normBass = clamp01(m_bass[z] / m_bassMax[z]);
    const float normMid = clamp01(m_mid[z] / m_midMax[z]);

    if (beatStr > m_impact[z]) m_impact[z] = beatStr;
    m_impact[z] *= expf(-dt / kImpactDecayTau);

    // Morph superformula params with normalised audio
    float paramAlpha = 1.0f - expf(-dt / 0.20f);
    float target_m = clampf(6.0f + 5.0f * normMid * (0.5f + 0.5f * sinf(m_t[z] * 0.3f)), 3.0f, 11.0f);
    m_param_m[z] += (target_m - m_param_m[z]) * paramAlpha;
    float target_n1 = clampf(1.0f + 0.6f * normBass * cosf(m_t[z] * 0.25f), 0.7f, 1.6f);
    m_param_n1[z] += (target_n1 - m_param_n1[z]) * paramAlpha;
    float target_n2 = clampf(1.5f + 0.9f * normBass, 0.8f, 2.4f);
    m_param_n2[z] += (target_n2 - m_param_n2[z]) * paramAlpha;
    float target_n3 = clampf(1.5f + 0.9f * normMid * sinf(m_t[z] * 0.4f), 0.8f, 2.4f);
    m_param_n3[z] += (target_n3 - m_param_n3[z]) * paramAlpha;

    const float beatMod = 0.3f + 0.7f * beatStr;

    float tRate = 0.8f + 3.5f * speedNorm;
    m_t[z] += tRate * dtVis;

    float glyphRotation = m_t[z] * 0.15f;
    float bandWidth = clampf(0.12f - 0.04f * normBass, 0.06f, 0.16f);

    uint8_t baseHue = static_cast<uint8_t>(m_chromaAngle[z] * (255.0f / kTwoPi)) + ctx.gHue;

    uint8_t fadeAmt = static_cast<uint8_t>(clampf(20.0f + 35.0f * (1.0f - normBass), 14.0f, 55.0f));
    fadeToBlackBy(ctx.leds, ctx.ledCount, fadeAmt);

    const float mid = static_cast<float>(HALF_LENGTH - 1);

    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        const float progress = (HALF_LENGTH <= 1) ? 0.0f
            : (static_cast<float>(dist) / mid);
        float dmid = static_cast<float>(dist);

        float phi = atan2f(dmid, 1.0f) + glyphRotation;
        float r_formula = evalSuperformula(phi, m_param_m[z], m_param_n1[z], m_param_n2[z], m_param_n3[z]);

        float distToCurve = fabsf(progress - r_formula);
        float bandWave = expf(-distToCurve / bandWidth);

        float breathing = 0.90f + 0.10f * cosf(kTwoPi * progress * 2.0f + m_t[z] * 0.8f);
        float impactAdd = m_impact[z] * bandWave * 0.35f;

        float brightness = (normBass * bandWave * breathing + impactAdd) * beatMod * silScale;
        brightness *= brightness;

        uint8_t val = static_cast<uint8_t>(clamp01(brightness) * 255.0f);
        val = scale8(val, ctx.brightness);

        uint8_t hue = baseHue + static_cast<uint8_t>(r_formula * 60.0f + progress * 20.0f);

        SET_CENTER_PAIR(ctx, dist, CHSV(hue, ctx.saturation, val));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

void LGPSuperformulaGlyphAREffect::cleanup() {}

const plugins::EffectMetadata& LGPSuperformulaGlyphAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Superformula Glyph (5L-AR)",
        "Living organic glyph with direct audio-reactive composition",
        plugins::EffectCategory::QUANTUM, 1
    };
    return meta;
}
uint8_t LGPSuperformulaGlyphAREffect::getParameterCount() const { return 0; }
const plugins::EffectParameter* LGPSuperformulaGlyphAREffect::getParameter(uint8_t) const { return nullptr; }
bool LGPSuperformulaGlyphAREffect::setParameter(const char*, float) { return false; }
float LGPSuperformulaGlyphAREffect::getParameter(const char*) const { return 0.0f; }

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
