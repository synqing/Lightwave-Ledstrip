/**
 * @file LGPCymaticLadderAREffect.cpp
 * @brief Cymatic Ladder (5-Layer Audio-Reactive)
 *
 * Standing-wave modes with full 5-layer AR composition. The harmonic mode
 * number n (2-8) is driven by structure + harmonic content with hysteresis
 * to prevent rapid mode jumps. Antinode blooms on beat; memory holds mode
 * persistence for organic transitions between standing-wave patterns.
 *
 * Centre-origin compliant. Dual-strip locked.
 */

#include "LGPCymaticLadderAREffect.h"
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

static constexpr float kPi  = 3.14159265358979323846f;
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

LGPCymaticLadderAREffect::LGPCymaticLadderAREffect()
    : m_t(0.0f)
    , m_bed(0.3f)
    , m_structure(0.5f)
    , m_impact(0.0f)
    , m_memory(0.0f)
    , m_modeSmooth(2.0f)
{
}

bool LGPCymaticLadderAREffect::init(plugins::EffectContext& ctx) {
    m_controls.resetDefaults();
    m_ar = lowrisk_ar::ArRuntimeState{};
    m_ar.tonalHue = static_cast<float>(ctx.gHue);
    m_ar.modeSmooth = 2.0f;
    m_t         = 0.0f;
    m_bed       = 0.3f;
    m_structure = 0.5f;
    m_impact    = 0.0f;
    m_memory    = 0.0f;
    m_modeSmooth = 2.0f;
    lightwaveos::effects::cinema::reset();
    return true;
}

// =========================================================================
// render() -- 5-Layer composition
// =========================================================================

void LGPCymaticLadderAREffect::render(plugins::EffectContext& ctx) {
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

    // Bed: standing-wave body brightness from RMS (tau 0.48s)
    m_bed = lowrisk_ar::smoothTo(m_bed,
        0.25f + 0.75f * sig.rms, dtSignal, 0.48f);

    // Structure: mode selection from harmonic + rhythmic + RMS (tau 0.18s)
    const float structTarget = lowrisk_ar::clamp01f(
        0.30f + 0.30f * sig.harmonic + 0.25f * sig.rhythmic + 0.15f * sig.rms);
    m_structure = lowrisk_ar::smoothTo(m_structure, structTarget, dtSignal, 0.18f);

    // Impact: antinode bloom on beat (fast attack, decay 0.22s)
    if (sig.beatTick) m_impact = fmaxf(m_impact, 0.88f);
    m_impact = lowrisk_ar::decay(m_impact, dtSignal, 0.22f);

    // Memory: mode hold persistence (tau 1.10s, slow)
    m_memory = lowrisk_ar::clamp01f(
        lowrisk_ar::decay(m_memory, dtSignal, 1.10f)
        + 0.09f * m_impact + 0.05f * sig.harmonic);

    // =================================================================
    // MODE SELECTION WITH HYSTERESIS
    // =================================================================

    // Mode target from harmonic content + structure (range 2..8)
    const float modeTarget = 2.0f + 6.0f * lowrisk_ar::clamp01f(
        0.10f + 0.58f * sig.harmonic + 0.32f * m_structure);

    // Hysteresis: only update if delta exceeds threshold
    if (fabsf(modeTarget - m_modeSmooth) > 0.14f) {
        m_modeSmooth = lowrisk_ar::smoothTo(m_modeSmooth, modeTarget, dtSignal, 0.30f);
    }
    const int n = static_cast<int>(lowrisk_ar::clampf(
        floorf(m_modeSmooth + 0.5f), 2.0f, 8.0f));

    // =================================================================
    // MOTION
    // =================================================================

    const float tRate = (1.10f + 4.20f * speedNorm) * m_controls.motionRate()
                        * (0.70f + 0.30f * m_structure);
    m_t += tRate * dtVisual;

    // =================================================================
    // EFFECT GEOMETRY -- Standing wave with mode-aware composition
    // =================================================================

    const float mid    = (STRIP_LENGTH - 1) * 0.5f;
    const float invMid = 1.0f / mid;

    // Phase driven by structure layer
    const float phase = m_t * (0.8f + 0.5f * speedNorm)
                       * (0.85f + 0.15f * m_structure);

    // Contrast exponent driven by structure + impact
    const float contrastExp = lowrisk_ar::clampf(
        1.8f + 0.8f * m_structure + 0.6f * m_impact, 1.4f, 3.4f);

    // =================================================================
    // PER-PIXEL RENDER
    // =================================================================

    const float bedBright = 0.25f + 0.75f * m_bed;
    const float master = (ctx.brightness / 255.0f)
                         * lowrisk_ar::clamp01f(m_ar.audioPresence);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float x     = static_cast<float>(i) / static_cast<float>(STRIP_LENGTH - 1);
        const float dmid  = static_cast<float>(i) - mid;
        const float distN = fabsf(dmid) * invMid;

        // Centre glue (Gaussian fall-off from centre)
        const float glue = 0.30f + 0.70f * expf(-(dmid * dmid) * 0.0016f);

        // Standing wave: sin(n * pi * x + phase)
        const float s = fabsf(sinf(static_cast<float>(n) * kPi * x + phase));
        const float structuredWave = powf(s, contrastExp);

        // Impact: antinode bloom on beat (additive at antinode positions)
        const float antinodeStrength = powf(s, 1.2f);
        const float impactAdd = m_impact * antinodeStrength * 0.32f;

        // Memory: gentle persistent glow
        const float memoryAdd = m_memory * s * 0.15f;

        // Compose: bed x structure + impact + memory
        float brightness = bedBright * structuredWave * glue + impactAdd + memoryAdd;
        brightness = lowrisk_ar::clamp01f(brightness);

        const uint8_t br = luminanceToBr(brightness, master);

        // Tonal hue: mode-aware chord hue with spatial offset
        const uint8_t hue = static_cast<uint8_t>(
            m_ar.tonalHue + static_cast<float>(n) * 8.0f
            + sig.harmonic * 36.0f + distN * 12.0f);

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

// =========================================================================
// cleanup / metadata / parameters
// =========================================================================

void LGPCymaticLadderAREffect::cleanup() {}

const plugins::EffectMetadata& LGPCymaticLadderAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Cymatic Ladder (5L-AR)",
        "Standing-wave modes with 5-layer audio-reactive composition",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPCymaticLadderAREffect::getParameterCount() const {
    return lowrisk_ar::Ar16Controls::parameterCount();
}

const plugins::EffectParameter* LGPCymaticLadderAREffect::getParameter(uint8_t index) const {
    return lowrisk_ar::Ar16Controls::parameter(index);
}

bool LGPCymaticLadderAREffect::setParameter(const char* name, float value) {
    return m_controls.set(name, value);
}

float LGPCymaticLadderAREffect::getParameter(const char* name) const {
    return m_controls.get(name);
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
