/**
 * @file LGPSuperformulaGlyphAREffect.cpp
 * @brief Superformula Living Glyph (5-Layer Audio-Reactive)
 *
 * Morphing organic glyph using Superformula mathematics with full 5-layer AR composition.
 * Superformula creates complex organic/geometric hybrids through parametric polar equation.
 * Parameters morph with audio, creating living sigil patterns.
 *
 * Centre-origin compliant. Dual-strip locked.
 */

#include "LGPSuperformulaGlyphAREffect.h"
#include "LGPFilmPost.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

// =========================================================================
// Local helpers
// =========================================================================

static constexpr float kTau = 6.28318530717958647692f;
static constexpr float kPi  = 3.14159265358979323846f;

static inline float fract(float x) { return x - floorf(x); }

static inline void writeDualLocked(plugins::EffectContext& ctx, int i, const CRGB& c) {
    ctx.leds[i] = c;
    int j = i + STRIP_LENGTH;
    if (j < (int)ctx.ledCount) ctx.leds[j] = c;
}

static inline uint8_t luminanceToBr(float wave01, float master) {
    const float base = 0.06f;
    float out = lowrisk_ar::clamp01f(base + (1.0f - base) * lowrisk_ar::clamp01f(wave01)) * master;
    return static_cast<uint8_t>(255.0f * out);
}

// =========================================================================
// Superformula evaluation
// =========================================================================

/**
 * Superformula: r(phi) = (|cos(m*phi/4)/a|^n2 + |sin(m*phi/4)/b|^n3)^(-1/n1)
 * Returns normalized radius [0..1] for given angle phi.
 */
static float evalSuperformula(float phi, float m, float n1, float n2, float n3) {
    const float a = 1.0f;  // Keep a=b=1 for simplicity
    const float b = 1.0f;

    const float mPhi4 = m * phi * 0.25f;

    // Prevent domain errors
    const float cosT = fabsf(cosf(mPhi4));
    const float sinT = fabsf(sinf(mPhi4));

    // Handle edge cases
    if (cosT < 1e-6f && sinT < 1e-6f) return 0.5f;

    const float term1 = powf(cosT / a, n2);
    const float term2 = powf(sinT / b, n3);
    const float sum   = term1 + term2;

    if (sum < 1e-6f) return 0.5f;

    float r = powf(sum, -1.0f / n1);

    // Normalize to [0..1] range (superformula can vary wildly)
    return lowrisk_ar::clampf(r * 0.35f, 0.0f, 1.0f);
}

// =========================================================================
// Construction / init / cleanup
// =========================================================================

LGPSuperformulaGlyphAREffect::LGPSuperformulaGlyphAREffect()
    : m_t(0.0f)
    , m_bed(0.3f)
    , m_impact(0.0f)
    , m_memory(0.0f)
    , m_param_m(6.0f)
    , m_param_n1(1.0f)
    , m_param_n2(1.5f)
    , m_param_n3(1.5f)
{
}

bool LGPSuperformulaGlyphAREffect::init(plugins::EffectContext& ctx) {
    m_controls.resetDefaults();
    m_ar = lowrisk_ar::ArRuntimeState{};
    m_ar.tonalHue = static_cast<float>(ctx.gHue);
    m_ar.modeSmooth = 2.0f;

    m_t        = 0.0f;
    m_bed      = 0.3f;
    m_impact   = 0.0f;
    m_memory   = 0.0f;
    m_param_m  = 6.0f;
    m_param_n1 = 1.0f;
    m_param_n2 = 1.5f;
    m_param_n3 = 1.5f;

    lightwaveos::effects::cinema::reset();
    return true;
}

// =========================================================================
// render() — 5-Layer composition
// =========================================================================

void LGPSuperformulaGlyphAREffect::render(plugins::EffectContext& ctx) {
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

    // Bed: slow atmosphere from RMS (tau 0.42s)
    m_bed = lowrisk_ar::smoothTo(m_bed,
        0.20f + 0.80f * sig.rms, dtSignal, 0.42f);

    // Structure: morph superformula params from harmonic + flux (tau 0.20s)
    // m: symmetry (3-11), modulated by harmonic
    const float target_m = lowrisk_ar::clampf(
        6.0f + 5.0f * sig.harmonic * (0.5f + 0.5f * sinf(m_t * 0.3f)),
        3.0f, 11.0f);
    m_param_m = lowrisk_ar::smoothTo(m_param_m, target_m, dtSignal, 0.20f);

    // n1: overall shape (0.7-1.6), modulated by flux
    const float target_n1 = lowrisk_ar::clampf(
        1.0f + 0.6f * sig.flux * cosf(m_t * 0.25f),
        0.7f, 1.6f);
    m_param_n1 = lowrisk_ar::smoothTo(m_param_n1, target_n1, dtSignal, 0.20f);

    // n2/n3: exponents (0.8-2.4), modulated by bass + rhythmic
    const float target_n2 = lowrisk_ar::clampf(
        1.5f + 0.9f * (sig.bass * 0.7f + sig.rhythmic * 0.3f),
        0.8f, 2.4f);
    m_param_n2 = lowrisk_ar::smoothTo(m_param_n2, target_n2, dtSignal, 0.20f);

    const float target_n3 = lowrisk_ar::clampf(
        1.5f + 0.9f * (sig.bass * 0.3f + sig.rhythmic * 0.7f) * sinf(m_t * 0.4f),
        0.8f, 2.4f);
    m_param_n3 = lowrisk_ar::smoothTo(m_param_n3, target_n3, dtSignal, 0.20f);

    // Impact: beat-triggered burst (fast attack, decay 0.18s)
    if (sig.beatTick) m_impact = fmaxf(m_impact, 1.00f);
    m_impact = lowrisk_ar::decay(m_impact, dtSignal, 0.18f);

    // Memory: sigil persistence (slow decay, fed by beat + flux)
    m_memory = lowrisk_ar::clamp01f(
        lowrisk_ar::decay(m_memory, dtSignal, 0.75f)
        + 0.15f * m_impact + 0.08f * sig.flux);

    // =================================================================
    // MOTION
    // =================================================================

    const float tRate = (0.80f + 3.50f * speedNorm) * m_controls.motionRate();
    m_t += tRate * dtVisual;

    // Glyph rotation
    const float glyphRotation = m_t * 0.15f + sig.harmonic * 0.5f;

    // =================================================================
    // PER-PIXEL RENDER
    // =================================================================

    const float mid    = (STRIP_LENGTH - 1) * 0.5f;
    const float invMid = 1.0f / mid;

    const float bedBright = 0.20f + 0.80f * m_bed;
    const float master = (ctx.brightness / 255.0f)
                         * lowrisk_ar::clamp01f(m_ar.audioPresence);

    // Band width for distance-to-curve rendering
    const float bandWidth = lowrisk_ar::clampf(
        0.12f - 0.04f * sig.flux,
        0.06f, 0.16f);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float dmid  = static_cast<float>(i) - mid;
        const float distN = fabsf(dmid) * invMid;  // [0..1] from centre

        // Convert pixel to polar coordinates (centre = origin)
        const float phi = atan2f(dmid, 1.0f) + glyphRotation;  // Angular position

        // Evaluate superformula at this angle
        const float r_formula = evalSuperformula(phi, m_param_m, m_param_n1, m_param_n2, m_param_n3);

        // Distance from pixel to the superformula curve
        const float distToCurve = fabsf(distN - r_formula);

        // Exponential band rendering
        const float bandWave = expf(-distToCurve / bandWidth);

        // Centre glue: stronger gaussian anchor
        const float glue = 0.30f + 0.70f * expf(-(dmid * dmid) * 0.0018f);

        // Breathing pulse
        const float breathing = 0.90f + 0.10f * cosf(kTau * distN * 2.0f + m_t * 0.8f);

        // Bed x structured geometry
        float structuredGeom = bandWave * glue * breathing;

        // Impact: additive brightening across glyph
        float impactAdd = m_impact * bandWave * glue * 0.40f;

        // Memory: gentle additive sigil persistence
        float memoryAdd = m_memory * (0.4f + 0.6f * bandWave) * 0.22f;

        // Compose: bed x structure + impact + memory
        float brightness = bedBright * structuredGeom + impactAdd + memoryAdd;
        brightness = lowrisk_ar::clamp01f(brightness);

        const uint8_t br = luminanceToBr(brightness, master);

        // Tonal hue: chord-driven anchor + spatial modulation
        const uint8_t hue = static_cast<uint8_t>(
            m_ar.tonalHue + r_formula * 60.0f + sig.harmonic * 25.0f);

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

// =========================================================================
// cleanup / metadata / parameters
// =========================================================================

void LGPSuperformulaGlyphAREffect::cleanup() {}

const plugins::EffectMetadata& LGPSuperformulaGlyphAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Superformula Glyph (5L-AR)",
        "Living organic glyph with superformula morphing and 5-layer audio-reactive composition",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPSuperformulaGlyphAREffect::getParameterCount() const {
    return lowrisk_ar::Ar16Controls::parameterCount();
}

const plugins::EffectParameter* LGPSuperformulaGlyphAREffect::getParameter(uint8_t index) const {
    return lowrisk_ar::Ar16Controls::parameter(index);
}

bool LGPSuperformulaGlyphAREffect::setParameter(const char* name, float value) {
    return m_controls.set(name, value);
}

float LGPSuperformulaGlyphAREffect::getParameter(const char* name) const {
    return m_controls.get(name);
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
