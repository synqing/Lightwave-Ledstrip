/**
 * @file LGPRDTriangleAREffect.cpp
 * @brief LGP Reaction Diffusion Triangle (5-Layer AR) -- REWRITTEN
 *
 * Gray-Scott reaction-diffusion system with direct audio-reactive composition.
 * Base maths from LGPReactionDiffusionTriangleEffect preserved exactly.
 *
 * Divergence fixes: Direct ControlBus reads, single-stage smoothing (50ms),
 * asymmetric max follower (58ms/500ms, floor 0.04), no brightness floors,
 * beatStrength() continuous, squared brightness for punch.
 *
 * Centre-origin compliant. Dual-strip locked.
 */

#include "LGPRDTriangleAREffect.h"
#include "LGPFilmPost.h"
#include "../CoreEffects.h"
#include "../../utils/Log.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

// =========================================================================
// Constants
// =========================================================================

static constexpr float kTwoPi = 6.28318530717958647692f;
static constexpr float kPi    = 3.14159265358979323846f;
static constexpr float kBassTau    = 0.050f;
static constexpr float kTrebleTau  = 0.040f;
static constexpr float kChromaTau  = 0.300f;
static constexpr float kFollowerAttackTau = 0.058f;
static constexpr float kFollowerDecayTau  = 0.500f;
static constexpr float kFollowerFloor     = 0.04f;
static constexpr float kImpactDecayTau = 0.180f;

// =========================================================================
// Local helpers
// =========================================================================

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
    if (j < (int)ctx.ledCount) ctx.leds[j] = c;
}

// =========================================================================
// Construction / init / cleanup
// =========================================================================

LGPRDTriangleAREffect::LGPRDTriangleAREffect() = default;

bool LGPRDTriangleAREffect::init(plugins::EffectContext& ctx) {
    for (uint8_t zi = 0; zi < kMaxZones; ++zi) {
        m_bass[zi] = 0.0f;
        m_treble[zi] = 0.0f;
        m_chromaAngle[zi] = 0.0f;
        m_bassMax[zi] = 0.15f;
        m_trebleMax[zi] = 0.15f;
        m_impact[zi] = 0.0f;
        m_F[zi] = 0.0380f;
        m_K[zi] = 0.0630f;
        m_meltK[zi] = 0.0018f;
    }

    // Allocate large buffers in PSRAM
#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<PsramData*>(heap_caps_malloc(sizeof(PsramData), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            LW_LOGE("LGPRDTriangleAREffect: PSRAM alloc failed (%u bytes)", (unsigned)sizeof(PsramData));
            return false;
        }
    }
#endif

    const int mid = STRIP_LENGTH / 2;
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
        // Initialise reaction-diffusion fields for every zone slot.
        for (int i = 0; i < STRIP_LENGTH; i++) {
            u[i] = 1.0f;
            v[i] = 0.0f;
            u2[i] = 1.0f;
            v2[i] = 0.0f;
        }

        // Seed centre with V.
        for (int i = mid - 6; i <= mid + 6; i++) {
            if (i >= 0 && i < STRIP_LENGTH) {
                v[i] = 1.0f;
                u[i] = 0.0f;
                v2[i] = 1.0f;
                u2[i] = 0.0f;
            }
        }
    }

    lightwaveos::effects::cinema::reset();
    return true;
}

// =========================================================================
// render() -- direct audio-reactive composition
// =========================================================================

void LGPRDTriangleAREffect::render(plugins::EffectContext& ctx) {
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

    // Beat modulation
    const float beatMod = 0.3f + 0.7f * beatStr;

    // =================================================================
    // STRUCTURE LAYER MODULATES GRAY-SCOTT PARAMETERS
    // =================================================================

    // Use normBass + normTreble to drive F/K (replacing old structure layer)
    const float structureProxy = clamp01(0.5f * normBass + 0.5f * normTreble);

    m_F[z] = clampf(
        0.0380f + 0.0080f * (structureProxy - 0.5f),
        0.0300f, 0.0500f);
    m_K[z] = clampf(
        0.0630f + 0.0100f * (structureProxy - 0.5f),
        0.0550f, 0.0750f);

    // meltK: centre-glue strength modulated by bass energy
    m_meltK[z] = clampf(
        0.0018f + 0.0008f * structureProxy,
        0.0010f, 0.0035f);

    // =================================================================
    // GRAY-SCOTT REACTION-DIFFUSION STEP
    // =================================================================

    const float Du = 1.0f;
    const float Dv = 0.5f;
    const float rdDt = clampf((0.9f + 0.6f * speedNorm) * (0.7f + 0.6f * normBass), 0.6f, 2.2f);
    const int iters = (speedNorm > 0.55f) ? 2 : 1;

    for (int iter = 0; iter < iters; iter++) {
        for (int i = 0; i < STRIP_LENGTH; i++) {
            const int im1 = (i == 0) ? 0 : (i - 1);
            const int ip1 = (i == STRIP_LENGTH - 1) ? (STRIP_LENGTH - 1) : (i + 1);

            const float lapU = u[im1] - 2.0f * u[i] + u[ip1];
            const float lapV = v[im1] - 2.0f * v[i] + v[ip1];

            const float uVal = u[i];
            const float vVal = v[i];
            const float uvv = uVal * vVal * vVal;

            u2[i] = uVal + (Du * lapU - uvv + m_F[z] * (1.0f - uVal)) * rdDt;
            v2[i] = vVal + (Dv * lapV + uvv - (m_K[z] + m_F[z]) * vVal) * rdDt;

            u2[i] = clamp01(u2[i]);
            v2[i] = clamp01(v2[i]);
        }

        // Swap buffers
        for (int i = 0; i < STRIP_LENGTH; i++) {
            u[i] = u2[i];
            v[i] = v2[i];
        }
    }

    // Impact layer: inject V at centre on beat
    if (m_impact[z] > 0.05f) {
        const int mid = STRIP_LENGTH / 2;
        const int radius = 8;
        for (int i = mid - radius; i <= mid + radius; i++) {
            if (i >= 0 && i < STRIP_LENGTH) {
                const float dist = fabsf(static_cast<float>(i - mid)) / static_cast<float>(radius);
                const float inject = m_impact[z] * (1.0f - dist) * 0.4f;
                v[i] = fminf(v[i] + inject, 1.0f);
                u[i] = fmaxf(u[i] - inject * 0.5f, 0.0f);
            }
        }
    }

    // =================================================================
    // PER-PIXEL RENDER
    // =================================================================

    const float mid = (STRIP_LENGTH - 1) * 0.5f;

    // Hue from chroma
    uint8_t baseHue = static_cast<uint8_t>(m_chromaAngle[z] * (255.0f / kTwoPi)) + ctx.gHue;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float x = static_cast<float>(i);
        const float dmid = x - mid;
        const float melt = expf(-(dmid * dmid) * m_meltK[z]);

        const float vSample = v[i];

        // Geometry: V concentration x centre-glue
        float structuredV = vSample * melt;

        // Impact: additive spike at centre
        const float impactDist = fabsf(dmid) / mid;
        const float impactAdd = m_impact[z] * (1.0f - impactDist) * 0.25f;

        // Compose: geometry * normBass * silScale * beatMod
        float brightness = (structuredV * normBass + impactAdd) * beatMod * silScale;

        // Squared for punch
        brightness *= brightness;

        uint8_t val = static_cast<uint8_t>(clamp01(brightness) * 255.0f);
        val = scale8(val, ctx.brightness);

        // Tonal hue: chroma anchor + spatial offset + V modulation
        const float dist = static_cast<float>(centerPairDistance(static_cast<uint16_t>(i)));
        uint8_t hue = baseHue + static_cast<uint8_t>(dist * 0.6f + vSample * 180.0f);

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, val));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

// =========================================================================
// cleanup / metadata / parameters
// =========================================================================

void LGPRDTriangleAREffect::cleanup() {
    // Retain PSRAM across effect lifetime; freeing on cleanup() fragments PSRAM under rapid cycling
    // and forces a fresh malloc on next init(). init() already guards with if (!m_ps).
}

const plugins::EffectMetadata& LGPRDTriangleAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP RD Triangle (5L-AR)",
        "Reaction-diffusion with direct audio-reactive composition",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPRDTriangleAREffect::getParameterCount() const { return 0; }
const plugins::EffectParameter* LGPRDTriangleAREffect::getParameter(uint8_t) const { return nullptr; }
bool LGPRDTriangleAREffect::setParameter(const char*, float) { return false; }
float LGPRDTriangleAREffect::getParameter(const char*) const { return 0.0f; }

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
