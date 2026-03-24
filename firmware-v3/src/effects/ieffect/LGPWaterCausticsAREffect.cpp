/**
 * @file LGPWaterCausticsAREffect.cpp
 * @brief Water Caustics (5-Layer AR) — REWRITTEN
 *
 * Ray-envelope cusp filaments. Audio drives brightness directly.
 * Independent strip colours preserved. Snare trigger for cusp accent.
 */

#include "LGPWaterCausticsAREffect.h"
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
static constexpr float kSnareDecayTau     = 0.250f;

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

LGPWaterCausticsAREffect::LGPWaterCausticsAREffect()
    : m_t1(0.0f), m_t2(0.0f), m_bass(0.0f), m_mid(0.0f), m_chromaAngle(0.0f),
      m_bassMax(0.15f), m_midMax(0.15f), m_impact(0.0f), m_snareImpact(0.0f) {}

bool LGPWaterCausticsAREffect::init(plugins::EffectContext& ctx) {
    m_t1 = 0.0f; m_t2 = 0.0f; m_bass = 0.0f; m_mid = 0.0f; m_chromaAngle = 0.0f;
    m_bassMax = 0.15f; m_midMax = 0.15f; m_impact = 0.0f; m_snareImpact = 0.0f;
    return true;
}

void LGPWaterCausticsAREffect::render(plugins::EffectContext& ctx) {
    const float dt = ctx.getSafeRawDeltaSeconds();
    const float dtVis = ctx.getSafeDeltaSeconds();
    const float speedNorm = ctx.speed / 50.0f;

    // STEP 1: Direct ControlBus reads
    const float rawBass = ctx.audio.available ? ctx.audio.bass() : 0.0f;
    const float rawMid = ctx.audio.available ? ctx.audio.mid() : 0.0f;
    const float beatStr = ctx.audio.available ? ctx.audio.beatStrength() : 0.0f;
    const bool snareHit = ctx.audio.available && ctx.audio.isSnareHit();
    const float silScale = ctx.audio.available ? ctx.audio.silentScale() : 0.0f;
    const float* chroma = ctx.audio.available ? ctx.audio.chroma() : nullptr;

    // STEP 2: Single-stage smoothing
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
    if (snareHit) m_snareImpact = fmaxf(m_snareImpact, 0.75f);
    m_snareImpact *= expf(-dt / kSnareDecayTau);

    // STEP 5: Caustic visual parameters
    const float beatMod = 0.3f + 0.7f * beatStr;

    // Refractive coefficients driven by normalised mid
    const float A  = 0.75f + 0.30f * normMid;
    const float B  = 0.35f + 0.22f * normMid;
    const float k1 = 0.18f * (1.0f + 0.25f * normMid);
    const float k2 = 0.41f * (1.0f + 0.22f * normMid);

    // Two time accumulators
    float t1Rate = (1.3f + 7.0f * speedNorm) * (0.7f + 0.4f * normBass);
    float t2Rate = (0.8f + 4.8f * speedNorm) * (0.7f + 0.4f * normBass);
    m_t1 += t1Rate * dtVis;
    m_t2 += t2Rate * dtVis;

    uint8_t baseHue = static_cast<uint8_t>(m_chromaAngle * (255.0f / kTwoPi)) + ctx.gHue;

    // Trail persistence
    uint8_t fadeAmt = static_cast<uint8_t>(clampf(16.0f + 30.0f * (1.0f - normBass), 10.0f, 50.0f));
    fadeToBlackBy(ctx.leds, ctx.ledCount, fadeAmt);

    // STEP 6: Per-pixel render (linear — independent strip colours)
    for (int i = 0; i < STRIP_LENGTH; i++) {
        float dist = static_cast<float>(centerPairDistance(static_cast<uint16_t>(i)));
        float distN = dist / static_cast<float>(HALF_LENGTH);

        // Ray-envelope cusp filaments
        float y = dist + A * sinf(dist * k1 + m_t1) + B * sinf(dist * k2 - m_t2 * 1.3f);
        float dydx = 1.0f + A * k1 * cosf(dist * k1 + m_t1) + B * k2 * cosf(dist * k2 - m_t2 * 1.3f);

        float density = 1.0f / (0.20f + fabsf(dydx));
        density = clamp01(density * 0.85f);

        float sheet = 0.5f + 0.5f * sinf(y * 0.22f + m_t2);

        // Cusp detection
        float cusp = expf(-fabsf(density - 0.90f) * 14.0f);

        // Compose brightness: audio x geometry x beat x silence
        float structuredDensity = 0.72f * density + 0.28f * sheet;
        float impactAdd = cusp * m_impact * 0.40f + cusp * m_snareImpact * 0.25f;
        float brightness = (normBass * structuredDensity + impactAdd) * beatMod * silScale;
        brightness *= brightness;  // Squared for punch

        uint8_t val = static_cast<uint8_t>(clamp01(brightness) * 255.0f);
        val = scale8(val, ctx.brightness);

        // Strip A
        uint8_t hueA = baseHue + static_cast<uint8_t>(distN * 20.0f + density * 32.0f);
        ctx.leds[i] = CHSV(hueA, ctx.saturation, val);

        // Strip B: offset hue and brightness for depth
        int j = i + STRIP_LENGTH;
        if (j < static_cast<int>(ctx.ledCount)) {
            uint8_t hueB = hueA + 4;
            uint8_t valB = scale8(val, 242);
            ctx.leds[j] = CHSV(hueB, ctx.saturation, valB);
        }
    }
}

void LGPWaterCausticsAREffect::cleanup() {}

const plugins::EffectMetadata& LGPWaterCausticsAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Water Caustics (5L-AR)",
        "Ray-envelope caustic filaments with direct audio-reactive composition",
        plugins::EffectCategory::QUANTUM, 1
    };
    return meta;
}
uint8_t LGPWaterCausticsAREffect::getParameterCount() const { return 0; }
const plugins::EffectParameter* LGPWaterCausticsAREffect::getParameter(uint8_t) const { return nullptr; }
bool LGPWaterCausticsAREffect::setParameter(const char*, float) { return false; }
float LGPWaterCausticsAREffect::getParameter(const char*) const { return 0.0f; }

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
