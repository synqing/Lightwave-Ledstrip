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

LGPSuperformulaGlyphAREffect::LGPSuperformulaGlyphAREffect()
    : m_t(0.0f), m_bass(0.0f), m_mid(0.0f), m_chromaAngle(0.0f),
      m_bassMax(0.15f), m_midMax(0.15f), m_impact(0.0f),
      m_param_m(6.0f), m_param_n1(1.0f), m_param_n2(1.5f), m_param_n3(1.5f) {}

bool LGPSuperformulaGlyphAREffect::init(plugins::EffectContext& ctx) {
    m_t = 0.0f; m_bass = 0.0f; m_mid = 0.0f; m_chromaAngle = 0.0f;
    m_bassMax = 0.15f; m_midMax = 0.15f; m_impact = 0.0f;
    m_param_m = 6.0f; m_param_n1 = 1.0f; m_param_n2 = 1.5f; m_param_n3 = 1.5f;
    lightwaveos::effects::cinema::reset();
    return true;
}

void LGPSuperformulaGlyphAREffect::render(plugins::EffectContext& ctx) {
    const float dt = ctx.getSafeRawDeltaSeconds();
    const float dtVis = ctx.getSafeDeltaSeconds();
    const float speedNorm = ctx.speed / 50.0f;

    const float rawBass = ctx.audio.available ? ctx.audio.bass() : 0.0f;
    const float rawMid = ctx.audio.available ? ctx.audio.mid() : 0.0f;
    const float beatStr = ctx.audio.available ? ctx.audio.beatStrength() : 0.0f;
    const float silScale = ctx.audio.available ? ctx.audio.silentScale() : 0.0f;
    const float* chroma = ctx.audio.available ? ctx.audio.chroma() : nullptr;

    m_bass += (rawBass - m_bass) * (1.0f - expf(-dt / kBassTau));
    m_mid += (rawMid - m_mid) * (1.0f - expf(-dt / kMidTau));

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
            float delta = target - m_chromaAngle;
            while (delta > kPi) delta -= kTwoPi;
            while (delta < -kPi) delta += kTwoPi;
            m_chromaAngle += delta * (1.0f - expf(-dt / kChromaTau));
            if (m_chromaAngle < 0.0f) m_chromaAngle += kTwoPi;
            if (m_chromaAngle >= kTwoPi) m_chromaAngle -= kTwoPi;
        }
    }

    {
        float aA = 1.0f - expf(-dt / kFollowerAttackTau);
        float dA = 1.0f - expf(-dt / kFollowerDecayTau);
        if (m_bass > m_bassMax) m_bassMax += (m_bass - m_bassMax) * aA;
        else m_bassMax += (m_bass - m_bassMax) * dA;
        if (m_bassMax < kFollowerFloor) m_bassMax = kFollowerFloor;
        if (m_mid > m_midMax) m_midMax += (m_mid - m_midMax) * aA;
        else m_midMax += (m_mid - m_midMax) * dA;
        if (m_midMax < kFollowerFloor) m_midMax = kFollowerFloor;
    }
    const float normBass = clamp01(m_bass / m_bassMax);
    const float normMid = clamp01(m_mid / m_midMax);

    if (beatStr > m_impact) m_impact = beatStr;
    m_impact *= expf(-dt / kImpactDecayTau);

    // Morph superformula params with normalised audio
    float paramAlpha = 1.0f - expf(-dt / 0.20f);
    float target_m = clampf(6.0f + 5.0f * normMid * (0.5f + 0.5f * sinf(m_t * 0.3f)), 3.0f, 11.0f);
    m_param_m += (target_m - m_param_m) * paramAlpha;
    float target_n1 = clampf(1.0f + 0.6f * normBass * cosf(m_t * 0.25f), 0.7f, 1.6f);
    m_param_n1 += (target_n1 - m_param_n1) * paramAlpha;
    float target_n2 = clampf(1.5f + 0.9f * normBass, 0.8f, 2.4f);
    m_param_n2 += (target_n2 - m_param_n2) * paramAlpha;
    float target_n3 = clampf(1.5f + 0.9f * normMid * sinf(m_t * 0.4f), 0.8f, 2.4f);
    m_param_n3 += (target_n3 - m_param_n3) * paramAlpha;

    const float beatMod = 0.3f + 0.7f * beatStr;

    float tRate = 0.8f + 3.5f * speedNorm;
    m_t += tRate * dtVis;

    float glyphRotation = m_t * 0.15f;
    float bandWidth = clampf(0.12f - 0.04f * normBass, 0.06f, 0.16f);

    uint8_t baseHue = static_cast<uint8_t>(m_chromaAngle * (255.0f / kTwoPi)) + ctx.gHue;

    uint8_t fadeAmt = static_cast<uint8_t>(clampf(20.0f + 35.0f * (1.0f - normBass), 14.0f, 55.0f));
    fadeToBlackBy(ctx.leds, ctx.ledCount, fadeAmt);

    const float mid = static_cast<float>(HALF_LENGTH - 1);

    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        const float progress = (HALF_LENGTH <= 1) ? 0.0f
            : (static_cast<float>(dist) / mid);
        float dmid = static_cast<float>(dist);

        float phi = atan2f(dmid, 1.0f) + glyphRotation;
        float r_formula = evalSuperformula(phi, m_param_m, m_param_n1, m_param_n2, m_param_n3);

        float distToCurve = fabsf(progress - r_formula);
        float bandWave = expf(-distToCurve / bandWidth);

        float breathing = 0.90f + 0.10f * cosf(kTwoPi * progress * 2.0f + m_t * 0.8f);
        float impactAdd = m_impact * bandWave * 0.35f;

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
