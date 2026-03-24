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

LGPRDTriangleAREffect::LGPRDTriangleAREffect()
    : m_ps(nullptr)
    , m_bass(0.0f), m_treble(0.0f), m_chromaAngle(0.0f)
    , m_bassMax(0.15f), m_trebleMax(0.15f)
    , m_impact(0.0f)
    , m_F(0.0380f), m_K(0.0630f), m_meltK(0.0018f)
{
}

bool LGPRDTriangleAREffect::init(plugins::EffectContext& ctx) {
    m_bass = 0.0f; m_treble = 0.0f; m_chromaAngle = 0.0f;
    m_bassMax = 0.15f; m_trebleMax = 0.15f;
    m_impact = 0.0f;
    m_F = 0.0380f; m_K = 0.0630f; m_meltK = 0.0018f;

    // Allocate large buffers in PSRAM
    if (!m_ps) {
        m_ps = static_cast<PsramData*>(heap_caps_malloc(sizeof(PsramData), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            LW_LOGE("LGPRDTriangleAREffect: PSRAM alloc failed (%u bytes)", (unsigned)sizeof(PsramData));
            return false;
        }
    }

    // Initialise reaction-diffusion fields
    for (int i = 0; i < STRIP_LENGTH; i++) {
        m_ps->u[i] = 1.0f;
        m_ps->v[i] = 0.0f;
    }

    // Seed centre with V
    const int mid = STRIP_LENGTH / 2;
    for (int i = mid - 6; i <= mid + 6; i++) {
        if (i >= 0 && i < STRIP_LENGTH) {
            m_ps->v[i] = 1.0f;
            m_ps->u[i] = 0.0f;
        }
    }

    lightwaveos::effects::cinema::reset();
    return true;
}

// =========================================================================
// render() -- direct audio-reactive composition
// =========================================================================

void LGPRDTriangleAREffect::render(plugins::EffectContext& ctx) {
    if (!m_ps) return;

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

    // Beat modulation
    const float beatMod = 0.3f + 0.7f * beatStr;

    // =================================================================
    // STRUCTURE LAYER MODULATES GRAY-SCOTT PARAMETERS
    // =================================================================

    // Use normBass + normTreble to drive F/K (replacing old structure layer)
    const float structureProxy = clamp01(0.5f * normBass + 0.5f * normTreble);

    m_F = clampf(
        0.0380f + 0.0080f * (structureProxy - 0.5f),
        0.0300f, 0.0500f);
    m_K = clampf(
        0.0630f + 0.0100f * (structureProxy - 0.5f),
        0.0550f, 0.0750f);

    // meltK: centre-glue strength modulated by bass energy
    m_meltK = clampf(
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

            const float lapU = m_ps->u[im1] - 2.0f * m_ps->u[i] + m_ps->u[ip1];
            const float lapV = m_ps->v[im1] - 2.0f * m_ps->v[i] + m_ps->v[ip1];

            const float u = m_ps->u[i];
            const float v = m_ps->v[i];
            const float uvv = u * v * v;

            m_ps->u2[i] = u + (Du * lapU - uvv + m_F * (1.0f - u)) * rdDt;
            m_ps->v2[i] = v + (Dv * lapV + uvv - (m_K + m_F) * v) * rdDt;

            m_ps->u2[i] = clamp01(m_ps->u2[i]);
            m_ps->v2[i] = clamp01(m_ps->v2[i]);
        }

        // Swap buffers
        for (int i = 0; i < STRIP_LENGTH; i++) {
            m_ps->u[i] = m_ps->u2[i];
            m_ps->v[i] = m_ps->v2[i];
        }
    }

    // Impact layer: inject V at centre on beat
    if (m_impact > 0.05f) {
        const int mid = STRIP_LENGTH / 2;
        const int radius = 8;
        for (int i = mid - radius; i <= mid + radius; i++) {
            if (i >= 0 && i < STRIP_LENGTH) {
                const float dist = fabsf(static_cast<float>(i - mid)) / static_cast<float>(radius);
                const float inject = m_impact * (1.0f - dist) * 0.4f;
                m_ps->v[i] = fminf(m_ps->v[i] + inject, 1.0f);
                m_ps->u[i] = fmaxf(m_ps->u[i] - inject * 0.5f, 0.0f);
            }
        }
    }

    // =================================================================
    // PER-PIXEL RENDER
    // =================================================================

    const float mid = (STRIP_LENGTH - 1) * 0.5f;

    // Hue from chroma
    uint8_t baseHue = static_cast<uint8_t>(m_chromaAngle * (255.0f / kTwoPi)) + ctx.gHue;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float x = static_cast<float>(i);
        const float dmid = x - mid;
        const float melt = expf(-(dmid * dmid) * m_meltK);

        const float v = m_ps->v[i];

        // Geometry: V concentration x centre-glue
        float structuredV = v * melt;

        // Impact: additive spike at centre
        const float impactDist = fabsf(dmid) / mid;
        const float impactAdd = m_impact * (1.0f - impactDist) * 0.25f;

        // Compose: geometry * normBass * silScale * beatMod
        float brightness = (structuredV * normBass + impactAdd) * beatMod * silScale;

        // Squared for punch
        brightness *= brightness;

        uint8_t val = static_cast<uint8_t>(clamp01(brightness) * 255.0f);
        val = scale8(val, ctx.brightness);

        // Tonal hue: chroma anchor + spatial offset + V modulation
        const float dist = static_cast<float>(centerPairDistance(static_cast<uint16_t>(i)));
        uint8_t hue = baseHue + static_cast<uint8_t>(dist * 0.6f + v * 180.0f);

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, val));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

// =========================================================================
// cleanup / metadata / parameters
// =========================================================================

void LGPRDTriangleAREffect::cleanup() {
    if (m_ps) { heap_caps_free(m_ps); m_ps = nullptr; }
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
