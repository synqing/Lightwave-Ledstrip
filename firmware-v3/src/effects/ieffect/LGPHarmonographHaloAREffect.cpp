/**
 * @file LGPHarmonographHaloAREffect.cpp
 * @brief Harmonograph Halo (5-Layer AR) — REWRITTEN
 *
 * Lissajous orbital halos with distance-to-curve band rendering.
 * Maths: x = sin(a*(phi+rot)+delta), y = sin(b*(phi-rot)), r = sqrt(x^2+y^2)*0.72
 * Distance-to-curve band creates halo, energy pulses on beat.
 *
 * Divergence fixes: Direct ControlBus reads, single-stage smoothing (50ms),
 * asymmetric max follower (58ms/500ms, floor 0.04), no brightness floors,
 * beatStrength() continuous, squared brightness for punch.
 *
 * Centre-origin compliant. Dual-strip locked.
 */

#include "LGPHarmonographHaloAREffect.h"
#include "LGPFilmPost.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include "effects/PersistenceHelpers.h"
using lightwaveos::effects::persistence::fadeToBlackByDt;

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

// Constructor
LGPHarmonographHaloAREffect::LGPHarmonographHaloAREffect() = default;

bool LGPHarmonographHaloAREffect::init(plugins::EffectContext& ctx) {
    for (uint8_t zi = 0; zi < kMaxZones; ++zi) {
        m_t[zi] = 0.0f;
        m_bass[zi] = 0.0f;
        m_treble[zi] = 0.0f;
        m_chromaAngle[zi] = 0.0f;
        m_bassMax[zi] = 0.15f;
        m_trebleMax[zi] = 0.15f;
        m_impact[zi] = 0.0f;
    }
    lightwaveos::effects::cinema::reset();
    return true;
}

void LGPHarmonographHaloAREffect::render(plugins::EffectContext& ctx) {
    const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;
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
    m_bass[z] += (rawBass - m_bass[z]) * (1.0f - expf(-dt / kBassTau));
    m_treble[z] += (rawTreble - m_treble[z]) * (1.0f - expf(-dt / kTrebleTau));

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

        if (m_treble[z] > m_trebleMax[z]) m_trebleMax[z] += (m_treble[z] - m_trebleMax[z]) * aA;
        else m_trebleMax[z] += (m_treble[z] - m_trebleMax[z]) * dA;
        if (m_trebleMax[z] < kFollowerFloor) m_trebleMax[z] = kFollowerFloor;
    }
    const float normBass = clamp01(m_bass[z] / m_bassMax[z]);
    const float normTreble = clamp01(m_treble[z] / m_trebleMax[z]);

    // STEP 4: Impact (continuous beatStrength rise, exponential decay)
    if (beatStr > m_impact[z]) m_impact[z] = beatStr;
    m_impact[z] *= expf(-dt / kImpactDecayTau);

    // STEP 5: Halo visual parameters
    const float beatMod = 0.3f + 0.7f * beatStr;

    // Lissajous frequency ratios driven by normalised treble
    const float freqA = 2.5f + 1.0f * normTreble;
    const float freqB = 2.0f + 1.0f * normTreble;

    // Phase offset drifts with bass
    const float deltaDrift = (0.5f + 0.8f * normBass) * dt;
    const float lissajousDelta = fract(m_chromaAngle[z] / kTwoPi + deltaDrift) * kTwoPi;

    // Rotation accumulation
    float rotRate = 0.85f + 3.50f * speedNorm;
    m_t[z] += rotRate * dtVis;

    // Halo band width (bass-driven)
    const float bandWidth = clampf(0.12f + 0.08f * normBass + 0.05f * m_impact[z], 0.08f, 0.25f);

    // Hue from chroma
    uint8_t baseHue = static_cast<uint8_t>(m_chromaAngle[z] * (255.0f / kTwoPi)) + ctx.gHue;

    // Trail persistence: more energy = shorter trails
    uint8_t fadeAmt = static_cast<uint8_t>(clampf(20.0f + 30.0f * (1.0f - normBass), 14.0f, 52.0f));
    fadeToBlackByDt(ctx.leds, ctx.ledCount, fadeAmt, ctx.getSafeDeltaSeconds());

    // =========================================================================
    // Lissajous orbital sampling (simulation physics — PRESERVED)
    // =========================================================================

    static constexpr int kOrbitSamples = 64;
    float orbitX[kOrbitSamples];
    float orbitY[kOrbitSamples];

    for (int s = 0; s < kOrbitSamples; s++) {
        const float phi = kTwoPi * (static_cast<float>(s) / static_cast<float>(kOrbitSamples));
        // Lissajous: x = sin(a*(phi+rot)+delta), y = sin(b*(phi-rot))
        const float x = sinf(freqA * (phi + m_t[z]) + lissajousDelta);
        const float y = sinf(freqB * (phi - m_t[z]));
        // Map to LED strip coordinate space (scaled by 0.72 for halo aesthetic)
        orbitX[s] = x * 0.72f;
        orbitY[s] = y * 0.72f;
    }

    // STEP 6: Per-pixel render (centre-origin)
    const float mid    = (STRIP_LENGTH - 1) * 0.5f;
    const float invMid = 1.0f / mid;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float dmid  = static_cast<float>(i) - mid;
        const float distN = fabsf(dmid) * invMid;  // 0 at centre, 1 at edges

        // Map LED position to 2D polar (simple radial mapping for LGP)
        const float ledX = distN;
        const float ledY = 0.0f;

        // Find minimum distance to orbital curve
        float minDist = 999.0f;
        for (int s = 0; s < kOrbitSamples; s++) {
            const float dx = ledX - orbitX[s];
            const float dy = ledY - orbitY[s];
            const float dist = sqrtf(dx * dx + dy * dy);
            if (dist < minDist) minDist = dist;
        }

        // Distance-to-curve band creates halo
        const float distBand = 1.0f - clamp01(minDist / bandWidth);
        const float geom = powf(distBand, 2.2f);

        // Centre glue
        const float glue = 0.40f + 0.60f * expf(-(dmid * dmid) * 0.0015f);

        // Halo geometry
        float haloShape = geom * glue;

        // Impact: additive orbital flash at curve points
        float impactShape = powf(distBand, 1.5f) * glue;
        float impactAdd = m_impact[z] * impactShape * 0.4f;

        // Compose brightness: audio magnitude x geometry x beat x silence
        float brightness = (haloShape * normBass + impactAdd) * beatMod * silScale;

        // Squared for punch
        brightness *= brightness;

        uint8_t val = static_cast<uint8_t>(clamp01(brightness) * 255.0f);
        val = scale8(val, ctx.brightness);

        // Hue: chroma anchor + orbital spatial offset
        uint8_t hue = baseHue + static_cast<uint8_t>(distBand * 48.0f);

        writeDualLocked(ctx, i, CHSV(hue, ctx.saturation, val));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

// Cleanup / metadata / parameters
void LGPHarmonographHaloAREffect::cleanup() {}

const plugins::EffectMetadata& LGPHarmonographHaloAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Harmonograph Halo (5L-AR)",
        "Lissajous orbital halos with direct audio-reactive composition",
        plugins::EffectCategory::QUANTUM, 1
    };
    return meta;
}
uint8_t LGPHarmonographHaloAREffect::getParameterCount() const { return 0; }
const plugins::EffectParameter* LGPHarmonographHaloAREffect::getParameter(uint8_t) const { return nullptr; }
bool LGPHarmonographHaloAREffect::setParameter(const char*, float) { return false; }
float LGPHarmonographHaloAREffect::getParameter(const char*) const { return 0.0f; }

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
