/**
 * @file LGPCatastropheCausticsAREffect.cpp
 * @brief Catastrophe Caustics (5-Layer Audio-Reactive)
 *
 * Ray-envelope histogram with catastrophe optics. Lens thickness field h(x,t)
 * with 3 sinusoidal terms + centre bias. Rays deflect by dh/dx, land at X=x+z*a.
 * Histogram accumulation + decay. Cusp detection via Laplacian.
 *
 * Full 5-layer AR composition: bed, structure, impact, memory, tonal.
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
// Local helpers
// =========================================================================

static constexpr float kTau = 6.28318530717958647692f;
static constexpr float kInv160 = 1.0f / 160.0f;

static inline float fract(float x) { return x - floorf(x); }

static inline void writeDualLocked(plugins::EffectContext& ctx, int i, const CRGB& c) {
    ctx.leds[i] = c;
    int j = i + STRIP_LENGTH;
    if (j < (int)ctx.ledCount) ctx.leds[j] = c;
}

// Filmic brightness mapping (base + soft-sat curve)
static inline uint8_t waveToBr(float wave01, float master) {
    const float base = 0.04f;
    float out = lowrisk_ar::clamp01f(base + (1.0f - base) * lowrisk_ar::clamp01f(wave01)) * master;
    return static_cast<uint8_t>(255.0f * out);
}

// =========================================================================
// Construction / destruction / init / cleanup
// =========================================================================

LGPCatastropheCausticsAREffect::LGPCatastropheCausticsAREffect()
    : m_t(0.0f)
    , m_bed(0.3f)
    , m_structure(0.5f)
    , m_impact(0.0f)
    , m_memory(0.0f)
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

    m_controls.resetDefaults();
    m_ar = lowrisk_ar::ArRuntimeState{};
    m_ar.tonalHue = static_cast<float>(ctx.gHue);
    m_ar.modeSmooth = 2.0f;
    m_t           = 0.0f;
    m_bed         = 0.3f;
    m_structure   = 0.5f;
    m_impact      = 0.0f;
    m_memory      = 0.0f;
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
// render() — 5-Layer composition
// =========================================================================

void LGPCatastropheCausticsAREffect::render(plugins::EffectContext& ctx) {
#ifndef NATIVE_BUILD
    if (!m_ps) return;
    float* I = m_ps->I;
#else
    float* I = m_I;
#endif

    // --- Timing ---
    const float dtSignal = AudioReactivePolicy::signalDt(ctx);
    const float dtVisual = AudioReactivePolicy::visualDt(ctx);
    const float speedNorm = ctx.speed / 50.0f;

    // --- Signals (shared infrastructure handles presence gate + fallback) ---
    const lowrisk_ar::ArSignalSnapshot sig =
        lowrisk_ar::updateSignals(m_ar, ctx, m_controls, dtSignal);

    // =================================================================
    // LAYER UPDATES
    // =================================================================

    // Bed: RMS-driven caustic brightness (tau 0.42s)
    m_bed = lowrisk_ar::smoothTo(m_bed,
        0.20f + 0.80f * sig.rms, dtSignal, 0.42f);

    // Structure: focus z + strength from mid+flux (tau 0.14s)
    const float structTarget = lowrisk_ar::clamp01f(
        0.30f + 0.40f * sig.mid + 0.20f * sig.flux + 0.10f * sig.rhythmic);
    m_structure = lowrisk_ar::smoothTo(m_structure, structTarget, dtSignal, 0.14f);

    // Impact: beat-triggered burst (decay 0.20s)
    if (sig.beatTick) m_impact = fmaxf(m_impact, 0.85f);
    m_impact = lowrisk_ar::decay(m_impact, dtSignal, 0.20f);

    // Memory: caustic persistence (slow decay, fed by beat + flux)
    m_memory = lowrisk_ar::clamp01f(
        lowrisk_ar::decay(m_memory, dtSignal, 0.85f)
        + 0.10f * m_impact + 0.08f * sig.flux);

    // =================================================================
    // MOTION
    // =================================================================

    const float tRate = (0.80f + 3.50f * speedNorm) * m_controls.motionRate()
                        * (0.60f + 0.40f * m_structure);
    m_t += tRate * dtVisual;

    // =================================================================
    // CAUSTIC PHYSICS LAYER PARAMS
    // =================================================================

    const float mid = (STRIP_LENGTH - 1) * 0.5f;

    // Focus depth z: structure modulates how sharply rays converge
    const float focusZ = lowrisk_ar::clampf(
        1.8f + 0.8f * m_structure - 0.4f * m_impact, 1.2f, 3.0f);

    // Lens wave amplitudes (3 sinusoidal terms)
    const float A1 = 0.18f * (0.80f + 0.20f * m_structure);
    const float A2 = 0.12f * (0.70f + 0.30f * sig.flux);
    const float A3 = 0.09f * (0.75f + 0.25f * sig.harmonic);

    // Centre bias: pulls rays towards centre LED
    const float centreBias = 0.22f * (0.80f + 0.20f * m_bed);

    // =================================================================
    // RAY-ENVELOPE HISTOGRAM
    // =================================================================

    // Decay intensity accumulator (frame-rate independent)
    const float decayRate = expf(-dtVisual * 2.2f);
    for (int i = 0; i < 160; i++) {
        I[i] *= decayRate;
    }

    // Lens thickness field h(x,t) = sum of 3 sinusoids + centre bias
    // Rays launched from strip positions, deflected by dh/dx gradient
    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float x = static_cast<float>(i) - mid;
        const float xN = x * kInv160;  // Normalised position [-0.5, 0.5]

        // Lens thickness: 3 travelling waves + centre bias
        const float h = A1 * sinf(kTau * (xN * 2.5f - m_t * 0.30f))
                      + A2 * sinf(kTau * (xN * 4.0f + m_t * 0.18f))
                      + A3 * sinf(kTau * (xN * 6.5f - m_t * 0.25f))
                      - centreBias * (xN * xN);

        // Lens thickness at offset position for gradient (central difference)
        const float dx = 0.5f;
        const float xN_left  = (x - dx) * kInv160;
        const float xN_right = (x + dx) * kInv160;

        const float h_left = A1 * sinf(kTau * (xN_left * 2.5f - m_t * 0.30f))
                           + A2 * sinf(kTau * (xN_left * 4.0f + m_t * 0.18f))
                           + A3 * sinf(kTau * (xN_left * 6.5f - m_t * 0.25f))
                           - centreBias * (xN_left * xN_left);

        const float h_right = A1 * sinf(kTau * (xN_right * 2.5f - m_t * 0.30f))
                            + A2 * sinf(kTau * (xN_right * 4.0f + m_t * 0.18f))
                            + A3 * sinf(kTau * (xN_right * 6.5f - m_t * 0.25f))
                            - centreBias * (xN_right * xN_right);

        // Gradient: dh/dx ≈ (h_right - h_left) / (2*dx)
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
        // Laplacian: d²I/dx² ≈ I[i-1] - 2*I[i] + I[i+1]
        const float laplacian = I[i-1] - 2.0f * I[i] + I[i+1];
        // Positive laplacian = local concave (potential caustic cusp)
        if (laplacian > 0.0f) {
            cusps[i] = lowrisk_ar::clamp01f(laplacian * 0.35f);
        }
    }

    // =================================================================
    // PER-PIXEL RENDER
    // =================================================================

    const float bedBright = 0.20f + 0.80f * m_bed;
    const float master = (ctx.brightness / 255.0f)
                         * lowrisk_ar::clamp01f(m_ar.audioPresence);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        // Caustic intensity (clamped, normalised)
        float caustic = lowrisk_ar::clamp01f(I[i] * 0.20f);

        // Cusp intensity
        float cusp = cusps[i];

        // Centre glue: fade edges
        const float dmid = static_cast<float>(i) - mid;
        const float glue = 0.30f + 0.70f * expf(-(dmid * dmid) * 0.0010f);

        // Bed × (caustic + cusps)
        float structuredCaustic = (caustic + 0.5f * cusp) * glue;

        // Impact: additive spike at cusps
        float impactAdd = m_impact * cusp * 0.40f;

        // Memory: gentle additive glow
        float memoryAdd = m_memory * (0.4f + 0.6f * caustic) * 0.15f;

        // Compose: bed × structure + impact + memory
        float brightness = bedBright * structuredCaustic + impactAdd + memoryAdd;
        brightness = lowrisk_ar::clamp01f(brightness);

        const uint8_t br = waveToBr(brightness, master);

        // Tonal hue: chord-driven anchor + caustic offset
        const uint8_t hue = static_cast<uint8_t>(
            m_ar.tonalHue + caustic * 48.0f + cusp * 35.0f + sig.harmonic * 18.0f);

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

// =========================================================================
// metadata / parameters
// =========================================================================

const plugins::EffectMetadata& LGPCatastropheCausticsAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Catastrophe Caustics (5L-AR)",
        "Ray-envelope caustics with 5-layer audio-reactive composition",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPCatastropheCausticsAREffect::getParameterCount() const {
    return lowrisk_ar::Ar16Controls::parameterCount();
}

const plugins::EffectParameter* LGPCatastropheCausticsAREffect::getParameter(uint8_t index) const {
    return lowrisk_ar::Ar16Controls::parameter(index);
}

bool LGPCatastropheCausticsAREffect::setParameter(const char* name, float value) {
    return m_controls.set(name, value);
}

float LGPCatastropheCausticsAREffect::getParameter(const char* name) const {
    return m_controls.get(name);
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
