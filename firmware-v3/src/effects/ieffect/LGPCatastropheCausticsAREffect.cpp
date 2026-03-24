/**
 * @file LGPCatastropheCausticsAREffect.cpp
 * @brief Catastrophe Caustics (5-Layer AR) -- REWRITTEN
 *
 * Ray-envelope histogram with catastrophe optics. Lens thickness field h(x,t)
 * with 3 sinusoidal terms + centre bias. Rays deflect by dh/dx, land at X=x+z*a.
 * Histogram accumulation + decay. Cusp detection via Laplacian.
 *
 * Divergence fixes: Direct ControlBus reads, single-stage smoothing (50ms),
 * asymmetric max follower (58ms/500ms, floor 0.04), no brightness floors,
 * beatStrength() continuous, squared brightness for punch.
 *
 * Centre-origin compliant. Dual-strip locked.
 */

#include "LGPCatastropheCausticsAREffect.h"
#include "LGPFilmPost.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

// =========================================================================
// Constants
// =========================================================================

static constexpr float kTwoPi = 6.28318530717958647692f;
static constexpr float kPi    = 3.14159265358979323846f;
static constexpr float kInv160 = 1.0f / 160.0f;
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
// Construction / destruction / init / cleanup
// =========================================================================

LGPCatastropheCausticsAREffect::LGPCatastropheCausticsAREffect()
    : m_t(0.0f)
    , m_bass(0.0f), m_treble(0.0f), m_chromaAngle(0.0f)
    , m_bassMax(0.15f), m_trebleMax(0.15f)
    , m_impact(0.0f)
#ifdef NATIVE_BUILD
    , m_I{}
#endif
{
}

LGPCatastropheCausticsAREffect::~LGPCatastropheCausticsAREffect() {
#ifndef NATIVE_BUILD
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
#endif
}

bool LGPCatastropheCausticsAREffect::init(plugins::EffectContext& ctx) {
#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<CausticsPsram*>(
            heap_caps_malloc(sizeof(CausticsPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) return false;
    }
    for (int i = 0; i < 160; i++) m_ps->I[i] = 0.0f;
#else
    for (int i = 0; i < 160; i++) m_I[i] = 0.0f;
#endif

    m_t = 0.0f;
    m_bass = 0.0f; m_treble = 0.0f; m_chromaAngle = 0.0f;
    m_bassMax = 0.15f; m_trebleMax = 0.15f;
    m_impact = 0.0f;
    lightwaveos::effects::cinema::reset();
    return true;
}

void LGPCatastropheCausticsAREffect::cleanup() {
#ifndef NATIVE_BUILD
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
#endif
}

// =========================================================================
// render() -- direct audio-reactive composition
// =========================================================================

void LGPCatastropheCausticsAREffect::render(plugins::EffectContext& ctx) {
#ifndef NATIVE_BUILD
    if (!m_ps) return;
    float* I = m_ps->I;
#else
    float* I = m_I;
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
    // MOTION
    // =================================================================

    const float tRate = (0.80f + 3.50f * speedNorm) * (0.7f + 0.6f * normBass)
                        * (0.60f + 0.40f * normTreble);
    m_t += tRate * dtVis;

    // =================================================================
    // CAUSTIC PHYSICS LAYER PARAMS
    // =================================================================

    const float mid = (STRIP_LENGTH - 1) * 0.5f;

    // Focus depth z: treble modulates convergence, impact defocuses
    const float focusZ = clampf(
        1.8f + 0.8f * normTreble - 0.4f * m_impact, 1.2f, 3.0f);

    // Lens wave amplitudes (3 sinusoidal terms)
    const float A1 = 0.18f * (0.80f + 0.20f * normTreble);
    const float A2 = 0.12f * (0.70f + 0.30f * normBass);
    const float A3 = 0.09f * (0.75f + 0.25f * normTreble);

    // Centre bias: pulls rays towards centre LED
    const float centreBias = 0.22f * (0.80f + 0.20f * normBass);

    // =================================================================
    // RAY-ENVELOPE HISTOGRAM
    // =================================================================

    // Decay intensity accumulator (frame-rate independent)
    const float decayRate = expf(-dtVis * 2.2f);
    for (int i = 0; i < 160; i++) {
        I[i] *= decayRate;
    }

    // Lens thickness field h(x,t) = sum of 3 sinusoids + centre bias
    // Rays launched from strip positions, deflected by dh/dx gradient
    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float x = static_cast<float>(i) - mid;
        const float xN = x * kInv160;  // Normalised position [-0.5, 0.5]

        // Lens thickness: 3 travelling waves + centre bias
        const float h = A1 * sinf(kTwoPi * (xN * 2.5f - m_t * 0.30f))
                      + A2 * sinf(kTwoPi * (xN * 4.0f + m_t * 0.18f))
                      + A3 * sinf(kTwoPi * (xN * 6.5f - m_t * 0.25f))
                      - centreBias * (xN * xN);

        // Lens thickness at offset position for gradient (central difference)
        const float dx = 0.5f;
        const float xN_left  = (x - dx) * kInv160;
        const float xN_right = (x + dx) * kInv160;

        const float h_left = A1 * sinf(kTwoPi * (xN_left * 2.5f - m_t * 0.30f))
                           + A2 * sinf(kTwoPi * (xN_left * 4.0f + m_t * 0.18f))
                           + A3 * sinf(kTwoPi * (xN_left * 6.5f - m_t * 0.25f))
                           - centreBias * (xN_left * xN_left);

        const float h_right = A1 * sinf(kTwoPi * (xN_right * 2.5f - m_t * 0.30f))
                            + A2 * sinf(kTwoPi * (xN_right * 4.0f + m_t * 0.18f))
                            + A3 * sinf(kTwoPi * (xN_right * 6.5f - m_t * 0.25f))
                            - centreBias * (xN_right * xN_right);

        // Gradient: dh/dx = (h_right - h_left) / (2*dx)
        const float dhdx = (h_right - h_left) / (2.0f * dx);

        // Ray deflection angle a = -dh/dx
        const float angle = -dhdx;

        // Ray landing position X = x + z*a (projected to screen at distance z)
        const float X = x + focusZ * angle;

        // Convert back to LED index
        const int targetIdx = static_cast<int>(X + mid + 0.5f);

        // Accumulate intensity if in bounds
        if (targetIdx >= 0 && targetIdx < 160) {
            I[targetIdx] += 0.25f;
        }
    }

    // =================================================================
    // CUSP DETECTION (Laplacian)
    // =================================================================

    float cusps[160];
    for (int i = 0; i < 160; i++) cusps[i] = 0.0f;

    for (int i = 1; i < 159; i++) {
        // Laplacian: d^2I/dx^2 = I[i-1] - 2*I[i] + I[i+1]
        const float laplacian = I[i-1] - 2.0f * I[i] + I[i+1];
        // Positive laplacian = local concave (potential caustic cusp)
        if (laplacian > 0.0f) {
            cusps[i] = clamp01(laplacian * 0.35f);
        }
    }

    // =================================================================
    // PER-PIXEL RENDER
    // =================================================================

    // Hue from chroma
    uint8_t baseHue = static_cast<uint8_t>(m_chromaAngle * (255.0f / kTwoPi)) + ctx.gHue;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        // Caustic intensity (clamped, normalised)
        float caustic = clamp01(I[i] * 0.20f);

        // Cusp intensity
        float cusp = cusps[i];

        // Centre glue: fade edges
        const float dmid = static_cast<float>(i) - mid;
        const float glue = 0.30f + 0.70f * expf(-(dmid * dmid) * 0.0010f);

        // Geometry: (caustic + cusps) x glue
        float structuredCaustic = (caustic + 0.5f * cusp) * glue;

        // Impact: additive spike at cusps
        float impactAdd = m_impact * cusp * 0.40f;

        // Compose: geometry * normBass * silScale * beatMod
        float brightness = (structuredCaustic * normBass + impactAdd) * beatMod * silScale;

        // Squared for punch
        brightness *= brightness;

        uint8_t val = static_cast<uint8_t>(clamp01(brightness) * 255.0f);
        val = scale8(val, ctx.brightness);

        // Tonal hue: chroma anchor + caustic offset
        uint8_t hue = baseHue + static_cast<uint8_t>(caustic * 48.0f + cusp * 35.0f);

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, val));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

// =========================================================================
// metadata / parameters
// =========================================================================

const plugins::EffectMetadata& LGPCatastropheCausticsAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Catastrophe Caustics (5L-AR)",
        "Ray-envelope caustics with direct audio-reactive composition",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPCatastropheCausticsAREffect::getParameterCount() const { return 0; }
const plugins::EffectParameter* LGPCatastropheCausticsAREffect::getParameter(uint8_t) const { return nullptr; }
bool LGPCatastropheCausticsAREffect::setParameter(const char*, float) { return false; }
float LGPCatastropheCausticsAREffect::getParameter(const char*) const { return 0.0f; }

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
