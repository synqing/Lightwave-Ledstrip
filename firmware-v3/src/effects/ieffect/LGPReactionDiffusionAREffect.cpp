/**
 * @file LGPReactionDiffusionAREffect.cpp
 * @brief Reaction Diffusion (5-Layer Audio-Reactive)
 *
 * Gray-Scott 1D reaction-diffusion with full 5-layer AR composition.
 * Base maths from LGPReactionDiffusionEffect, restructured into
 * explicit bed/structure/impact/tonal/memory layers.
 *
 * Centre-origin compliant. Dual-strip locked. PSRAM-allocated.
 */

#include "LGPReactionDiffusionAREffect.h"
#include "LGPFilmPost.h"
#include "../CoreEffects.h"
#include "../../utils/Log.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

// =========================================================================
// Local helpers
// =========================================================================

static inline void writeDualLocked(plugins::EffectContext& ctx, int i, const CRGB& c) {
    ctx.leds[i] = c;
    int j = i + STRIP_LENGTH;
    if (j < (int)ctx.ledCount) ctx.leds[j] = c;
}

static inline uint8_t luminanceToBr(float wave01, float master) {
    const float base = 0.06f;
    float out = clamp01(base + (1.0f - base) * clamp01(wave01)) * master;
    return static_cast<uint8_t>(255.0f * out);
}

// =========================================================================
// Construction / init / cleanup
// =========================================================================

LGPReactionDiffusionAREffect::LGPReactionDiffusionAREffect()
    : m_t(0.0f)
    , m_bed(0.3f)
    , m_structure(0.5f)
    , m_impact(0.0f)
    , m_memory(0.0f)
{
}

bool LGPReactionDiffusionAREffect::init(plugins::EffectContext& ctx) {
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

    // Allocate PSRAM buffers for reaction-diffusion simulation
    if (!m_ps) {
        m_ps = static_cast<PsramData*>(heap_caps_malloc(sizeof(PsramData), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            LW_LOGE("LGPReactionDiffusionAREffect: PSRAM alloc failed (%u bytes)",
                    (unsigned)sizeof(PsramData));
            return false;
        }
    }

    // Standard Gray-Scott init: U=1 everywhere, V=0; seed centre region with V
    for (int i = 0; i < STRIP_LENGTH; i++) {
        m_ps->u[i] = 1.0f;
        m_ps->v[i] = 0.0f;
    }
    const int mid = STRIP_LENGTH / 2;
    for (int i = mid - 6; i <= mid + 6; i++) {
        if (i >= 0 && i < STRIP_LENGTH) {
            m_ps->v[i] = 1.0f;
            m_ps->u[i] = 0.0f;
        }
    }

    return true;
}

// =========================================================================
// render() — 5-Layer composition + Gray-Scott simulation
// =========================================================================

void LGPReactionDiffusionAREffect::render(plugins::EffectContext& ctx) {
    if (!m_ps) return;

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

    // Bed: slow atmosphere from RMS (tau 0.45s)
    m_bed = lowrisk_ar::smoothTo(m_bed,
        0.20f + 0.80f * sig.rms, dtSignal, 0.45f);

    // Structure: F/K parameter modulation from bass + flux (tau 0.15s)
    const float structTarget = lowrisk_ar::clamp01f(
        0.30f + 0.40f * sig.bass + 0.25f * sig.flux + 0.05f * sig.rhythmic);
    m_structure = lowrisk_ar::smoothTo(m_structure, structTarget, dtSignal, 0.15f);

    // Impact: beat-triggered V-concentration spike at centre (decay 0.22s)
    if (sig.beatTick) m_impact = fmaxf(m_impact, 0.85f);
    m_impact = lowrisk_ar::decay(m_impact, dtSignal, 0.22f);

    // Memory: accumulated glow from reaction activity (slow decay, fed by structure)
    m_memory = lowrisk_ar::clamp01f(
        lowrisk_ar::decay(m_memory, dtSignal, 0.90f)
        + 0.08f * m_structure + 0.04f * sig.flux);
    lowrisk_ar::applyBedImpactMemoryMix(
        m_controls,
        static_cast<float>(ctx.rawTotalTimeMs),
        m_bed,
        m_impact,
        m_memory);

    // =================================================================
    // GRAY-SCOTT SIMULATION DRIVEN BY LAYERS
    // =================================================================

    // F/K parameters modulated by structure layer
    // F range: 0.035-0.042 (feed rate)
    // K range: 0.058-0.068 (kill rate)
    const float F = lowrisk_ar::clampf(
        0.0380f + 0.0070f * (m_structure - 0.5f), 0.035f, 0.042f);
    const float K = lowrisk_ar::clampf(
        0.0630f + 0.0100f * (m_structure - 0.5f), 0.058f, 0.068f);

    const float Du = 1.0f;
    const float Dv = 0.5f;

    // Time step: faster at higher speed + structure, stable range
    const float dt = lowrisk_ar::clampf(
        (0.9f + 0.6f * speedNorm + 0.3f * m_structure) * mod.motionRate, 0.7f, 2.2f);

    // Beat impact: inject V concentration at centre seed region
    if (m_impact > 0.05f) {
        const int mid = STRIP_LENGTH / 2;
        const float injectAmount = m_impact * 0.15f;
        for (int i = mid - 5; i <= mid + 5; i++) {
            if (i >= 0 && i < STRIP_LENGTH) {
                m_ps->v[i] = lowrisk_ar::clamp01f(m_ps->v[i] + injectAmount);
            }
        }
    }

    // Do 1-2 iterations per frame depending on speed (keeps cost bounded)
    const int iters = (speedNorm > 0.55f) ? 2 : 1;

    for (int iter = 0; iter < iters; iter++) {
        for (int i = 0; i < STRIP_LENGTH; i++) {
            const int im1 = (i == 0) ? 0 : (i - 1);
            const int ip1 = (i == STRIP_LENGTH - 1) ? (STRIP_LENGTH - 1) : (i + 1);

            // 1D Laplacian (stable for this use)
            const float lapU = m_ps->u[im1] - 2.0f * m_ps->u[i] + m_ps->u[ip1];
            const float lapV = m_ps->v[im1] - 2.0f * m_ps->v[i] + m_ps->v[ip1];

            const float u = m_ps->u[i];
            const float v = m_ps->v[i];

            // Gray-Scott reaction term (u*v^2) and feed/kill
            const float uvv = u * v * v;

            m_ps->u2[i] = u + (Du * lapU - uvv + F * (1.0f - u)) * dt;
            m_ps->v2[i] = v + (Dv * lapV + uvv - (K + F) * v) * dt;

            m_ps->u2[i] = clamp01(m_ps->u2[i]);
            m_ps->v2[i] = clamp01(m_ps->v2[i]);
        }

        for (int i = 0; i < STRIP_LENGTH; i++) {
            m_ps->u[i] = m_ps->u2[i];
            m_ps->v[i] = m_ps->v2[i];
        }
    }

    // =================================================================
    // PER-PIXEL RENDER WITH 5-LAYER COMPOSITION
    // =================================================================

    const float mid = (STRIP_LENGTH - 1) * 0.5f;
    const float bedBright = 0.20f + 0.80f * m_bed;
    const float master = (ctx.brightness / 255.0f)
                         * lowrisk_ar::clamp01f(m_ar.audioPresence) * mod.brightnessScale;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float x = static_cast<float>(i);
        const float dist = static_cast<float>(centerPairDistance(static_cast<uint16_t>(i)));

        const float dmid = x - mid;
        const float melt = expf(-(dmid * dmid) * 0.0018f);

        const float v = m_ps->v[i];

        // Bed x structured geometry: V concentration with centre melt glue
        float structuredGeom = v * melt + 0.25f * v;

        // Impact: additive spike at centre region
        float impactAdd = m_impact * melt * 0.30f;

        // Memory: gentle additive glow from accumulated activity
        float memoryAdd = m_memory * (0.5f + 0.5f * v) * 0.15f;

        // Compose: bed x structure + impact + memory
        float brightness = bedBright * structuredGeom + impactAdd + memoryAdd;
        brightness = clamp01(brightness);

        const uint8_t br = luminanceToBr(brightness, master);

        // Tonal hue: chord-driven anchor + spatial offset + V-driven tonal range
        const uint8_t hue = static_cast<uint8_t>(
            m_ar.tonalHue + dist * 0.6f + v * 180.0f + sig.harmonic * 15.0f);

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
    m_t += 1.0f;
}

// =========================================================================
// cleanup / metadata / parameters
// =========================================================================

void LGPReactionDiffusionAREffect::cleanup() {
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
}

const plugins::EffectMetadata& LGPReactionDiffusionAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Reaction Diffusion (5L-AR)",
        "Gray-Scott slime with 5-layer audio-reactive composition",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPReactionDiffusionAREffect::getParameterCount() const {
    return lowrisk_ar::Ar16Controls::parameterCount();
}

const plugins::EffectParameter* LGPReactionDiffusionAREffect::getParameter(uint8_t index) const {
    return lowrisk_ar::Ar16Controls::parameter(index);
}

bool LGPReactionDiffusionAREffect::setParameter(const char* name, float value) {
    return m_controls.set(name, value);
}

float LGPReactionDiffusionAREffect::getParameter(const char* name) const {
    return m_controls.get(name);
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
