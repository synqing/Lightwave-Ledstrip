/**
 * @file LGPRDTriangleAREffect.cpp
 * @brief LGP Reaction Diffusion Triangle (5-Layer Audio-Reactive)
 *
 * Gray-Scott reaction-diffusion system with full 5-layer AR composition.
 * Base maths from LGPReactionDiffusionTriangleEffect, restructured into
 * explicit bed/structure/impact/tonal/memory layers.
 *
 * Centre-origin compliant. Dual-strip locked.
 */

#include "LGPRDTriangleAREffect.h"
#include "LGPFilmPost.h"
#include "../CoreEffects.h"
#include "../../utils/Log.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

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

LGPRDTriangleAREffect::LGPRDTriangleAREffect()
    : m_bed(0.3f)
    , m_structure(0.5f)
    , m_impact(0.0f)
    , m_memory(0.0f)
    , m_F(0.0380f)
    , m_K(0.0630f)
    , m_meltK(0.0018f)
{
}

bool LGPRDTriangleAREffect::init(plugins::EffectContext& ctx) {
    m_controls.resetDefaults();
    m_ar = lowrisk_ar::ArRuntimeState{};
    m_ar.tonalHue = static_cast<float>(ctx.gHue);
    m_ar.modeSmooth = 2.0f;

    m_bed         = 0.3f;
    m_structure   = 0.5f;
    m_impact      = 0.0f;
    m_memory      = 0.0f;

    m_F     = 0.0380f;
    m_K     = 0.0630f;
    m_meltK = 0.0018f;

    // Allocate large buffers in PSRAM
    if (!m_ps) {
        m_ps = static_cast<PsramData*>(heap_caps_malloc(sizeof(PsramData), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            LW_LOGE("LGPRDTriangleAREffect: PSRAM alloc failed (%u bytes)", (unsigned)sizeof(PsramData));
            return false;
        }
    }

    // Initialise reaction-diffusion fields
    for (int i = 0; i < STRIP_LENGTH; i++) {
        m_ps->u[i] = 1.0f;
        m_ps->v[i] = 0.0f;
    }

    // Seed centre with V
    const int mid = STRIP_LENGTH / 2;
    for (int i = mid - 6; i <= mid + 6; i++) {
        if (i >= 0 && i < STRIP_LENGTH) {
            m_ps->v[i] = 1.0f;
            m_ps->u[i] = 0.0f;
        }
    }

    lightwaveos::effects::cinema::reset();
    return true;
}

// =========================================================================
// render() — 5-Layer composition
// =========================================================================

void LGPRDTriangleAREffect::render(plugins::EffectContext& ctx) {
    if (!m_ps) return;

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

    // Bed: slow RMS-driven atmosphere (tau 0.45s)
    m_bed = lowrisk_ar::smoothTo(m_bed,
        0.25f + 0.75f * sig.rms, dtSignal, 0.45f);

    // Structure: F/K/meltK modulation from bass + mid + flux (tau 0.18s)
    const float structTarget = clamp01(
        0.30f + 0.35f * sig.bass + 0.25f * sig.mid + 0.10f * sig.flux);
    m_structure = lowrisk_ar::smoothTo(m_structure, structTarget, dtSignal, 0.18f);

    // Impact: beat-triggered V-injection at centre (decay 0.20s)
    if (sig.beatTick) m_impact = fmaxf(m_impact, 1.0f);
    m_impact = lowrisk_ar::decay(m_impact, dtSignal, 0.20f);

    // Memory: accumulated glow (slow decay, fed by beat + flux)
    m_memory = clamp01(
        lowrisk_ar::decay(m_memory, dtSignal, 0.85f)
        + 0.15f * m_impact + 0.08f * sig.flux);

    // =================================================================
    // STRUCTURE LAYER MODULATES GRAY-SCOTT PARAMETERS
    // =================================================================

    // F/K: structure layer pushes into different reaction regimes
    m_F = lowrisk_ar::clampf(
        0.0380f + 0.0080f * (m_structure - 0.5f),
        0.0300f, 0.0500f);
    m_K = lowrisk_ar::clampf(
        0.0630f + 0.0100f * (m_structure - 0.5f),
        0.0550f, 0.0750f);

    // meltK: centre-glue strength modulated by structure
    m_meltK = lowrisk_ar::clampf(
        0.0018f + 0.0008f * m_structure,
        0.0010f, 0.0035f);

    // =================================================================
    // GRAY-SCOTT REACTION-DIFFUSION STEP
    // =================================================================

    const float Du = 1.0f;
    const float Dv = 0.5f;
    const float dt = 0.9f + 0.6f * speedNorm;
    const int iters = (speedNorm > 0.55f) ? 2 : 1;

    for (int iter = 0; iter < iters; iter++) {
        for (int i = 0; i < STRIP_LENGTH; i++) {
            const int im1 = (i == 0) ? 0 : (i - 1);
            const int ip1 = (i == STRIP_LENGTH - 1) ? (STRIP_LENGTH - 1) : (i + 1);

            const float lapU = m_ps->u[im1] - 2.0f * m_ps->u[i] + m_ps->u[ip1];
            const float lapV = m_ps->v[im1] - 2.0f * m_ps->v[i] + m_ps->v[ip1];

            const float u = m_ps->u[i];
            const float v = m_ps->v[i];
            const float uvv = u * v * v;

            m_ps->u2[i] = u + (Du * lapU - uvv + m_F * (1.0f - u)) * dt;
            m_ps->v2[i] = v + (Dv * lapV + uvv - (m_K + m_F) * v) * dt;

            m_ps->u2[i] = clamp01(m_ps->u2[i]);
            m_ps->v2[i] = clamp01(m_ps->v2[i]);
        }

        // Swap buffers
        for (int i = 0; i < STRIP_LENGTH; i++) {
            m_ps->u[i] = m_ps->u2[i];
            m_ps->v[i] = m_ps->v2[i];
        }
    }

    // Impact layer: inject V at centre on beat
    if (m_impact > 0.05f) {
        const int mid = STRIP_LENGTH / 2;
        const int radius = 8;
        for (int i = mid - radius; i <= mid + radius; i++) {
            if (i >= 0 && i < STRIP_LENGTH) {
                const float dist = fabsf(static_cast<float>(i - mid)) / static_cast<float>(radius);
                const float inject = m_impact * (1.0f - dist) * 0.4f;
                m_ps->v[i] = fminf(m_ps->v[i] + inject, 1.0f);
                m_ps->u[i] = fmaxf(m_ps->u[i] - inject * 0.5f, 0.0f);
            }
        }
    }

    // =================================================================
    // PER-PIXEL RENDER: 5-LAYER COMPOSITION
    // =================================================================

    const float mid = (STRIP_LENGTH - 1) * 0.5f;
    const float master = (ctx.brightness / 255.0f)
                         * clamp01(m_ar.audioPresence);
    const float bedBright = 0.25f + 0.75f * m_bed;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float x = static_cast<float>(i);
        const float dist = static_cast<float>(centerPairDistance(static_cast<uint16_t>(i)));

        const float dmid = x - mid;
        const float melt = expf(-(dmid * dmid) * m_meltK);

        const float v = m_ps->v[i];

        // Bed × structure (V concentration × centre-glue)
        float structuredV = v * melt;

        // Impact: additive spike at centre
        const float impactDist = fabsf(dmid) / mid;
        const float impactAdd = m_impact * (1.0f - impactDist) * 0.25f;

        // Memory: gentle additive glow
        const float memoryAdd = m_memory * (0.4f + 0.6f * v) * 0.15f;

        // Compose: bed × (v × melt) + impact + memory
        float brightness = bedBright * (0.15f * melt + 0.85f * structuredV)
                         + impactAdd + memoryAdd;
        brightness = clamp01(brightness);

        const uint8_t br = luminanceToBr(brightness, master);

        // Tonal hue: chord-driven anchor + spatial offset + V modulation
        const uint8_t hue = static_cast<uint8_t>(
            m_ar.tonalHue + dist * 0.6f + v * 180.0f + sig.harmonic * 15.0f);

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

// =========================================================================
// cleanup / metadata / parameters
// =========================================================================

void LGPRDTriangleAREffect::cleanup() {
    if (m_ps) { heap_caps_free(m_ps); m_ps = nullptr; }
}

const plugins::EffectMetadata& LGPRDTriangleAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP RD Triangle (5L-AR)",
        "Reaction-diffusion with 5-layer audio-reactive composition",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPRDTriangleAREffect::getParameterCount() const {
    return lowrisk_ar::Ar16Controls::parameterCount();
}

const plugins::EffectParameter* LGPRDTriangleAREffect::getParameter(uint8_t index) const {
    return lowrisk_ar::Ar16Controls::parameter(index);
}

bool LGPRDTriangleAREffect::setParameter(const char* name, float value) {
    return m_controls.set(name, value);
}

float LGPRDTriangleAREffect::getParameter(const char* name) const {
    return m_controls.get(name);
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
