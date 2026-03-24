/**
 * @file LGPLorenzRibbonAREffect.cpp
 * @brief Lorenz Ribbon (5-Layer AR) — REWRITTEN
 *
 * Lorenz attractor trail projected radially with direct audio-reactive composition.
 * Base maths from LGPLorenzRibbonEffect preserved exactly.
 *
 * Divergence fixes: Direct ControlBus reads, single-stage smoothing (50ms),
 * asymmetric max follower (58ms/500ms, floor 0.04), no brightness floors,
 * beatStrength() continuous, squared brightness for punch.
 *
 * Physics: Classic Lorenz system (sigma=10, rho=28, beta=8/3)
 *   96-sample trail with age-dependent fade, projected as ribbon thickness
 *
 * Centre-origin compliant. Dual-strip locked.
 */

#include "LGPLorenzRibbonAREffect.h"
#include "LGPFilmPost.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#endif

#include "../../utils/Log.h"

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

static inline float distN_from_index(int i) {
    const float mid = (STRIP_LENGTH - 1) * 0.5f;
    return fabsf((float)i - mid) / mid;
}

static inline float centreGlue(int i) {
    const float mid = (STRIP_LENGTH - 1) * 0.5f;
    const float d = (float)i - mid;
    return 0.30f + 0.70f * expf(-(d * d) * 0.0016f);
}

// Rational approximation to exp(-x), accurate enough for ribbon falloff.
static inline float fastExpFalloff(float x) {
    const float xClamped = (x < 0.0f) ? 0.0f : x;
    return 1.0f / (1.0f + xClamped + 0.48f * xClamped * xClamped);
}

// =========================================================================
// Construction / init / cleanup
// =========================================================================

LGPLorenzRibbonAREffect::LGPLorenzRibbonAREffect()
    : m_ps(nullptr)
    , m_x(1.0f), m_y(0.0f), m_z(0.0f), m_t(0.0f), m_head(0)
    , m_bass(0.0f), m_treble(0.0f), m_chromaAngle(0.0f)
    , m_bassMax(0.15f), m_trebleMax(0.15f)
    , m_impact(0.0f)
{
}

bool LGPLorenzRibbonAREffect::init(plugins::EffectContext& ctx) {
    m_x = 1.0f; m_y = 0.0f; m_z = 0.0f;
    m_t = 0.0f; m_head = 0;
    m_bass = 0.0f; m_treble = 0.0f; m_chromaAngle = 0.0f;
    m_bassMax = 0.15f; m_trebleMax = 0.15f;
    m_impact = 0.0f;

    lightwaveos::effects::cinema::reset();

#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<LorenzPsram*>(heap_caps_malloc(sizeof(LorenzPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            LW_LOGE("LGPLorenzRibbonAREffect: PSRAM alloc failed");
            return false;
        }
    }
    memset(m_ps, 0, sizeof(LorenzPsram));
#endif

    return true;
}

// =========================================================================
// render() — direct audio-reactive composition
// =========================================================================

void LGPLorenzRibbonAREffect::render(plugins::EffectContext& ctx) {
#ifndef NATIVE_BUILD
    if (!m_ps) return;
#endif

    const float dt = ctx.getSafeRawDeltaSeconds();
    const float dtVis = ctx.getSafeDeltaSeconds();
    (void)dtVis;
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

    const float beatMod = 0.3f + 0.7f * beatStr;

    // =================================================================
    // LORENZ ATTRACTOR PHYSICS (PRESERVED EXACTLY)
    // =================================================================

    const float sigma = 10.0f;
    const float rho   = 28.0f;
    const float beta  = 8.0f / 3.0f;

    // Structure modulates integration speed and sub-step count
    // normBass + normTreble replace m_structure; normBass + m_impact replace mod.motionRate/motionDepth
    const float structureDrive = clamp01(0.30f + 0.45f * normBass + 0.25f * normTreble);
    const float motionRate = 0.6f + 0.8f * normBass + 0.3f * m_impact;
    const float motionDepth = 0.5f + 0.5f * normBass;

    float dtL = (0.0065f + 0.010f * speedNorm) * (0.7f + 0.5f * structureDrive) * motionRate;
    int sub = 2 + static_cast<int>(3.5f * speedNorm * (0.6f + 0.4f * structureDrive)
                                    * (0.75f + 0.50f * motionDepth));
    if (sub > 7) sub = 7;

#ifndef NATIVE_BUILD
    for (int s = 0; s < sub; s++) {
        float dx = sigma * (m_y - m_x);
        float dy = m_x * (rho - m_z) - m_y;
        float dz = m_x * m_y - beta * m_z;

        m_x += dx * dtL;
        m_y += dy * dtL;
        m_z += dz * dtL;
        m_t += dtL;

        float r = (0.55f * fabsf(m_x) / 22.0f) + (0.45f * m_z / 55.0f);
        r = clamp01(r);

        m_ps->trail[m_head] = r;
        m_head = (uint8_t)((m_head + 1) % TRAIL_N);
    }
#endif

    // =================================================================
    // PER-PIXEL RENDER
    // =================================================================

    // Structure modulates ribbon thickness
    const float thickBase = 0.040f + 0.030f * (1.0f - speedNorm);
    const float thick = clampf(
        thickBase * (0.65f + 0.55f * structureDrive) * (0.75f + 0.50f * motionDepth),
        0.020f, 0.140f);

    const int trailStride = (speedNorm > 1.50f) ? 3 : ((speedNorm > 0.95f) ? 2 : 1);
    static float trailR[TRAIL_N];
    static float trailFade[TRAIL_N];
    int trailSamples = 0;
    for (int k = 0; k < TRAIL_N; k += trailStride) {
        int idx = static_cast<int>(m_head) - 1 - k;
        while (idx < 0) idx += TRAIL_N;
        const float age = static_cast<float>(k) / static_cast<float>(TRAIL_N - 1);
        float fade = 1.0f - age;
        fade = fade * fade * (0.70f + 0.30f * fade);
        if (fade < 0.02f) {
            break;
        }
        trailR[trailSamples] = m_ps->trail[idx];
        trailFade[trailSamples] = fade;
        ++trailSamples;
    }

    // Hue from chroma
    uint8_t baseHue = static_cast<uint8_t>(m_chromaAngle * (255.0f / kTwoPi)) + ctx.gHue;

#ifndef NATIVE_BUILD
    for (int i = 0; i < STRIP_LENGTH; i++) {
        float dn = distN_from_index(i);
        float glue = centreGlue(i);
        float best = 0.0f;

        for (int k = 0; k < trailSamples; k++) {
            const float g = fastExpFalloff(fabsf(dn - trailR[k]) / thick) * trailFade[k];
            best = fmaxf(best, g);
        }

        // Thin specular edge for ribbon feel
        float spec = best * best * sqrtf(best) * 0.55f;

        // Ribbon geometry: proximity to trail + specular
        float ribbonGeom = clamp01((0.22f + 0.78f * best + spec) * glue);

        // Impact: additive trajectory burst
        float impactAdd = m_impact * best * glue * 0.30f;

        // Compose brightness: geometry x normBass x silScale x beatMod
        float brightness = (ribbonGeom * normBass + impactAdd) * beatMod * silScale;

        // Squared for punch
        brightness *= brightness;

        uint8_t val = static_cast<uint8_t>(clamp01(brightness) * 255.0f);
        val = scale8(val, ctx.brightness);

        // Tonal: chaotic energy drives hue with chroma anchor
        float energy = clamp01(
            (fabsf(m_x) / 22.0f) * 0.6f + (fabsf(m_y) / 28.0f) * 0.4f);
        float hueShift = 25.0f * energy + 45.0f * spec;
        uint8_t hue = baseHue + static_cast<uint8_t>(hueShift);

        writeDualLocked(ctx, i, CHSV(hue, ctx.saturation, val));
    }
#endif

    lightwaveos::effects::cinema::apply(ctx, speedNorm, lightwaveos::effects::cinema::QualityPreset::LIGHTWEIGHT);
}

// =========================================================================
// cleanup / metadata / parameters
// =========================================================================

void LGPLorenzRibbonAREffect::cleanup() {
#ifndef NATIVE_BUILD
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
#endif
}

const plugins::EffectMetadata& LGPLorenzRibbonAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Lorenz Ribbon (5L-AR)",
        "Chaotic attractor ribbon with direct audio-reactive composition",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPLorenzRibbonAREffect::getParameterCount() const { return 0; }
const plugins::EffectParameter* LGPLorenzRibbonAREffect::getParameter(uint8_t) const { return nullptr; }
bool LGPLorenzRibbonAREffect::setParameter(const char*, float) { return false; }
float LGPLorenzRibbonAREffect::getParameter(const char*) const { return 0.0f; }

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
