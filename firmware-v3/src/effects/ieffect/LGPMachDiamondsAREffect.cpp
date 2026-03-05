/**
 * @file LGPMachDiamondsAREffect.cpp
 * @brief Mach Diamonds (5-Layer Audio-Reactive)
 *
 * Shock-diamond standing-wave pulses with full 5-layer AR composition.
 * Base maths from the lowrisk_ar LGPMachDiamondsEffect, restructured into
 * explicit bed/structure/impact/tonal/memory layers.
 *
 * Centre-origin compliant. Dual-strip locked.
 */

#include "LGPMachDiamondsAREffect.h"
#include "LGPFilmPost.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

// =========================================================================
// Local helpers (shared with ShapeBangersPack pattern)
// =========================================================================

static constexpr float kTau = 6.28318530717958647692f;

static inline float fract(float x) { return x - floorf(x); }

static inline float tri01(float x) {
    float f = fract(x);
    return lowrisk_ar::clamp01f(1.0f - fabsf(2.0f * f - 1.0f));
}

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

LGPMachDiamondsAREffect::LGPMachDiamondsAREffect()
    : m_t(0.0f)
    , m_bed(0.3f)
    , m_structure(0.5f)
    , m_impact(0.0f)
    , m_snareImpact(0.0f)
    , m_memory(0.0f)
{
}

bool LGPMachDiamondsAREffect::init(plugins::EffectContext& ctx) {
    m_controls.resetDefaults();
    m_ar = lowrisk_ar::ArRuntimeState{};
    m_ar.tonalHue = static_cast<float>(ctx.gHue);
    m_ar.modeSmooth = 2.0f;
    m_t           = 0.0f;
    m_bed         = 0.3f;
    m_structure   = 0.5f;
    m_impact      = 0.0f;
    m_snareImpact = 0.0f;
    m_memory      = 0.0f;
    lightwaveos::effects::cinema::reset();
    return true;
}

// =========================================================================
// render() — 5-Layer composition
// =========================================================================

void LGPMachDiamondsAREffect::render(plugins::EffectContext& ctx) {
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

    // Structure: geometry modulation from bass + flux (tau 0.15s)
    const float structTarget = lowrisk_ar::clamp01f(
        0.35f + 0.35f * sig.bass + 0.20f * sig.flux + 0.10f * sig.rhythmic);
    m_structure = lowrisk_ar::smoothTo(m_structure, structTarget, dtSignal, 0.15f);

    // Impact: beat-triggered burst (fast attack, decay 0.20s)
    if (sig.beatTick) m_impact = fmaxf(m_impact, 0.90f);
    m_impact = lowrisk_ar::decay(m_impact, dtSignal, 0.20f);

    // Snare: separate fast envelope
    if (ctx.audio.available && ctx.audio.isSnareHit())
        m_snareImpact = fmaxf(m_snareImpact, 0.70f);
    m_snareImpact = lowrisk_ar::decay(m_snareImpact, dtSignal, 0.25f);

    // Memory: accumulated glow (slow decay, fed by beat + flux)
    m_memory = lowrisk_ar::clamp01f(
        lowrisk_ar::decay(m_memory, dtSignal, 0.80f)
        + 0.12f * m_impact + 0.06f * sig.flux);

    // =================================================================
    // MOTION
    // =================================================================

    const float tRate = (1.10f + 4.20f * speedNorm) * m_controls.motionRate()
                        * (0.70f + 0.30f * m_structure);
    m_t += tRate * dtVisual;

    // =================================================================
    // EFFECT GEOMETRY DRIVEN BY LAYERS
    // =================================================================

    const float mid    = (STRIP_LENGTH - 1) * 0.5f;
    const float invMid = 1.0f / mid;

    // Spacing: structure widens/narrows shock cells, impact compresses
    const float spacing = lowrisk_ar::clampf(
        0.13f * (1.0f - 0.25f * (m_structure - 0.5f)) - 0.08f * m_impact,
        0.07f, 0.24f);

    const float drift = m_t * (0.20f + 0.35f * speedNorm + 0.10f * m_structure);

    // Contrast: sharper with structure + impact
    const float sharpExp = lowrisk_ar::clampf(
        2.2f + 0.8f * m_structure + 0.6f * m_impact, 1.8f, 3.8f);

    // =================================================================
    // PER-PIXEL RENDER
    // =================================================================

    const float bedBright = 0.25f + 0.75f * m_bed;
    const float master = (ctx.brightness / 255.0f)
                         * lowrisk_ar::clamp01f(m_ar.audioPresence);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float dmid  = static_cast<float>(i) - mid;
        const float distN = fabsf(dmid) * invMid;

        // Shock diamond triangular wave
        const float cellPhase = (distN / spacing) - drift;
        const float tri = tri01(cellPhase);
        const float geom = powf(tri, sharpExp);

        // Diamond breathing
        const float breathing = 0.85f + 0.15f * cosf(kTau * cellPhase + m_t * 0.6f);

        // Centre glue
        const float glue = 0.35f + 0.65f * expf(-(dmid * dmid) * 0.0013f);

        // Bed x structured geometry
        float structuredGeom = geom * breathing * glue;

        // Impact: additive spike at diamond peaks
        float impactShape = powf(tri, 1.5f) * glue;
        float impactAdd   = m_impact * impactShape * 0.35f
                          + m_snareImpact * impactShape * 0.20f;

        // Memory: gentle additive glow
        float memoryAdd = m_memory * (0.5f + 0.5f * tri) * 0.18f;

        // Compose: bed x structure + impact + memory
        float brightness = bedBright * structuredGeom + impactAdd + memoryAdd;
        brightness = lowrisk_ar::clamp01f(brightness);

        const uint8_t br = luminanceToBr(brightness, master);

        // Tonal hue: chord-driven anchor + spatial offset
        const uint8_t hue = static_cast<uint8_t>(
            m_ar.tonalHue + tri * 52.0f + sig.harmonic * 20.0f);

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

// =========================================================================
// cleanup / metadata / parameters
// =========================================================================

void LGPMachDiamondsAREffect::cleanup() {}

const plugins::EffectMetadata& LGPMachDiamondsAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Mach Diamonds (5L-AR)",
        "Shock-diamond jewellery with 5-layer audio-reactive composition",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPMachDiamondsAREffect::getParameterCount() const {
    return lowrisk_ar::Ar16Controls::parameterCount();
}

const plugins::EffectParameter* LGPMachDiamondsAREffect::getParameter(uint8_t index) const {
    return lowrisk_ar::Ar16Controls::parameter(index);
}

bool LGPMachDiamondsAREffect::setParameter(const char* name, float value) {
    return m_controls.set(name, value);
}

float LGPMachDiamondsAREffect::getParameter(const char* name) const {
    return m_controls.get(name);
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
