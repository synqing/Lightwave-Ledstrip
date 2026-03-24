/**
 * @file LGPSchlierenFlowAREffect.cpp
 * @brief Schlieren Flow (5-Layer Audio-Reactive) — REWRITTEN
 *
 * Knife-edge gradient flow with tanh edge detection. Audio drives
 * edge sharpness and flow speed directly. 3 percussion layers.
 *
 * Divergence fixes: Direct ControlBus reads, single-stage smoothing,
 * asymmetric max follower, no brightness floors, beatStrength() continuous.
 *
 * NOTE: Independent strip colours preserved — uses per-strip write, not SET_CENTER_PAIR.
 * Centre-origin compliant via centreDistance weighting.
 */

#include "LGPSchlierenFlowAREffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

static constexpr float kTwoPi = 6.28318530717958647692f;
static constexpr float kPi    = 3.14159265358979323846f;
static constexpr float kBassTau    = 0.050f;
static constexpr float kTrebleTau  = 0.040f;
static constexpr float kChromaTau  = 0.300f;
static constexpr float kFollowerAttackTau = 0.058f;
static constexpr float kFollowerDecayTau  = 0.500f;
static constexpr float kFollowerFloor     = 0.04f;
static constexpr float kImpactDecayTau    = 0.180f;
static constexpr float kSnareDecayTau     = 0.220f;
static constexpr float kHihatDecayTau     = 0.150f;

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

LGPSchlierenFlowAREffect::LGPSchlierenFlowAREffect()
    : m_t(0.0f), m_bass(0.0f), m_treble(0.0f), m_chromaAngle(0.0f),
      m_bassMax(0.15f), m_trebleMax(0.15f),
      m_impact(0.0f), m_snareImpact(0.0f), m_hihatImpact(0.0f) {}

bool LGPSchlierenFlowAREffect::init(plugins::EffectContext& ctx) {
    m_t = 0.0f; m_bass = 0.0f; m_treble = 0.0f; m_chromaAngle = 0.0f;
    m_bassMax = 0.15f; m_trebleMax = 0.15f;
    m_impact = 0.0f; m_snareImpact = 0.0f; m_hihatImpact = 0.0f;
    return true;
}

void LGPSchlierenFlowAREffect::render(plugins::EffectContext& ctx) {
    const float dt = ctx.getSafeRawDeltaSeconds();
    const float dtVis = ctx.getSafeDeltaSeconds();
    const float speedNorm = ctx.speed / 50.0f;

    // STEP 1: Direct ControlBus reads
    const float rawBass = ctx.audio.available ? ctx.audio.bass() : 0.0f;
    const float rawTreble = ctx.audio.available ? ctx.audio.treble() : 0.0f;
    const float beatStr = ctx.audio.available ? ctx.audio.beatStrength() : 0.0f;
    const bool snareHit = ctx.audio.available && ctx.audio.isSnareHit();
    const bool hihatHit = ctx.audio.available && ctx.audio.isHihatHit();
    const float silScale = ctx.audio.available ? ctx.audio.silentScale() : 0.0f;
    const float* chroma = ctx.audio.available ? ctx.audio.chroma() : nullptr;

    // STEP 2: Single-stage smoothing
    m_bass += (rawBass - m_bass) * (1.0f - expf(-dt / kBassTau));
    m_treble += (rawTreble - m_treble) * (1.0f - expf(-dt / kTrebleTau));

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

        if (m_treble > m_trebleMax) m_trebleMax += (m_treble - m_trebleMax) * aA;
        else m_trebleMax += (m_treble - m_trebleMax) * dA;
        if (m_trebleMax < kFollowerFloor) m_trebleMax = kFollowerFloor;
    }
    const float normBass = clamp01(m_bass / m_bassMax);
    const float normTreble = clamp01(m_treble / m_trebleMax);

    // STEP 4: 3 percussion impact layers
    if (beatStr > m_impact) m_impact = beatStr;
    m_impact *= expf(-dt / kImpactDecayTau);

    if (snareHit) m_snareImpact = fmaxf(m_snareImpact, 0.75f);
    m_snareImpact *= expf(-dt / kSnareDecayTau);

    if (hihatHit) m_hihatImpact = fmaxf(m_hihatImpact, 0.70f);
    m_hihatImpact *= expf(-dt / kHihatDecayTau);

    // STEP 5: Schlieren visual parameters
    const float beatMod = 0.3f + 0.7f * beatStr;

    // Flow rate: bass drives speed
    float flowRate = (1.4f + 8.0f * speedNorm) * (0.7f + 0.5f * normBass);
    m_t += flowRate * dtVis;

    // Spatial frequencies: bass modulates low freq, treble modulates high
    float f1 = 0.060f * (1.0f + 0.20f * normBass);
    float f2 = 0.145f * (1.0f + 0.15f * normBass);
    float f3 = 0.310f * (1.0f + 0.20f * normTreble);

    // Edge sharpness: treble sharpens, hihat pops
    float edgeGain = 5.0f + 4.0f * normTreble + 3.0f * m_hihatImpact;

    // Hue from chroma
    uint8_t baseHue = static_cast<uint8_t>(m_chromaAngle * (255.0f / kTwoPi)) + ctx.gHue;

    float midPos = (STRIP_LENGTH - 1) * 0.5f;

    // Trail persistence
    uint8_t fadeAmt = static_cast<uint8_t>(clampf(16.0f + 30.0f * (1.0f - normBass), 10.0f, 50.0f));
    fadeToBlackBy(ctx.leds, ctx.ledCount, fadeAmt);

    // STEP 6: Per-pixel render (linear — independent strip colours)
    for (int i = 0; i < STRIP_LENGTH; i++) {
        float x = static_cast<float>(i);
        float dmid = x - midPos;

        // Knife-edge gradient flow (3 sine layers)
        float rho = sinf(x * f1 + m_t)
                  + 0.7f * sinf(x * f2 - m_t * 1.2f)
                  + 0.3f * sinf(x * f3 + m_t * 2.1f);

        float grad = f1 * cosf(x * f1 + m_t)
                   + 0.7f * f2 * cosf(x * f2 - m_t * 1.2f)
                   + 0.3f * f3 * cosf(x * f3 + m_t * 2.1f);

        // Edge detection via tanh
        float edge = 0.5f + 0.5f * tanhf(grad * edgeGain);

        // Centre melt
        float melt = expf(-(dmid * dmid) * 0.0028f);

        // Structured edge
        float structuredEdge = 0.65f * (edge * melt + 0.25f * melt)
                             + 0.35f * (0.5f + 0.5f * sinf(rho));

        // 3 percussion layers at edge positions
        float impactAdd = m_impact * edge * 0.30f
                        + m_snareImpact * edge * 0.25f
                        + m_hihatImpact * edge * 0.20f;

        // Compose brightness: audio x geometry x beat x silence
        float brightness = (normBass * structuredEdge + impactAdd) * beatMod * silScale;

        // Squared for punch
        brightness *= brightness;

        uint8_t val = static_cast<uint8_t>(clamp01(brightness) * 255.0f);
        val = scale8(val, ctx.brightness);

        float distN = fabsf(dmid) / midPos;

        // Strip A: chroma hue + spatial offset
        uint8_t hueA = baseHue + static_cast<uint8_t>(distN * 18.0f);
        ctx.leds[i] = CHSV(hueA, ctx.saturation, val);

        // Strip B: slight hue offset for visual depth
        int j = i + STRIP_LENGTH;
        if (j < static_cast<int>(ctx.ledCount)) {
            uint8_t hueB = hueA + 5;
            uint8_t valB = scale8(val, 245);
            ctx.leds[j] = CHSV(hueB, ctx.saturation, valB);
        }
    }
}

void LGPSchlierenFlowAREffect::cleanup() {}

const plugins::EffectMetadata& LGPSchlierenFlowAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Schlieren Flow (5L-AR)",
        "Knife-edge gradient flow with direct audio-reactive composition",
        plugins::EffectCategory::QUANTUM, 1
    };
    return meta;
}
uint8_t LGPSchlierenFlowAREffect::getParameterCount() const { return 0; }
const plugins::EffectParameter* LGPSchlierenFlowAREffect::getParameter(uint8_t) const { return nullptr; }
bool LGPSchlierenFlowAREffect::setParameter(const char*, float) { return false; }
float LGPSchlierenFlowAREffect::getParameter(const char*) const { return 0.0f; }

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
