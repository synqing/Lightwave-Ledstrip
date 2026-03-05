/**
 * @file LGPSchlierenFlowAREffect.cpp
 * @brief Schlieren Flow (5-Layer Audio-Reactive)
 *
 * Knife-edge gradient flow with full 5-layer AR composition.
 * Base maths from the lowrisk_ar LGPSchlierenFlowEffect, restructured into
 * explicit bed/structure/impact/tonal/memory layers.
 *
 * Centre-origin compliant. Dual-strip with independent hue/brightness offsets.
 */

#include "LGPSchlierenFlowAREffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

// =========================================================================
// Construction / init / cleanup
// =========================================================================

LGPSchlierenFlowAREffect::LGPSchlierenFlowAREffect()
    : m_t(0.0f)
    , m_bed(0.3f)
    , m_structure(0.5f)
    , m_impact(0.0f)
    , m_snareImpact(0.0f)
    , m_hihatImpact(0.0f)
    , m_memory(0.0f)
{
}

bool LGPSchlierenFlowAREffect::init(plugins::EffectContext& ctx) {
    m_controls.resetDefaults();
    m_ar = lowrisk_ar::ArRuntimeState{};
    m_ar.tonalHue = static_cast<float>(ctx.gHue);
    m_ar.modeSmooth = 2.0f;
    m_t           = 0.0f;
    m_bed         = 0.3f;
    m_structure   = 0.5f;
    m_impact      = 0.0f;
    m_snareImpact = 0.0f;
    m_hihatImpact = 0.0f;
    m_memory      = 0.0f;
    return true;
}

// =========================================================================
// render() -- 5-Layer composition
// =========================================================================

void LGPSchlierenFlowAREffect::render(plugins::EffectContext& ctx) {
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

    // Bed: smooth haze from RMS (tau 0.50s)
    m_bed = lowrisk_ar::smoothTo(m_bed,
        0.20f + 0.80f * sig.rms, dtSignal, 0.50f);

    // Structure: gradient slope from bass, edge sharpening from treble (tau 0.12s)
    const float structTarget = lowrisk_ar::clamp01f(
        0.30f + 0.35f * sig.bass + 0.15f * sig.treble + 0.20f * sig.flux);
    m_structure = lowrisk_ar::smoothTo(m_structure, structTarget, dtSignal, 0.12f);

    // Impact: beat-triggered burst (decay 0.18s)
    if (sig.beatTick) m_impact = fmaxf(m_impact, 0.85f);
    m_impact = lowrisk_ar::decay(m_impact, dtSignal, 0.18f);

    // Hihat: fast edge-intensity pop (decay 0.15s)
    if (ctx.audio.available && ctx.audio.isHihatHit())
        m_hihatImpact = fmaxf(m_hihatImpact, 0.75f);
    m_hihatImpact = lowrisk_ar::decay(m_hihatImpact, dtSignal, 0.15f);

    // Snare: separate envelope (decay 0.22s)
    if (ctx.audio.available && ctx.audio.isSnareHit())
        m_snareImpact = fmaxf(m_snareImpact, 0.70f);
    m_snareImpact = lowrisk_ar::decay(m_snareImpact, dtSignal, 0.22f);

    // Memory: turbulence persistence after hits (slow decay, fed by impact + flux)
    m_memory = lowrisk_ar::clamp01f(
        lowrisk_ar::decay(m_memory, dtSignal, 0.85f)
        + 0.10f * m_impact + 0.08f * m_hihatImpact + 0.04f * sig.flux);

    // =================================================================
    // MOTION
    // =================================================================

    const float flowRate = (1.45f + 8.40f * speedNorm) * m_controls.motionRate()
                           * (0.70f + 0.30f * m_structure);
    m_t += flowRate * dtVisual;

    // =================================================================
    // SCHLIEREN GEOMETRY DRIVEN BY LAYERS
    // =================================================================

    // Spatial frequencies modulated by structure
    const float f1 = 0.060f * (1.0f + 0.16f * m_structure);
    const float f2 = 0.145f * (1.0f + 0.14f * m_structure);
    const float f3 = 0.310f * (1.0f + 0.18f * m_structure);

    // Edge gain: structure sharpens edges, hihat pops
    const float edgeGain = 6.0f + 3.0f * m_structure + 2.5f * m_hihatImpact;

    // Turbulence: structure + memory drive distortion
    const float turbulence = m_structure * 0.45f + m_memory * 0.30f;

    const float midPos = (STRIP_LENGTH - 1) * 0.5f;

    // =================================================================
    // PER-PIXEL RENDER
    // =================================================================

    const float bedBright = 0.25f + 0.75f * m_bed;
    const float master = (ctx.brightness / 255.0f)
                         * lowrisk_ar::clamp01f(m_ar.audioPresence);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float x = static_cast<float>(i);
        const float dist = static_cast<float>(centerPairDistance(static_cast<uint16_t>(i)));
        const float distN = dist / static_cast<float>(HALF_LENGTH);
        const float dmid = x - midPos;

        // Knife-edge gradient flow (original Schlieren physics)
        float rho =
            sinf(x * f1 + m_t)
            + 0.7f * sinf(x * f2 - m_t * 1.2f)
            + 0.3f * sinf(x * f3 + m_t * 2.1f + turbulence * 2.0f);

        float grad =
            f1 * cosf(x * f1 + m_t)
            + 0.7f * f2 * cosf(x * f2 - m_t * 1.2f)
            + 0.3f * f3 * cosf(x * f3 + m_t * 2.1f);

        float edge = 0.5f + 0.5f * tanhf(grad * edgeGain);

        // Centre glue
        const float melt = expf(-(dmid * dmid) * 0.0028f);

        // Structured edge: blend edge*melt with softer sinusoidal fill
        float structuredEdge = 0.65f * (edge * melt + 0.25f * melt)
                             + 0.35f * (0.5f + 0.5f * sinf(rho));

        // Impact: hihat/snare/beat pops at edge positions
        float impactAdd = m_impact * edge * 0.30f
                        + m_hihatImpact * edge * 0.25f
                        + m_snareImpact * edge * 0.18f;

        // Memory: gentle turbulence persistence at centre
        float memoryAdd = m_memory * melt * 0.14f;

        // Compose: bed x structure + impact + memory
        float brightness = bedBright * structuredEdge + impactAdd + memoryAdd;
        brightness = lowrisk_ar::clamp01f(brightness);

        const float base = 0.06f;
        const float out = lowrisk_ar::clamp01f(base + (1.0f - base) * brightness) * master;
        const uint8_t brA = static_cast<uint8_t>(255.0f * out);

        // Tonal hue: chord-driven with spatial offset (no ambient blend)
        const uint8_t hueA = static_cast<uint8_t>(
            m_ar.tonalHue + distN * 18.0f + sig.harmonic * 32.0f);

        // Strip B: slight hue and brightness offset
        const uint8_t hueB = static_cast<uint8_t>(
            hueA + 5u + static_cast<uint8_t>(sig.rhythmic * 4.0f));
        const uint8_t brB = scale8_video(brA,
            static_cast<uint8_t>(240 + static_cast<uint8_t>(sig.treble * 10.0f)));

        ctx.leds[i] = ctx.palette.getColor(hueA, brA);
        const int j = i + STRIP_LENGTH;
        if (j < ctx.ledCount) {
            ctx.leds[j] = ctx.palette.getColor(hueB, brB);
        }
    }
}

// =========================================================================
// cleanup / metadata / parameters
// =========================================================================

void LGPSchlierenFlowAREffect::cleanup() {}

const plugins::EffectMetadata& LGPSchlierenFlowAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Schlieren Flow (5L-AR)",
        "Knife-edge gradient flow with 5-layer audio-reactive composition",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPSchlierenFlowAREffect::getParameterCount() const {
    return lowrisk_ar::Ar16Controls::parameterCount();
}

const plugins::EffectParameter* LGPSchlierenFlowAREffect::getParameter(uint8_t index) const {
    return lowrisk_ar::Ar16Controls::parameter(index);
}

bool LGPSchlierenFlowAREffect::setParameter(const char* name, float value) {
    return m_controls.set(name, value);
}

float LGPSchlierenFlowAREffect::getParameter(const char* name) const {
    return m_controls.get(name);
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
