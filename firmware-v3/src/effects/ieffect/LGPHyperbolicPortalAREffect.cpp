/**
 * @file LGPHyperbolicPortalAREffect.cpp
 * @brief Hyperbolic Portal (5-Layer AR) — REWRITTEN
 *
 * Poincare-style hyperbolic portal with multi-band ribs.
 * Hyperbolic coordinate: u = artanh(r * 0.985)
 * Rib pattern: sin(w1*u + phi) + 0.62*sin(w2*u - 0.7*phi)
 * Sharp bands via smoothstep + pow, edge boost, centre glue.
 *
 * Divergence fixes: Direct ControlBus reads, single-stage smoothing (50ms),
 * asymmetric max follower (58ms/500ms, floor 0.04), no brightness floors,
 * beatStrength() continuous, squared brightness for punch.
 *
 * Centre-origin compliant. Dual-strip locked.
 */

#include "LGPHyperbolicPortalAREffect.h"
#include "LGPFilmPost.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

// Constants
static constexpr float kTwoPi = 6.28318530717958647692f;
static constexpr float kPi    = 3.14159265358979323846f;
static constexpr float kBassTau    = 0.050f;
static constexpr float kTrebleTau  = 0.040f;
static constexpr float kChromaTau  = 0.300f;
static constexpr float kFollowerAttackTau = 0.058f;
static constexpr float kFollowerDecayTau  = 0.500f;
static constexpr float kFollowerFloor     = 0.04f;
static constexpr float kImpactDecayTau = 0.180f;

// Helpers
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

static inline void writeDualLocked(plugins::EffectContext& ctx, int i, const CRGB& c) {
    ctx.leds[i] = c;
    int j = i + STRIP_LENGTH;
    if (j < static_cast<int>(ctx.ledCount)) ctx.leds[j] = c;
}

// Safe artanh with clamp near singularity at r=1
static inline float artanh_safe(float r) {
    const float clamped = clampf(r, -0.9999f, 0.9999f);
    return 0.5f * logf((1.0f + clamped) / (1.0f - clamped));
}

static inline float smoothstep(float edge0, float edge1, float x) {
    float t = clamp01((x - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

// Constructor
LGPHyperbolicPortalAREffect::LGPHyperbolicPortalAREffect()
    : m_phi(0.0f), m_bass(0.0f), m_treble(0.0f), m_chromaAngle(0.0f),
      m_bassMax(0.15f), m_trebleMax(0.15f), m_impact(0.0f) {}

bool LGPHyperbolicPortalAREffect::init(plugins::EffectContext& ctx) {
    m_phi = 0.0f; m_bass = 0.0f; m_treble = 0.0f; m_chromaAngle = 0.0f;
    m_bassMax = 0.15f; m_trebleMax = 0.15f; m_impact = 0.0f;
    lightwaveos::effects::cinema::reset();
    return true;
}

void LGPHyperbolicPortalAREffect::render(plugins::EffectContext& ctx) {
    const float dt = ctx.getSafeRawDeltaSeconds();
    const float dtVis = ctx.getSafeDeltaSeconds();
    const float speedNorm = ctx.speed / 50.0f;

    // STEP 1: Direct ControlBus reads
    const float rawBass = ctx.audio.available ? ctx.audio.bass() : 0.0f;
    const float rawTreble = ctx.audio.available ? ctx.audio.treble() : 0.0f;
    const float beatStr = ctx.audio.available ? ctx.audio.beatStrength() : 0.0f;
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

    // STEP 4: Impact (continuous beatStrength rise, exponential decay)
    if (beatStr > m_impact) m_impact = beatStr;
    m_impact *= expf(-dt / kImpactDecayTau);

    // STEP 5: Portal visual parameters
    const float beatMod = 0.3f + 0.7f * beatStr;

    // Rib frequencies driven by normalised treble
    const float w1 = 6.0f + 8.0f * normTreble;
    const float w2 = 10.0f + 10.0f * normTreble;

    // Phase rotation accumulation
    float phiRate = 0.80f + 3.50f * speedNorm;
    m_phi += phiRate * dtVis;

    // Hue from chroma
    uint8_t baseHue = static_cast<uint8_t>(m_chromaAngle * (255.0f / kTwoPi)) + ctx.gHue;

    // Trail persistence: more energy = shorter trails
    uint8_t fadeAmt = static_cast<uint8_t>(clampf(20.0f + 30.0f * (1.0f - normBass), 14.0f, 52.0f));
    fadeToBlackBy(ctx.leds, ctx.ledCount, fadeAmt);

    // STEP 6: Per-pixel render (centre-origin)
    const float mid    = (STRIP_LENGTH - 1) * 0.5f;
    const float invMid = 1.0f / mid;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float dmid  = static_cast<float>(i) - mid;
        const float distN = fabsf(dmid) * invMid;

        // Hyperbolic coordinate mapping
        const float r = distN * 0.985f;
        const float u = artanh_safe(r);

        // Multi-band ribs
        const float rib1 = sinf(w1 * u + m_phi);
        const float rib2 = sinf(w2 * u - 0.7f * m_phi);
        const float ribRaw = rib1 + 0.62f * rib2;

        // Sharp bands via smoothstep + pow
        const float ribSmooth = smoothstep(-0.5f, 0.5f, ribRaw);
        const float ribSharp = powf(ribSmooth, 2.8f);

        // Edge boost (stronger at hyperbolic infinity)
        const float edgeBoost = 1.0f + 0.6f * distN;

        // Centre glue
        const float glue = 0.40f + 0.60f * expf(-(dmid * dmid) * 0.0015f);

        // Portal shape
        const float portalShape = ribSharp * edgeBoost * glue;

        // Impact: brightens ribs
        const float impactAdd = m_impact * ribSharp * 0.4f;

        // Compose brightness: audio magnitude x geometry x beat x silence
        float brightness = (portalShape * normBass + impactAdd) * beatMod * silScale;

        // Squared for punch
        brightness *= brightness;

        uint8_t val = static_cast<uint8_t>(clamp01(brightness) * 255.0f);
        val = scale8(val, ctx.brightness);

        // Hue: chroma anchor + hyperbolic spatial offset
        uint8_t hue = baseHue + static_cast<uint8_t>(u * 35.0f);

        writeDualLocked(ctx, i, CHSV(hue, ctx.saturation, val));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

// Cleanup / metadata / parameters
void LGPHyperbolicPortalAREffect::cleanup() {}

const plugins::EffectMetadata& LGPHyperbolicPortalAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Hyperbolic Portal (5L-AR)",
        "Poincare-style portal with direct audio-reactive composition",
        plugins::EffectCategory::QUANTUM, 1
    };
    return meta;
}
uint8_t LGPHyperbolicPortalAREffect::getParameterCount() const { return 0; }
const plugins::EffectParameter* LGPHyperbolicPortalAREffect::getParameter(uint8_t) const { return nullptr; }
bool LGPHyperbolicPortalAREffect::setParameter(const char*, float) { return false; }
float LGPHyperbolicPortalAREffect::getParameter(const char*) const { return 0.0f; }

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
