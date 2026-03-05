/**
 * @file LGPSpirographCrownAREffect.cpp
 * @brief Spirograph Crown (5-Layer Audio-Reactive)
 *
 * Hypotrochoid crown geometry with facet sparkle.
 * Base maths: parametric hypotrochoid with R=1.0, r=0.30+drift, d=0.78.
 * Distance-to-curve metric creates luminous band structure.
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

// =========================================================================
// Local helpers
// =========================================================================

static constexpr float kTau = 6.28318530717958647692f;

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
// Hypotrochoid distance metric
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
        const float t = (kTau * static_cast<float>(i)) / static_cast<float>(samples);

        // Hypotrochoid parametric equations
        const float x = (R - r_param) * cosf(t) + d * cosf(ratio * t);
        const float y = (R - r_param) * sinf(t) - d * sinf(ratio * t);

        // Convert sampled point to polar
        const float curve_r = sqrtf(x * x + y * y);
        const float curve_theta = atan2f(y, x);

        // Angular distance (wrapped)
        float dtheta = fabsf(theta - curve_theta);
        if (dtheta > M_PI) dtheta = kTau - dtheta;

        // Combined distance metric (radial + angular weighted)
        const float dist = sqrtf((r - curve_r) * (r - curve_r) +
                                 (r * dtheta) * (r * dtheta));

        if (dist < minDist) minDist = dist;
    }

    return minDist;
}

// =========================================================================
// Construction / init / cleanup
// =========================================================================

LGPSpirographCrownAREffect::LGPSpirographCrownAREffect()
    : m_t(0.0f)
    , m_bed(0.3f)
    , m_structure(0.5f)
    , m_impact(0.0f)
    , m_memory(0.0f)
{
}

bool LGPSpirographCrownAREffect::init(plugins::EffectContext& ctx) {
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

// =========================================================================
// render() — 5-Layer composition
// =========================================================================

void LGPSpirographCrownAREffect::render(plugins::EffectContext& ctx) {
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

    // Bed: slow atmosphere from RMS (tau 0.40s)
    m_bed = lowrisk_ar::smoothTo(m_bed,
        0.25f + 0.75f * sig.rms, dtSignal, 0.40f);

    // Structure: r parameter + facet from rhythmic + mid (tau 0.16s)
    const float structTarget = lowrisk_ar::clamp01f(
        0.45f + 0.30f * sig.rhythmic + 0.25f * sig.mid);
    m_structure = lowrisk_ar::smoothTo(m_structure, structTarget, dtSignal, 0.16f);

    // Impact: beat-triggered crown flash (fast attack, decay 0.20s)
    if (sig.beatTick) m_impact = fmaxf(m_impact, 1.0f);
    m_impact = lowrisk_ar::decay(m_impact, dtSignal, 0.20f);

    // Memory: crown persistence (slow decay 0.70s, fed by beat)
    m_memory = lowrisk_ar::clamp01f(
        lowrisk_ar::decay(m_memory, dtSignal, 0.70f)
        + 0.15f * m_impact);

    // =================================================================
    // MOTION
    // =================================================================

    const float tRate = (0.80f + 3.50f * speedNorm) * m_controls.motionRate()
                        * (0.65f + 0.35f * m_structure);
    m_t += tRate * dtVisual;

    // =================================================================
    // EFFECT GEOMETRY DRIVEN BY LAYERS
    // =================================================================

    const float mid    = (STRIP_LENGTH - 1) * 0.5f;
    const float invMid = 1.0f / mid;

    // r parameter drift: structure modulates the hypotrochoid shape
    const float r_drift = 0.30f + 0.12f * (m_structure - 0.5f);

    // Facet sparkle: rhythmic modulation creates glinting effect
    const float facet = 0.85f + 0.15f * sig.rhythmic;

    // Band width: impact narrows the crown band
    const float bandWidth = lowrisk_ar::clampf(
        0.08f - 0.03f * m_impact, 0.03f, 0.10f);

    // =================================================================
    // PER-PIXEL RENDER
    // =================================================================

    const float bedBright = 0.25f + 0.75f * m_bed;
    const float master = (ctx.brightness / 255.0f)
                         * lowrisk_ar::clamp01f(m_ar.audioPresence);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float dmid  = static_cast<float>(i) - mid;
        const float distN = fabsf(dmid) * invMid;

        // Convert to polar coordinates (centre origin at LED 79/80)
        const float r = distN;
        const float theta = (m_t * 0.35f) + static_cast<float>(i) * 0.042f;

        // Distance to hypotrochoid curve
        const float dist = distToCurve(r, theta, r_drift);

        // Band structure: crown appears where dist is small
        const float band = 1.0f - lowrisk_ar::clamp01f(dist / bandWidth);
        const float bandSharp = powf(band, 2.2f);

        // Facet sparkle modulation
        const float facetMod = facet * (0.90f + 0.10f * cosf(kTau * theta * 3.7f));

        // Centre glue: stronger near centre
        const float glue = 0.40f + 0.60f * expf(-(dmid * dmid) * 0.0015f);

        // Bed x structured geometry
        float structuredGeom = bandSharp * facetMod * glue;

        // Impact: additive flash at crown band
        float impactAdd = m_impact * bandSharp * glue * 0.40f;

        // Memory: gentle persistent glow
        float memoryAdd = m_memory * band * 0.22f;

        // Compose: bed x structure + impact + memory
        float brightness = bedBright * structuredGeom + impactAdd + memoryAdd;
        brightness = lowrisk_ar::clamp01f(brightness);

        const uint8_t br = luminanceToBr(brightness, master);

        // Tonal hue: chord-driven anchor + spatial modulation
        const uint8_t hue = static_cast<uint8_t>(
            m_ar.tonalHue + band * 45.0f + sig.harmonic * 18.0f);

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

// =========================================================================
// cleanup / metadata / parameters
// =========================================================================

void LGPSpirographCrownAREffect::cleanup() {}

const plugins::EffectMetadata& LGPSpirographCrownAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Spirograph Crown (5L-AR)",
        "Hypotrochoid crown jewellery with 5-layer audio-reactive composition",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPSpirographCrownAREffect::getParameterCount() const {
    return lowrisk_ar::Ar16Controls::parameterCount();
}

const plugins::EffectParameter* LGPSpirographCrownAREffect::getParameter(uint8_t index) const {
    return lowrisk_ar::Ar16Controls::parameter(index);
}

bool LGPSpirographCrownAREffect::setParameter(const char* name, float value) {
    return m_controls.set(name, value);
}

float LGPSpirographCrownAREffect::getParameter(const char* name) const {
    return m_controls.get(name);
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
