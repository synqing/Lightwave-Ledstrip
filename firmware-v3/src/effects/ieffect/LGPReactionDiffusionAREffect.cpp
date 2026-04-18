/**
 * @file LGPReactionDiffusionAREffect.cpp
 * @brief Reaction Diffusion (5-Layer AR) — REWRITTEN
 *
 * Gray-Scott 1D simulation. Simulation generates spatial pattern.
 * Audio drives brightness directly (not just F/K coefficients).
 * PSRAM-allocated. Beat injects V at centre. Centre-origin compliant.
 */

#include "LGPReactionDiffusionAREffect.h"
#include "LGPFilmPost.h"
#include "../CoreEffects.h"
#include "../../utils/Log.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

static constexpr float kTwoPi = 6.28318530717958647692f;
static constexpr float kPi    = 3.14159265358979323846f;
static constexpr float kBassTau   = 0.050f;
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

LGPReactionDiffusionAREffect::LGPReactionDiffusionAREffect() = default;

bool LGPReactionDiffusionAREffect::init(plugins::EffectContext& ctx) {
    for (uint8_t zi = 0; zi < kMaxZones; ++zi) {
        m_t[zi] = 0.0f;
        m_bass[zi] = 0.0f;
        m_chromaAngle[zi] = 0.0f;
        m_bassMax[zi] = 0.15f;
        m_impact[zi] = 0.0f;
    }
    lightwaveos::effects::cinema::reset();

#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<PsramData*>(heap_caps_malloc(sizeof(PsramData), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            LW_LOGE("LGPReactionDiffusionAREffect: PSRAM alloc failed (%u bytes)",
                    (unsigned)sizeof(PsramData));
            return false;
        }
    }
#endif

    const int mid = kStripLen / 2;
    for (uint8_t zi = 0; zi < kMaxZones; ++zi) {
#ifndef NATIVE_BUILD
        float* u = m_ps->u[zi];
        float* v = m_ps->v[zi];
        float* u2 = m_ps->u2[zi];
        float* v2 = m_ps->v2[zi];
#else
        float* u = m_u[zi];
        float* v = m_v[zi];
        float* u2 = m_u2[zi];
        float* v2 = m_v2[zi];
#endif
        // Gray-Scott init: U=1 everywhere, V=0; seed centre with V.
        for (int i = 0; i < kStripLen; i++) {
            u[i] = 1.0f;
            v[i] = 0.0f;
            u2[i] = 1.0f;
            v2[i] = 0.0f;
        }
        for (int i = mid - 6; i <= mid + 6; i++) {
            if (i >= 0 && i < kStripLen) {
                v[i] = 1.0f;
                u[i] = 0.0f;
                v2[i] = 1.0f;
                u2[i] = 0.0f;
            }
        }
    }
    return true;
}

void LGPReactionDiffusionAREffect::render(plugins::EffectContext& ctx) {
    const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;

#ifndef NATIVE_BUILD
    if (!m_ps) return;
    float* u = m_ps->u[z];
    float* v = m_ps->v[z];
    float* u2 = m_ps->u2[z];
    float* v2 = m_ps->v2[z];
#else
    float* u = m_u[z];
    float* v = m_v[z];
    float* u2 = m_u2[z];
    float* v2 = m_v2[z];
#endif

    const float dt = ctx.getSafeRawDeltaSeconds();
    const float dtVis = ctx.getSafeDeltaSeconds();
    const float speedNorm = ctx.speed / 50.0f;

    // STEP 1: Direct ControlBus reads
    const float rawBass = ctx.audio.available ? ctx.audio.bass() : 0.0f;
    const float beatStr = ctx.audio.available ? ctx.audio.beatStrength() : 0.0f;
    const float silScale = ctx.audio.available ? ctx.audio.silentScale() : 0.0f;
    const float* chroma = ctx.audio.available ? ctx.audio.chroma() : nullptr;

    // STEP 2: Single-stage smoothing
    m_bass[z] += (rawBass - m_bass[z]) * (1.0f - expf(-dt / kBassTau));

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

    // STEP 3: Max follower
    {
        float aA = 1.0f - expf(-dt / kFollowerAttackTau);
        float dA = 1.0f - expf(-dt / kFollowerDecayTau);
        if (m_bass[z] > m_bassMax[z]) m_bassMax[z] += (m_bass[z] - m_bassMax[z]) * aA;
        else m_bassMax[z] += (m_bass[z] - m_bassMax[z]) * dA;
        if (m_bassMax[z] < kFollowerFloor) m_bassMax[z] = kFollowerFloor;
    }
    const float normBass = clamp01(m_bass[z] / m_bassMax[z]);

    // STEP 4: Impact
    if (beatStr > m_impact[z]) m_impact[z] = beatStr;
    m_impact[z] *= expf(-dt / kImpactDecayTau);

    // STEP 5: Gray-Scott simulation (pattern generator)
    // F/K modulated by bass for visual variety — but brightness is direct
    const float F = clampf(0.0380f + 0.0070f * (normBass - 0.5f), 0.035f, 0.042f);
    const float K = clampf(0.0630f + 0.0100f * (normBass - 0.5f), 0.058f, 0.068f);
    const float Du = 1.0f, Dv = 0.5f;
    const float simDt = clampf(0.9f + 0.6f * speedNorm, 0.7f, 2.2f);

    // Beat injects V at centre
    if (m_impact[z] > 0.05f) {
        const int mid = kStripLen / 2;
        float injectAmt = m_impact[z] * 0.15f;
        for (int i = mid - 5; i <= mid + 5; i++) {
            if (i >= 0 && i < kStripLen)
                v[i] = clamp01(v[i] + injectAmt);
        }
    }

    // 1-2 iterations per frame
    const int iters = (speedNorm > 0.55f) ? 2 : 1;
    for (int iter = 0; iter < iters; iter++) {
        for (int i = 0; i < kStripLen; i++) {
            int im1 = (i == 0) ? 0 : (i - 1);
            int ip1 = (i == kStripLen - 1) ? (kStripLen - 1) : (i + 1);
            float lapU = u[im1] - 2.0f * u[i] + u[ip1];
            float lapV = v[im1] - 2.0f * v[i] + v[ip1];
            float uVal = u[i];
            float vVal = v[i];
            float uvv = uVal * vVal * vVal;
            u2[i] = clamp01(uVal + (Du * lapU - uvv + F * (1.0f - uVal)) * simDt);
            v2[i] = clamp01(vVal + (Dv * lapV + uvv - (K + F) * vVal) * simDt);
        }
        for (int i = 0; i < kStripLen; i++) {
            u[i] = u2[i];
            v[i] = v2[i];
        }
    }

    // STEP 6: Per-pixel render
    const float beatMod = 0.3f + 0.7f * beatStr;
    const float midF = (kStripLen - 1) * 0.5f;
    uint8_t baseHue = static_cast<uint8_t>(m_chromaAngle[z] * (255.0f / kTwoPi)) + ctx.gHue;

    uint8_t fadeAmt = static_cast<uint8_t>(clampf(18.0f + 35.0f * (1.0f - normBass), 12.0f, 55.0f));
    fadeToBlackBy(ctx.leds, ctx.ledCount, fadeAmt);

    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        const float progress = (HALF_LENGTH <= 1) ? 0.0f
            : (static_cast<float>(dist) / static_cast<float>(HALF_LENGTH - 1));

        // Map centre-outward dist to strip index for V lookup
        int idxR = CENTER_RIGHT + dist;
        float vSample = (idxR < kStripLen) ? v[idxR] : 0.0f;

        // Centre melt
        float dmid = static_cast<float>(dist);
        float melt = expf(-(dmid * dmid) * 0.0018f);

        // Impact at centre
        float impactAdd = m_impact[z] * melt * 0.35f;

        // Compose: audio x V-pattern x beat x silence
        float brightness = (normBass * (vSample * melt + 0.25f * vSample) + impactAdd) * beatMod * silScale;
        brightness *= brightness;  // Squared for punch

        uint8_t val = static_cast<uint8_t>(clamp01(brightness) * 255.0f);
        val = scale8(val, ctx.brightness);

        // Hue: chroma base + V-driven tonal range + spatial offset
        uint8_t hue = baseHue + static_cast<uint8_t>(vSample * 120.0f + progress * 15.0f);

        SET_CENTER_PAIR(ctx, dist, CHSV(hue, ctx.saturation, val));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
    m_t[z] += 1.0f;
}

void LGPReactionDiffusionAREffect::cleanup() {
    // Retain PSRAM across effect lifetime; freeing on cleanup() fragments PSRAM under rapid cycling
    // and forces a fresh malloc on next init(). init() already guards with if (!m_ps).
}

const plugins::EffectMetadata& LGPReactionDiffusionAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Reaction Diffusion (5L-AR)",
        "Gray-Scott slime with direct audio-reactive composition",
        plugins::EffectCategory::QUANTUM, 1
    };
    return meta;
}
uint8_t LGPReactionDiffusionAREffect::getParameterCount() const { return 0; }
const plugins::EffectParameter* LGPReactionDiffusionAREffect::getParameter(uint8_t) const { return nullptr; }
bool LGPReactionDiffusionAREffect::setParameter(const char*, float) { return false; }
float LGPReactionDiffusionAREffect::getParameter(const char*) const { return 0.0f; }

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
