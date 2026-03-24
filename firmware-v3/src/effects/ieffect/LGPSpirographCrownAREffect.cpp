/**
 * @file LGPSpirographCrownAREffect.cpp
 * @brief Spirograph Crown (5-Layer AR) — REWRITTEN
 *
 * Hypotrochoid crown geometry with facet sparkle.
 * Base maths: parametric hypotrochoid with R=1.0, r=0.30+drift, d=0.78.
 * Distance-to-curve metric creates luminous band structure.
 *
 * Divergence fixes: Direct ControlBus reads, single-stage smoothing (50ms),
 * asymmetric max follower (58ms/500ms, floor 0.04), no brightness floors,
 * beatStrength() continuous, squared brightness for punch.
 *
 * Centre-origin compliant. Dual-strip locked.
 */

#include "LGPSpirographCrownAREffect.h"
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
static inline float fract(float x) { return x - floorf(x); }

static inline void writeDualLocked(plugins::EffectContext& ctx, int i, const CRGB& c) {
    ctx.leds[i] = c;
    int j = i + STRIP_LENGTH;
    if (j < static_cast<int>(ctx.ledCount)) ctx.leds[j] = c;
}

// =========================================================================
// Hypotrochoid distance metric (simulation physics — PRESERVED)
// =========================================================================

/**
 * Compute distance from polar coordinate (r, theta) to hypotrochoid curve.
 * Sample the curve at N theta increments and return minimum distance.
 *
 * Parametric hypotrochoid:
 *   x = (R - r_param)*cos(t) + d*cos((R - r_param)/r_param * t)
 *   y = (R - r_param)*sin(t) - d*sin((R - r_param)/r_param * t)
 *
 * Where R=1.0, r_param varies, d=0.78
 */
static float distToCurve(float r, float theta, float r_param) {
    const float R = 1.0f;
    const float d = 0.78f;
    const int samples = 64;

    float minDist = 10.0f;
    const float ratio = (R - r_param) / r_param;

    for (int i = 0; i < samples; i++) {
        const float t = (kTwoPi * static_cast<float>(i)) / static_cast<float>(samples);

        // Hypotrochoid parametric equations
        const float x = (R - r_param) * cosf(t) + d * cosf(ratio * t);
        const float y = (R - r_param) * sinf(t) - d * sinf(ratio * t);

        // Convert sampled point to polar
        const float curve_r = sqrtf(x * x + y * y);
        const float curve_theta = atan2f(y, x);

        // Angular distance (wrapped)
        float dtheta = fabsf(theta - curve_theta);
        if (dtheta > kPi) dtheta = kTwoPi - dtheta;

        // Combined distance metric (radial + angular weighted)
        const float dist = sqrtf((r - curve_r) * (r - curve_r) +
                                 (r * dtheta) * (r * dtheta));

        if (dist < minDist) minDist = dist;
    }

    return minDist;
}

// Constructor
LGPSpirographCrownAREffect::LGPSpirographCrownAREffect()
    : m_t(0.0f), m_bass(0.0f), m_treble(0.0f), m_chromaAngle(0.0f),
      m_bassMax(0.15f), m_trebleMax(0.15f), m_impact(0.0f) {}

bool LGPSpirographCrownAREffect::init(plugins::EffectContext& ctx) {
    m_t = 0.0f; m_bass = 0.0f; m_treble = 0.0f; m_chromaAngle = 0.0f;
    m_bassMax = 0.15f; m_trebleMax = 0.15f; m_impact = 0.0f;
    lightwaveos::effects::cinema::reset();
    return true;
}

void LGPSpirographCrownAREffect::render(plugins::EffectContext& ctx) {
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

    // STEP 5: Crown visual parameters
    const float beatMod = 0.3f + 0.7f * beatStr;

    // r parameter drift: treble modulates the hypotrochoid shape
    const float r_drift = 0.30f + 0.12f * (normTreble - 0.5f);

    // Band width: impact narrows the crown band
    const float bandWidth = clampf(0.08f - 0.03f * m_impact, 0.03f, 0.10f);

    // Time accumulation
    float tRate = 0.80f + 3.50f * speedNorm;
    m_t += tRate * dtVis;

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

        // Convert to polar coordinates (centre origin at LED 79/80)
        const float r = distN;
        const float theta = (m_t * 0.35f) + static_cast<float>(i) * 0.042f;

        // Distance to hypotrochoid curve
        const float dist = distToCurve(r, theta, r_drift);

        // Band structure: crown appears where dist is small
        const float band = 1.0f - clamp01(dist / bandWidth);
        const float bandSharp = powf(band, 2.2f);

        // Facet sparkle modulation (treble-driven)
        const float facet = (0.85f + 0.15f * normTreble) *
                            (0.90f + 0.10f * cosf(kTwoPi * theta * 3.7f));

        // Centre glue: stronger near centre
        const float glue = 0.40f + 0.60f * expf(-(dmid * dmid) * 0.0015f);

        // Crown geometry
        const float crownShape = bandSharp * facet * glue;

        // Impact: additive flash at crown band
        const float impactAdd = m_impact * bandSharp * glue * 0.4f;

        // Compose brightness: audio magnitude x geometry x beat x silence
        float brightness = (crownShape * normBass + impactAdd) * beatMod * silScale;

        // Squared for punch
        brightness *= brightness;

        uint8_t val = static_cast<uint8_t>(clamp01(brightness) * 255.0f);
        val = scale8(val, ctx.brightness);

        // Hue: chroma anchor + crown spatial offset
        uint8_t hue = baseHue + static_cast<uint8_t>(band * 45.0f);

        writeDualLocked(ctx, i, CHSV(hue, ctx.saturation, val));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

// Cleanup / metadata / parameters
void LGPSpirographCrownAREffect::cleanup() {}

const plugins::EffectMetadata& LGPSpirographCrownAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Spirograph Crown (5L-AR)",
        "Hypotrochoid crown with direct audio-reactive composition",
        plugins::EffectCategory::QUANTUM, 1
    };
    return meta;
}
uint8_t LGPSpirographCrownAREffect::getParameterCount() const { return 0; }
const plugins::EffectParameter* LGPSpirographCrownAREffect::getParameter(uint8_t) const { return nullptr; }
bool LGPSpirographCrownAREffect::setParameter(const char*, float) { return false; }
float LGPSpirographCrownAREffect::getParameter(const char*) const { return 0.0f; }

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
