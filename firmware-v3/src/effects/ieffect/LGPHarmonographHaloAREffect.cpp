/**
 * @file LGPHarmonographHaloAREffect.cpp
 * @brief Harmonograph Halo (5-Layer Audio-Reactive)
 *
 * Lissajous orbital halos with full 5-layer AR composition.
 * Maths: x = sin(a*(phi+rot)+delta), y = sin(b*(phi-rot)), r = sqrt(x²+y²)*0.72
 * Distance-to-curve band creates halo, energy pulses on beat.
 *
 * Centre-origin compliant. Dual-strip locked.
 */

#include "LGPHarmonographHaloAREffect.h"
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
// Construction / init / cleanup
// =========================================================================

LGPHarmonographHaloAREffect::LGPHarmonographHaloAREffect()
    : m_t(0.0f)
    , m_bed(0.3f)
    , m_freqA(3.0f)
    , m_freqB(2.5f)
    , m_delta(0.0f)
    , m_impact(0.0f)
    , m_memory(0.0f)
{
}

bool LGPHarmonographHaloAREffect::init(plugins::EffectContext& ctx) {
    m_controls.resetDefaults();
    m_ar = lowrisk_ar::ArRuntimeState{};
    m_ar.tonalHue = static_cast<float>(ctx.gHue);
    m_ar.modeSmooth = 2.0f;
    m_t      = 0.0f;
    m_bed    = 0.3f;
    m_freqA  = 3.0f;
    m_freqB  = 2.5f;
    m_delta  = 0.0f;
    m_impact = 0.0f;
    m_memory = 0.0f;
    lightwaveos::effects::cinema::reset();
    return true;
}

// =========================================================================
// render() — 5-Layer composition
// =========================================================================

void LGPHarmonographHaloAREffect::render(plugins::EffectContext& ctx) {
    // --- Timing ---
    const float dtSignal = AudioReactivePolicy::signalDt(ctx);
    const float dtVisual = AudioReactivePolicy::visualDt(ctx);
    const float speedNorm = ctx.speed / 50.0f;

    // --- Signals (shared infrastructure handles presence gate + fallback) ---
    const lowrisk_ar::ArSignalSnapshot sig =
        lowrisk_ar::updateSignals(m_ar, ctx, m_controls, dtSignal);
    const lowrisk_ar::ArModulationProfile mod =
        lowrisk_ar::buildModulation(m_controls, sig, m_ar, ctx);
    m_ar.tonalHue = mod.baseHue;

    // =================================================================
    // LAYER UPDATES
    // =================================================================

    // Bed: slow atmosphere from RMS (tau 0.40s)
    m_bed = lowrisk_ar::smoothTo(m_bed,
        0.25f + 0.75f * sig.rms, dtSignal, 0.40f);

    // Structure: Lissajous frequency ratios from harmonic + mid bands (tau 0.20s)
    // freqA: 2.5 - 3.5 driven by harmonic content
    const float freqATarget = 2.5f + 1.0f * sig.harmonic;
    m_freqA = lowrisk_ar::smoothTo(m_freqA, freqATarget, dtSignal, 0.20f);

    // freqB: 2.0 - 3.0 driven by mid-range energy
    const float freqBTarget = 2.0f + 1.0f * sig.mid;
    m_freqB = lowrisk_ar::smoothTo(m_freqB, freqBTarget, dtSignal, 0.20f);

    // delta: phase offset drifts with flux + harmonic
    const float deltaDrift = (0.5f + 0.8f * sig.flux + 0.3f * sig.harmonic) * dtSignal;
    m_delta = fract(m_delta + deltaDrift);

    // Impact: beat-triggered orbital flash (decay 0.20s)
    if (sig.beatTick) m_impact = fmaxf(m_impact, 0.85f);
    m_impact = lowrisk_ar::decay(m_impact, dtSignal, 0.20f);

    // Memory: halo persistence (slow decay 0.75s, fed by beat + flux)
    m_memory = lowrisk_ar::clamp01f(
        lowrisk_ar::decay(m_memory, dtSignal, 0.75f)
        + 0.15f * m_impact + 0.08f * sig.flux);
    lowrisk_ar::applyBedImpactMemoryMix(
        m_controls,
        static_cast<float>(ctx.rawTotalTimeMs),
        m_bed,
        m_impact,
        m_memory);

    // =================================================================
    // MOTION
    // =================================================================

    const float rotRate = (0.85f + 3.50f * speedNorm) * mod.motionRate
                          * (0.65f + 0.35f * sig.rhythmic);
    m_t += rotRate * dtVisual;

    // =================================================================
    // LISSAJOUS ORBITAL SAMPLING (parametric curve)
    // =================================================================

    // Sample 64 points around the orbital (phi ∈ [0, 2π])
    static constexpr int kOrbitSamples = 64;
    float orbitX[kOrbitSamples];
    float orbitY[kOrbitSamples];

    for (int s = 0; s < kOrbitSamples; s++) {
        const float phi = kTau * (static_cast<float>(s) / static_cast<float>(kOrbitSamples));
        // Lissajous: x = sin(a*(phi+rot)+delta), y = sin(b*(phi-rot))
        const float x = sinf(m_freqA * (phi + m_t) + m_delta * kTau);
        const float y = sinf(m_freqB * (phi - m_t));
        // Map to LED strip coordinate space (scaled by 0.72 for halo aesthetic)
        orbitX[s] = x * 0.72f;
        orbitY[s] = y * 0.72f;
    }

    // =================================================================
    // PER-PIXEL RENDER
    // =================================================================

    const float mid    = (STRIP_LENGTH - 1) * 0.5f;
    const float invMid = 1.0f / mid;

    const float bedBright = 0.25f + 0.75f * m_bed;
    const float master = (ctx.brightness / 255.0f)
                         * lowrisk_ar::clamp01f(m_ar.audioPresence) * mod.brightnessScale;

    // Halo band width (affected by structure)
    const float bandWidth = lowrisk_ar::clampf(
        0.12f + 0.08f * sig.bass + 0.05f * m_impact,
        0.08f, 0.25f);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float dmid  = static_cast<float>(i) - mid;
        const float distN = fabsf(dmid) * invMid;  // 0 at centre, 1 at edges

        // Map LED position to 2D polar (simple radial mapping for LGP)
        const float ledX = distN;  // Assume LEDs map to radius
        const float ledY = 0.0f;   // Simplified: 1D strip as radial slice

        // Find minimum distance to orbital curve
        float minDist = 999.0f;
        for (int s = 0; s < kOrbitSamples; s++) {
            const float dx = ledX - orbitX[s];
            const float dy = ledY - orbitY[s];
            const float dist = sqrtf(dx * dx + dy * dy);
            if (dist < minDist) minDist = dist;
        }

        // Distance-to-curve band creates halo
        const float distBand = 1.0f - lowrisk_ar::clamp01f(minDist / bandWidth);
        const float geom = powf(distBand, 2.2f);

        // Centre glue
        const float glue = 0.40f + 0.60f * expf(-(dmid * dmid) * 0.0015f);

        // Bed x structured geometry
        float structuredGeom = geom * glue;

        // Impact: additive orbital flash at curve points
        float impactShape = powf(distBand, 1.5f) * glue;
        float impactAdd   = m_impact * impactShape * 0.40f;

        // Memory: gentle additive glow
        float memoryAdd = m_memory * (0.4f + 0.6f * geom) * 0.20f;

        // Compose: bed x structure + impact + memory
        float brightness = bedBright * structuredGeom + impactAdd + memoryAdd;
        brightness = lowrisk_ar::clamp01f(brightness);

        const uint8_t br = luminanceToBr(brightness, master);

        // Tonal hue: chord-driven anchor + spatial offset + orbital phase
        const uint8_t hue = static_cast<uint8_t>(
            m_ar.tonalHue + distBand * 48.0f + sig.harmonic * 25.0f + m_delta * 64.0f);

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

// =========================================================================
// cleanup / metadata / parameters
// =========================================================================

void LGPHarmonographHaloAREffect::cleanup() {}

const plugins::EffectMetadata& LGPHarmonographHaloAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Harmonograph Halo (5L-AR)",
        "Lissajous orbital halos with 5-layer audio-reactive composition",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPHarmonographHaloAREffect::getParameterCount() const {
    return lowrisk_ar::Ar16Controls::parameterCount();
}

const plugins::EffectParameter* LGPHarmonographHaloAREffect::getParameter(uint8_t index) const {
    return lowrisk_ar::Ar16Controls::parameter(index);
}

bool LGPHarmonographHaloAREffect::setParameter(const char* name, float value) {
    return m_controls.set(name, value);
}

float LGPHarmonographHaloAREffect::getParameter(const char* name) const {
    return m_controls.get(name);
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
