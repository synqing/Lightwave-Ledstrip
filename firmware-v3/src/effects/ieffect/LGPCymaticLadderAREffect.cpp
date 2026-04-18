/**
 * @file LGPCymaticLadderAREffect.cpp
 * @brief Cymatic Ladder (5-Layer Audio-Reactive) — REWRITTEN
 *
 * Standing-wave modes with harmonic mode selection. Mode number n (2-8)
 * driven by mid-frequency content. Audio drives brightness directly.
 *
 * Divergence fixes: Direct ControlBus reads, single-stage smoothing,
 * asymmetric max follower, no brightness floors, SET_CENTER_PAIR.
 *
 * Centre-origin compliant. Dual-strip mirrored.
 */

#include "LGPCymaticLadderAREffect.h"
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
static constexpr float kImpactDecayTau = 0.200f;

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

LGPCymaticLadderAREffect::LGPCymaticLadderAREffect() = default;

bool LGPCymaticLadderAREffect::init(plugins::EffectContext& ctx) {
    for (uint8_t zi = 0; zi < kMaxZones; ++zi) {
        m_t[zi] = 0.0f;
        m_bass[zi] = 0.0f;
        m_mid[zi] = 0.0f;
        m_chromaAngle[zi] = 0.0f;
        m_bassMax[zi] = 0.15f;
        m_midMax[zi] = 0.15f;
        m_modeSmooth[zi] = 3.0f;
        m_impact[zi] = 0.0f;
    }
    lightwaveos::effects::cinema::reset();
    return true;
}

void LGPCymaticLadderAREffect::render(plugins::EffectContext& ctx) {
    const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;
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
    m_bass[z] += (rawBass - m_bass[z]) * (1.0f - expf(-dt / kBassTau));
    m_mid[z] += (rawMid - m_mid[z]) * (1.0f - expf(-dt / kMidTau));

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
            float delta = target - m_chromaAngle[z];
            while (delta > kPi) delta -= kTwoPi;
            while (delta < -kPi) delta += kTwoPi;
            m_chromaAngle[z] += delta * (1.0f - expf(-dt / kChromaTau));
            if (m_chromaAngle[z] < 0.0f) m_chromaAngle[z] += kTwoPi;
            if (m_chromaAngle[z] >= kTwoPi) m_chromaAngle[z] -= kTwoPi;
        }
    }

    // STEP 3: Max followers
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

    // STEP 4: Impact
    if (beatStr > m_impact[z]) m_impact[z] = beatStr;
    m_impact[z] *= expf(-dt / kImpactDecayTau);

    // STEP 5: Standing wave mode selection with hysteresis
    // Mid energy drives mode number: more energy = higher harmonics
    float modeTarget = 2.0f + 6.0f * normMid;  // Range 2-8
    if (fabsf(modeTarget - m_modeSmooth[z]) > 0.3f) {
        float modeAlpha = 1.0f - expf(-dt / 0.20f);
        m_modeSmooth[z] += (modeTarget - m_modeSmooth[z]) * modeAlpha;
    }
    const int n = static_cast<int>(clampf(floorf(m_modeSmooth[z] + 0.5f), 2.0f, 8.0f));

    // Beat modulation
    const float beatMod = 0.3f + 0.7f * beatStr;

    // Standing wave phase
    float tRate = 1.0f + 4.0f * speedNorm;
    m_t[z] += tRate * dtVis;
    const float phase = m_t[z] * (0.8f + 0.5f * speedNorm);

    // Wave contrast: more bass = sharper peaks
    const float contrastExp = clampf(2.0f + 1.2f * normBass + 0.5f * m_impact[z], 1.5f, 3.8f);

    // Hue from chroma
    uint8_t baseHue = static_cast<uint8_t>(m_chromaAngle[z] * (255.0f / kTwoPi)) + ctx.gHue;

    // Trail persistence
    uint8_t fadeAmt = static_cast<uint8_t>(clampf(18.0f + 35.0f * (1.0f - normBass), 12.0f, 55.0f));
    fadeToBlackBy(ctx.leds, ctx.ledCount, fadeAmt);

    // STEP 6: Per-pixel render — centre-outward, 4-way symmetric
    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        const float progress = (HALF_LENGTH <= 1) ? 0.0f
            : (static_cast<float>(dist) / static_cast<float>(HALF_LENGTH - 1));

        // Standing wave: |sin(n * pi * progress + phase)|
        float s = fabsf(sinf(static_cast<float>(n) * kPi * progress + phase));
        float wave = powf(s, contrastExp);

        // Antinode bloom on beat
        float antinodeStr = powf(s, 1.2f);
        float impactAdd = m_impact[z] * antinodeStr * 0.35f;

        // Compose: audio magnitude x wave geometry x beat x silence
        float brightness = (normBass * wave + impactAdd) * beatMod * silScale;

        // Squared for punch
        brightness *= brightness;

        uint8_t val = static_cast<uint8_t>(clamp01(brightness) * 255.0f);
        val = scale8(val, ctx.brightness);

        // Hue: mode-offset + spatial gradient
        uint8_t hue = baseHue + static_cast<uint8_t>(static_cast<float>(n) * 8.0f + progress * 30.0f);

        SET_CENTER_PAIR(ctx, dist, CHSV(hue, ctx.saturation, val));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

void LGPCymaticLadderAREffect::cleanup() {}

const plugins::EffectMetadata& LGPCymaticLadderAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Cymatic Ladder (5L-AR)",
        "Standing-wave modes with direct audio-reactive composition",
        plugins::EffectCategory::QUANTUM, 1
    };
    return meta;
}
uint8_t LGPCymaticLadderAREffect::getParameterCount() const { return 0; }
const plugins::EffectParameter* LGPCymaticLadderAREffect::getParameter(uint8_t) const { return nullptr; }
bool LGPCymaticLadderAREffect::setParameter(const char*, float) { return false; }
float LGPCymaticLadderAREffect::getParameter(const char*) const { return 0.0f; }

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
