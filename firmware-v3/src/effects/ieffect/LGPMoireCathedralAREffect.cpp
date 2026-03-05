/**
 * @file LGPMoireCathedralAREffect.cpp
 * @brief Moire Cathedral (5-Layer Audio-Reactive)
 *
 * Interference arches from close gratings with full 5-layer AR composition.
 * Base maths from ShapeBangersPack LGPMoireCathedralEffect, restructured into
 * explicit bed/structure/impact/tonal/memory layers.
 *
 * Centre-origin compliant. Dual-strip locked.
 */

#include "LGPMoireCathedralAREffect.h"
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

// =========================================================================
// Construction / init / cleanup
// =========================================================================

LGPMoireCathedralAREffect::LGPMoireCathedralAREffect()
    : m_t(0.0f)
    , m_bed(0.3f)
    , m_structure(0.5f)
    , m_impact(0.0f)
    , m_memory(0.0f)
{
}

bool LGPMoireCathedralAREffect::init(plugins::EffectContext& ctx) {
    m_controls.resetDefaults();
    m_ar = lowrisk_ar::ArRuntimeState{};
    m_ar.tonalHue = static_cast<float>(ctx.gHue);
    m_ar.modeSmooth = 2.0f;
    m_t         = 0.0f;
    m_bed       = 0.3f;
    m_structure = 0.5f;
    m_impact    = 0.0f;
    m_memory    = 0.0f;
    lightwaveos::effects::cinema::reset();
    return true;
}

// =========================================================================
// render() — 5-Layer composition
// =========================================================================

void LGPMoireCathedralAREffect::render(plugins::EffectContext& ctx) {
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

    // Structure: grating pitch modulation from bass + mid (tau 0.14s)
    const float structTarget = lowrisk_ar::clamp01f(
        0.30f + 0.40f * sig.bass + 0.20f * sig.mid + 0.10f * sig.rhythmic);
    m_structure = lowrisk_ar::smoothTo(m_structure, structTarget, dtSignal, 0.14f);

    // Impact: beat-triggered arch brightening (fast attack, decay 0.20s)
    if (sig.beatTick) m_impact = fmaxf(m_impact, 0.85f);
    m_impact = lowrisk_ar::decay(m_impact, dtSignal, 0.20f);

    // Memory: accumulated arch glow (slow decay, fed by impact + flux)
    m_memory = lowrisk_ar::clamp01f(
        lowrisk_ar::decay(m_memory, dtSignal, 0.80f)
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

    const float tRate = (0.85f + 3.50f * speedNorm) * mod.motionRate
                        * (0.75f + 0.25f * m_structure);
    m_t += tRate * dtVisual;

    // =================================================================
    // EFFECT GEOMETRY DRIVEN BY LAYERS
    // =================================================================

    const float mid    = (STRIP_LENGTH - 1) * 0.5f;

    // Grating pitches: structure modulates p1 (6-9 range), p2 tracks p1+0.6
    const float p1 = lowrisk_ar::clampf(
        7.5f + (m_structure - 0.5f) * 3.0f, 6.0f, 9.0f);
    const float p2 = p1 + 0.6f;

    // Grating rotation rates (from original w1/w2)
    const float w1 = 0.65f + 0.40f * speedNorm;
    const float w2 = 0.58f + 0.35f * speedNorm;

    // Cathedral rib sharpening: original 1.35, increased by impact
    const float ribPow = lowrisk_ar::clampf(
        1.35f + 0.45f * m_impact, 1.30f, 2.20f);

    // =================================================================
    // PER-PIXEL RENDER
    // =================================================================

    const float bedBright = 0.25f + 0.75f * m_bed;
    const float master = (ctx.brightness / 255.0f)
                         * lowrisk_ar::clamp01f(m_ar.audioPresence) * mod.brightnessScale;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float x = static_cast<float>(i);
        const float dmid = x - mid;

        // Centre glue (original 0.0011 decay)
        const float glue = 0.40f + 0.60f * expf(-(dmid * dmid) * 0.0011f);

        // Two grating waves
        const float g1 = sinf(kTau * (x / p1) + m_t * w1);
        const float g2 = sinf(kTau * (x / p2) - m_t * w2);

        // Moire interference (0..2 range)
        const float moire = fabsf(g1 - g2);
        float wave = lowrisk_ar::clamp01f(moire * 0.55f);

        // Cathedral ribs: compress highlights (impact sharpens)
        wave = powf(wave, ribPow);

        // Bed x structured geometry
        float structuredGeom = wave * glue;

        // Impact: additive brightness at arch peaks
        float impactAdd = m_impact * wave * glue * 0.30f;

        // Memory: gentle additive glow at arches
        float memoryAdd = m_memory * (0.5f + 0.5f * wave) * glue * 0.20f;

        // Compose: bed x structure + impact + memory
        float brightness = bedBright * structuredGeom + impactAdd + memoryAdd;
        brightness = lowrisk_ar::clamp01f(brightness);

        const uint8_t br = luminanceToBr(brightness, master);

        // Tonal hue: chord-driven anchor + spatial offset (original two-tone pattern)
        const uint8_t hue = static_cast<uint8_t>(
            m_ar.tonalHue + wave * 42.0f + glue * 12.0f + sig.harmonic * 15.0f);

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

// =========================================================================
// cleanup / metadata / parameters
// =========================================================================

void LGPMoireCathedralAREffect::cleanup() {}

const plugins::EffectMetadata& LGPMoireCathedralAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Moire Cathedral (5L-AR)",
        "Interference arches with 5-layer audio-reactive composition",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPMoireCathedralAREffect::getParameterCount() const {
    return lowrisk_ar::Ar16Controls::parameterCount();
}

const plugins::EffectParameter* LGPMoireCathedralAREffect::getParameter(uint8_t index) const {
    return lowrisk_ar::Ar16Controls::parameter(index);
}

bool LGPMoireCathedralAREffect::setParameter(const char* name, float value) {
    return m_controls.set(name, value);
}

float LGPMoireCathedralAREffect::getParameter(const char* name) const {
    return m_controls.get(name);
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
