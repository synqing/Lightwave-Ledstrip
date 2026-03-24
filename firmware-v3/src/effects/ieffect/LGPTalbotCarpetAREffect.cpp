/**
 * @file LGPTalbotCarpetAREffect.cpp
 * @brief Talbot Carpet (5-Layer AR) — REWRITTEN
 *
 * Fresnel self-imaging lattice with harmonic sum. Audio drives brightness directly.
 * Heavy inner loop — uses LIGHTWEIGHT cinema preset.
 */

#include "LGPTalbotCarpetAREffect.h"
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
static constexpr float kImpactDecayTau    = 0.220f;

static constexpr uint8_t kMaxHarmonics = 7;
static constexpr float kHarmonicOrder[kMaxHarmonics] = {1,2,3,4,5,6,7};
static constexpr float kHarmonicInv[kMaxHarmonics] = {1.0f,0.5f,0.3333333f,0.25f,0.2f,0.1666667f,0.14285715f};

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
static inline float fract(float x) { return x - floorf(x); }

LGPTalbotCarpetAREffect::LGPTalbotCarpetAREffect()
    : m_t(0.0f), m_bass(0.0f), m_mid(0.0f), m_chromaAngle(0.0f),
      m_bassMax(0.15f), m_midMax(0.15f), m_impact(0.0f) {}

bool LGPTalbotCarpetAREffect::init(plugins::EffectContext& ctx) {
    m_t = 0.0f; m_bass = 0.0f; m_mid = 0.0f; m_chromaAngle = 0.0f;
    m_bassMax = 0.15f; m_midMax = 0.15f; m_impact = 0.0f;
    lightwaveos::effects::cinema::reset();
    return true;
}

void LGPTalbotCarpetAREffect::render(plugins::EffectContext& ctx) {
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

    // Grating pitch driven by mid energy
    const float p = clampf(10.0f - 4.0f * normMid, 6.0f, 22.0f);

    // Motion + impact drives propagation depth
    float tRate = 1.0f + 4.0f * speedNorm;
    m_t += tRate * dtVis;
    const float z = m_t + 0.45f * m_impact;

    // Lock pulse near rational Talbot fractions
    const float lockPulse = expf(-fabsf(fract(z * 0.09f) - 0.5f) * 7.5f) * m_impact;

    // Adaptive harmonic count (bounded cost)
    int harmonicCount = clampf(5.0f - speedNorm, 3.0f, static_cast<float>(kMaxHarmonics));

    const float phiBase = kTwoPi / p;
    float zPhase[kMaxHarmonics];
    float harmonicNorm = 0.0f;
    for (int k = 0; k < harmonicCount; k++) {
        zPhase[k] = kHarmonicOrder[k] * kHarmonicOrder[k] * z * 0.55f;
        harmonicNorm += kHarmonicInv[k];
    }

    uint8_t baseHue = static_cast<uint8_t>(m_chromaAngle * (255.0f / kTwoPi)) + ctx.gHue;

    uint8_t fadeAmt = static_cast<uint8_t>(clampf(18.0f + 35.0f * (1.0f - normBass), 12.0f, 55.0f));
    fadeToBlackBy(ctx.leds, ctx.ledCount, fadeAmt);

    // Per-pixel render — centre-outward, 4-way symmetric
    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        const float progress = (HALF_LENGTH <= 1) ? 0.0f
            : (static_cast<float>(dist) / static_cast<float>(HALF_LENGTH - 1));

        // Fresnel harmonic sum
        float phi = static_cast<float>(dist) * phiBase;
        float sumC = 0.0f, sumS = 0.0f;
        for (int k = 0; k < harmonicCount; k++) {
            float phase = kHarmonicOrder[k] * phi + zPhase[k];
            sumC += kHarmonicInv[k] * cosf(phase);
            sumS += kHarmonicInv[k] * sinf(phase);
        }
        float amp = sqrtf(sumC * sumC + sumS * sumS) / (harmonicNorm + 1e-6f);
        amp = clamp01(amp);

        // Impact: lock pulse adds brightness
        float impactAdd = lockPulse * 0.35f;

        // Compose: audio x geometry x beat x silence
        float brightness = (normBass * amp + impactAdd) * beatMod * silScale;
        brightness *= brightness;  // Squared for punch

        uint8_t val = static_cast<uint8_t>(clamp01(brightness) * 255.0f);
        val = scale8(val, ctx.brightness);

        uint8_t hue = baseHue + static_cast<uint8_t>(progress * 22.0f);

        SET_CENTER_PAIR(ctx, dist, CHSV(hue, ctx.saturation, val));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm, lightwaveos::effects::cinema::QualityPreset::LIGHTWEIGHT);
}

void LGPTalbotCarpetAREffect::cleanup() {}

const plugins::EffectMetadata& LGPTalbotCarpetAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Talbot Carpet (5L-AR)",
        "Self-imaging lattice with direct audio-reactive composition",
        plugins::EffectCategory::QUANTUM, 1
    };
    return meta;
}
uint8_t LGPTalbotCarpetAREffect::getParameterCount() const { return 0; }
const plugins::EffectParameter* LGPTalbotCarpetAREffect::getParameter(uint8_t) const { return nullptr; }
bool LGPTalbotCarpetAREffect::setParameter(const char*, float) { return false; }
float LGPTalbotCarpetAREffect::getParameter(const char*) const { return 0.0f; }

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
