/**
 * @file LGPHyperbolicPortalAREffect.cpp
 * @brief Hyperbolic Portal (5-Layer Audio-Reactive)
 *
 * Poincaré-style hyperbolic portal with multi-band ribs.
 * Hyperbolic coordinate: u = artanh(r * 0.985)
 * Rib pattern: sin(w1*u + phi) + 0.62*sin(w2*u - 0.7*phi)
 * Sharp bands via smoothstep + pow, edge boost, centre glue.
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

// =========================================================================
// Local helpers
// =========================================================================

static constexpr float kTau = 6.28318530717958647692f;

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

// Safe artanh with clamp near singularity at r=1
static inline float artanh_safe(float r) {
    const float clamped = lowrisk_ar::clampf(r, -0.9999f, 0.9999f);
    return 0.5f * logf((1.0f + clamped) / (1.0f - clamped));
}

static inline float smoothstep(float edge0, float edge1, float x) {
    float t = lowrisk_ar::clamp01f((x - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

// =========================================================================
// Construction / init / cleanup
// =========================================================================

LGPHyperbolicPortalAREffect::LGPHyperbolicPortalAREffect()
    : m_phi(0.0f)
    , m_bed(0.3f)
    , m_w1(8.0f)
    , m_w2(13.0f)
    , m_impact(0.0f)
    , m_memory(0.0f)
{
}

bool LGPHyperbolicPortalAREffect::init(plugins::EffectContext& ctx) {
    m_controls.resetDefaults();
    m_ar = lowrisk_ar::ArRuntimeState{};
    m_ar.tonalHue = static_cast<float>(ctx.gHue);
    m_ar.modeSmooth = 2.0f;
    m_phi         = 0.0f;
    m_bed         = 0.3f;
    m_w1          = 8.0f;
    m_w2          = 13.0f;
    m_impact      = 0.0f;
    m_memory      = 0.0f;
    lightwaveos::effects::cinema::reset();
    return true;
}

// =========================================================================
// render() — 5-Layer composition
// =========================================================================

void LGPHyperbolicPortalAREffect::render(plugins::EffectContext& ctx) {
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

    // Bed: RMS portal glow (tau 0.40s)
    m_bed = lowrisk_ar::smoothTo(m_bed,
        0.20f + 0.80f * sig.rms, dtSignal, 0.40f);

    // Structure: rib frequencies from harmonic + rhythmic (tau 0.18s)
    const float w1Target = 6.0f + 6.0f * sig.harmonic + 3.0f * sig.rhythmic;
    const float w2Target = 10.0f + 8.0f * sig.harmonic + 5.0f * sig.rhythmic;
    m_w1 = lowrisk_ar::smoothTo(m_w1, w1Target, dtSignal, 0.18f);
    m_w2 = lowrisk_ar::smoothTo(m_w2, w2Target, dtSignal, 0.18f);

    // Impact: beat rib brightening (decay 0.20s)
    if (sig.beatTick) m_impact = fmaxf(m_impact, 0.85f);
    m_impact = lowrisk_ar::decay(m_impact, dtSignal, 0.20f);

    // Memory: portal persistence (tau 0.70s)
    m_memory = lowrisk_ar::clamp01f(
        lowrisk_ar::decay(m_memory, dtSignal, 0.70f)
        + 0.15f * m_impact + 0.08f * sig.flux);

    // =================================================================
    // MOTION
    // =================================================================

    const float phiRate = (0.80f + 3.50f * speedNorm) * m_controls.motionRate()
                          * (0.75f + 0.25f * sig.rhythmic);
    m_phi += phiRate * dtVisual;

    // =================================================================
    // PER-PIXEL RENDER
    // =================================================================

    const float mid    = (STRIP_LENGTH - 1) * 0.5f;
    const float invMid = 1.0f / mid;

    const float bedBright = 0.30f + 0.70f * m_bed;
    const float master = (ctx.brightness / 255.0f)
                         * lowrisk_ar::clamp01f(m_ar.audioPresence);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float dmid  = static_cast<float>(i) - mid;
        const float distN = fabsf(dmid) * invMid;

        // Hyperbolic coordinate mapping
        const float r = distN * 0.985f;
        const float u = artanh_safe(r);

        // Multi-band ribs
        const float rib1 = sinf(m_w1 * u + m_phi);
        const float rib2 = sinf(m_w2 * u - 0.7f * m_phi);
        const float ribRaw = rib1 + 0.62f * rib2;

        // Sharp bands via smoothstep + pow
        const float ribSmooth = smoothstep(-0.5f, 0.5f, ribRaw);
        const float ribSharp = powf(ribSmooth, 2.8f);

        // Edge boost (stronger at hyperbolic infinity)
        const float edgeBoost = 1.0f + 0.6f * distN;

        // Centre glue
        const float glue = 0.40f + 0.60f * expf(-(dmid * dmid) * 0.0015f);

        // Portal base shape
        const float portalShape = ribSharp * edgeBoost * glue;

        // Impact: brightens ribs
        const float impactAdd = m_impact * ribSharp * 0.40f;

        // Memory: gentle additive persistence
        const float memoryAdd = m_memory * (0.4f + 0.6f * ribSmooth) * 0.22f;

        // Compose: bed x portal + impact + memory
        float brightness = bedBright * portalShape + impactAdd + memoryAdd;
        brightness = lowrisk_ar::clamp01f(brightness);

        const uint8_t br = luminanceToBr(brightness, master);

        // Tonal hue: chord-driven anchor + hyperbolic spatial offset
        const uint8_t hue = static_cast<uint8_t>(
            m_ar.tonalHue + u * 35.0f + sig.harmonic * 18.0f);

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

// =========================================================================
// cleanup / metadata / parameters
// =========================================================================

void LGPHyperbolicPortalAREffect::cleanup() {}

const plugins::EffectMetadata& LGPHyperbolicPortalAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Hyperbolic Portal (5L-AR)",
        "Poincaré-style portal with multi-band hyperbolic ribs",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPHyperbolicPortalAREffect::getParameterCount() const {
    return lowrisk_ar::Ar16Controls::parameterCount();
}

const plugins::EffectParameter* LGPHyperbolicPortalAREffect::getParameter(uint8_t index) const {
    return lowrisk_ar::Ar16Controls::parameter(index);
}

bool LGPHyperbolicPortalAREffect::setParameter(const char* name, float value) {
    return m_controls.set(name, value);
}

float LGPHyperbolicPortalAREffect::getParameter(const char* name) const {
    return m_controls.get(name);
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
