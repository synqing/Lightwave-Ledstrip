/**
 * @file LGPMoireCathedralAREffect.cpp
 * @brief Moire Cathedral (5-Layer AR) — REWRITTEN
 *
 * Two close-pitch gratings creating interference arches.
 * Audio drives brightness directly. Centre-origin, dual-strip mirrored.
 */

#include "LGPMoireCathedralAREffect.h"
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
static constexpr float kImpactDecayTau    = 0.200f;

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

LGPMoireCathedralAREffect::LGPMoireCathedralAREffect()
    : m_t(0.0f), m_bass(0.0f), m_mid(0.0f), m_chromaAngle(0.0f),
      m_bassMax(0.15f), m_midMax(0.15f), m_impact(0.0f) {}

bool LGPMoireCathedralAREffect::init(plugins::EffectContext& ctx) {
    m_t = 0.0f; m_bass = 0.0f; m_mid = 0.0f; m_chromaAngle = 0.0f;
    m_bassMax = 0.15f; m_midMax = 0.15f; m_impact = 0.0f;
    lightwaveos::effects::cinema::reset();
    return true;
}

void LGPMoireCathedralAREffect::render(plugins::EffectContext& ctx) {
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

    const float beatMod = 0.3f + 0.7f * beatStr;

    // Grating pitches: mid energy shifts pitch
    float p1 = clampf(7.5f + 3.0f * (normMid - 0.5f), 6.0f, 9.0f);
    float p2 = p1 + 0.6f;

    float w1 = 0.65f + 0.40f * speedNorm;
    float w2 = 0.58f + 0.35f * speedNorm;

    float tRate = 0.85f + 3.5f * speedNorm;
    m_t += tRate * dtVis;

    // Rib sharpening: bass sharpens, impact boosts
    float ribPow = clampf(1.35f + 0.45f * normBass + 0.3f * m_impact, 1.30f, 2.50f);

    uint8_t baseHue = static_cast<uint8_t>(m_chromaAngle * (255.0f / kTwoPi)) + ctx.gHue;

    uint8_t fadeAmt = static_cast<uint8_t>(clampf(18.0f + 35.0f * (1.0f - normBass), 12.0f, 55.0f));
    fadeToBlackBy(ctx.leds, ctx.ledCount, fadeAmt);

    // Per-pixel render — centre-outward, 4-way symmetric
    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        const float progress = (HALF_LENGTH <= 1) ? 0.0f
            : (static_cast<float>(dist) / static_cast<float>(HALF_LENGTH - 1));
        const float x = static_cast<float>(dist);

        // Two grating waves
        float g1 = sinf(kTwoPi * (x / p1) + m_t * w1);
        float g2 = sinf(kTwoPi * (x / p2) - m_t * w2);

        // Moire interference
        float moire = fabsf(g1 - g2);
        float wave = clamp01(moire * 0.55f);
        wave = powf(wave, ribPow);

        // Impact at arch peaks
        float impactAdd = m_impact * wave * 0.35f;

        // Compose: audio x geometry x beat x silence
        float brightness = (normBass * wave + impactAdd) * beatMod * silScale;
        brightness *= brightness;

        uint8_t val = static_cast<uint8_t>(clamp01(brightness) * 255.0f);
        val = scale8(val, ctx.brightness);

        uint8_t hue = baseHue + static_cast<uint8_t>(wave * 42.0f + progress * 12.0f);

        SET_CENTER_PAIR(ctx, dist, CHSV(hue, ctx.saturation, val));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

void LGPMoireCathedralAREffect::cleanup() {}

const plugins::EffectMetadata& LGPMoireCathedralAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Moire Cathedral (5L-AR)",
        "Interference arches with direct audio-reactive composition",
        plugins::EffectCategory::QUANTUM, 1
    };
    return meta;
}
uint8_t LGPMoireCathedralAREffect::getParameterCount() const { return 0; }
const plugins::EffectParameter* LGPMoireCathedralAREffect::getParameter(uint8_t) const { return nullptr; }
bool LGPMoireCathedralAREffect::setParameter(const char*, float) { return false; }
float LGPMoireCathedralAREffect::getParameter(const char*) const { return 0.0f; }

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
