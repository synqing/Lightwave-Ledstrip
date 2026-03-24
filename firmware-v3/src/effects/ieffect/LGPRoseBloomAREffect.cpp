/**
 * @file LGPRoseBloomAREffect.cpp
 * @brief Rose Bloom (5-Layer Audio-Reactive) — REWRITTEN
 *
 * Rhodonea curve blooming petals. Petal count driven by mid-frequency
 * audio content. Audio drives brightness directly.
 *
 * Divergence fixes: Direct ControlBus reads, single-stage smoothing,
 * asymmetric max follower, no brightness floors, SET_CENTER_PAIR.
 *
 * Centre-origin compliant. Dual-strip mirrored.
 */

#include "LGPRoseBloomAREffect.h"
#include "LGPFilmPost.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

static constexpr float kTwoPi = 6.28318530717958647692f;
static constexpr float kPi    = 3.14159265358979323846f;
static constexpr float kBassTau    = 0.050f;
static constexpr float kMidTau     = 0.055f;
static constexpr float kChromaTau  = 0.300f;
static constexpr float kFollowerAttackTau = 0.058f;
static constexpr float kFollowerDecayTau  = 0.500f;
static constexpr float kFollowerFloor     = 0.04f;
static constexpr float kImpactDecayTau = 0.180f;

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

LGPRoseBloomAREffect::LGPRoseBloomAREffect()
    : m_t(0.0f), m_bass(0.0f), m_mid(0.0f), m_chromaAngle(0.0f),
      m_bassMax(0.15f), m_midMax(0.15f), m_petalK(5.0f), m_impact(0.0f) {}

bool LGPRoseBloomAREffect::init(plugins::EffectContext& ctx) {
    m_t = 0.0f; m_bass = 0.0f; m_mid = 0.0f; m_chromaAngle = 0.0f;
    m_bassMax = 0.15f; m_midMax = 0.15f; m_petalK = 5.0f; m_impact = 0.0f;
    lightwaveos::effects::cinema::reset();
    return true;
}

void LGPRoseBloomAREffect::render(plugins::EffectContext& ctx) {
    const float dt = ctx.getSafeRawDeltaSeconds();
    const float dtVis = ctx.getSafeDeltaSeconds();
    const float speedNorm = ctx.speed / 50.0f;

    // STEP 1: Direct ControlBus reads
    const float rawBass = ctx.audio.available ? ctx.audio.bass() : 0.0f;
    const float rawMid = ctx.audio.available ? ctx.audio.mid() : 0.0f;
    const float beatStr = ctx.audio.available ? ctx.audio.beatStrength() : 0.0f;
    const float silScale = ctx.audio.available ? ctx.audio.silentScale() : 0.0f;
    const float* chroma = ctx.audio.available ? ctx.audio.chroma() : nullptr;

    // STEP 2: Single-stage smoothing
    m_bass += (rawBass - m_bass) * (1.0f - expf(-dt / kBassTau));
    m_mid += (rawMid - m_mid) * (1.0f - expf(-dt / kMidTau));

    // Circular chroma EMA
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

    // STEP 3: Max followers
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

    // STEP 4: Impact
    if (beatStr > m_impact) m_impact = beatStr;
    m_impact *= expf(-dt / kImpactDecayTau);

    // STEP 5: Rose visual parameters
    const float beatMod = 0.3f + 0.7f * beatStr;

    // Petal count driven by mid energy (3-7 petals)
    float kfTarget = 3.0f + 4.0f * normMid;
    float petalAlpha = 1.0f - expf(-dt / 0.25f);
    m_petalK += (kfTarget - m_petalK) * petalAlpha;
    m_petalK = clampf(m_petalK, 3.0f, 7.0f);

    // Motion
    float tRate = 0.3f + 1.8f * speedNorm;
    m_t += tRate * dtVis;

    // Bloom modulation: opening/closing
    float bloomMod = 0.55f + 0.45f * sinf(m_t * 0.35f);

    // Band width: tighter with impact
    float bandWidth = clampf(0.14f - 0.04f * m_impact, 0.08f, 0.18f);

    // Hue from chroma
    uint8_t baseHue = static_cast<uint8_t>(m_chromaAngle * (255.0f / kTwoPi)) + ctx.gHue;

    // Trail persistence
    uint8_t fadeAmt = static_cast<uint8_t>(clampf(20.0f + 35.0f * (1.0f - normBass), 14.0f, 55.0f));
    fadeToBlackBy(ctx.leds, ctx.ledCount, fadeAmt);

    // STEP 6: Per-pixel render — centre-outward, 4-way symmetric
    const float mid = static_cast<float>(HALF_LENGTH - 1);

    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        const float progress = (HALF_LENGTH <= 1) ? 0.0f
            : (static_cast<float>(dist) / mid);

        // 1D rhodonea: distance from LED to curve position
        // On 1D strip, theta is either 0 or pi. Use progress as radial position.
        float r = static_cast<float>(dist);
        float rCurve = fabsf(cosf(m_petalK * kPi * progress)) * mid * bloomMod;
        float distToCurve = fabsf(r - rCurve) / mid;

        // Gaussian band around curve
        float band = expf(-distToCurve * distToCurve / (bandWidth * bandWidth));

        // Breathing
        float breathing = 0.90f + 0.10f * cosf(kTwoPi * (progress + m_t * 0.25f));

        // Impact flash at petal edges
        float impactAdd = m_impact * band * 0.35f;

        // Compose brightness: audio x geometry x beat x silence
        float brightness = (normBass * band * breathing + impactAdd) * beatMod * silScale;

        // Squared for punch
        brightness *= brightness;

        uint8_t val = static_cast<uint8_t>(clamp01(brightness) * 255.0f);
        val = scale8(val, ctx.brightness);

        // Hue: chroma base + band offset
        uint8_t hue = baseHue + static_cast<uint8_t>(band * 45.0f + progress * 20.0f);

        SET_CENTER_PAIR(ctx, dist, CHSV(hue, ctx.saturation, val));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

void LGPRoseBloomAREffect::cleanup() {}

const plugins::EffectMetadata& LGPRoseBloomAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Rose Bloom (5L-AR)",
        "Rhodonea curve blooming petals with direct audio-reactive composition",
        plugins::EffectCategory::QUANTUM, 1
    };
    return meta;
}
uint8_t LGPRoseBloomAREffect::getParameterCount() const { return 0; }
const plugins::EffectParameter* LGPRoseBloomAREffect::getParameter(uint8_t) const { return nullptr; }
bool LGPRoseBloomAREffect::setParameter(const char*, float) { return false; }
float LGPRoseBloomAREffect::getParameter(const char*) const { return 0.0f; }

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
