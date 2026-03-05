/**
 * @file LGPRoseBloomAREffect.cpp
 * @brief Rose Bloom (5-Layer Audio-Reactive)
 *
 * Rhodonea curve blooming petals with full 5-layer AR composition.
 * Base maths: r = |cos(kf * theta)| with kf drifting 3-7 (petal count).
 * Distance-to-curve band rendering with "opening bloom" modulation.
 *
 * Centre-origin compliant. Dual-strip locked.
 */

#include "LGPRoseBloomAREffect.h"
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

LGPRoseBloomAREffect::LGPRoseBloomAREffect()
    : m_t(0.0f)
    , m_bed(0.3f)
    , m_structure(5.0f)
    , m_impact(0.0f)
    , m_memory(0.0f)
{
}

bool LGPRoseBloomAREffect::init(plugins::EffectContext& ctx) {
    m_controls.resetDefaults();
    m_ar = lowrisk_ar::ArRuntimeState{};
    m_ar.tonalHue = static_cast<float>(ctx.gHue);
    m_ar.modeSmooth = 2.0f;
    m_t         = 0.0f;
    m_bed       = 0.3f;
    m_structure = 5.0f;
    m_impact    = 0.0f;
    m_memory    = 0.0f;
    lightwaveos::effects::cinema::reset();
    return true;
}

// =========================================================================
// render() — 5-Layer composition
// =========================================================================

void LGPRoseBloomAREffect::render(plugins::EffectContext& ctx) {
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

    // Bed: slow RMS glow (tau 0.42s)
    m_bed = lowrisk_ar::smoothTo(m_bed,
        0.20f + 0.80f * sig.rms, dtSignal, 0.42f);

    // Structure: petal count kf from harmonic + rhythmic (tau 0.25s)
    // Range 3.0 - 7.0 (3-7 petals)
    const float kfTarget = lowrisk_ar::clampf(
        5.0f + 1.5f * sig.harmonic + 0.8f * sig.rhythmic,
        3.0f, 7.0f);
    m_structure = lowrisk_ar::smoothTo(m_structure, kfTarget, dtSignal, 0.25f);

    // Impact: beat-triggered petal flash (decay 0.18s)
    if (sig.beatTick) m_impact = fmaxf(m_impact, 1.0f);
    m_impact = lowrisk_ar::decay(m_impact, dtSignal, 0.18f);

    // Memory: bloom persistence (slow decay, fed by beat + flux)
    m_memory = lowrisk_ar::clamp01f(
        lowrisk_ar::decay(m_memory, dtSignal, 0.80f)
        + 0.15f * m_impact + 0.08f * sig.flux);

    // =================================================================
    // MOTION
    // =================================================================

    const float tRate = (0.35f + 2.0f * speedNorm) * m_controls.motionRate()
                        * (0.80f + 0.20f * sig.rms);
    m_t += tRate * dtVisual;

    // Opening bloom modulation: 0.55 + 0.45*sin(t*0.35)
    const float bloomMod = 0.55f + 0.45f * sinf(m_t * 0.35f);

    // =================================================================
    // EFFECT GEOMETRY DRIVEN BY LAYERS
    // =================================================================

    const float mid    = (STRIP_LENGTH - 1) * 0.5f;
    const float invMid = 1.0f / mid;

    // Band width: tighter with impact
    const float bandWidth = lowrisk_ar::clampf(
        0.15f - 0.05f * m_impact, 0.08f, 0.18f);

    // =================================================================
    // PER-PIXEL RENDER
    // =================================================================

    const float bedBright = 0.20f + 0.80f * m_bed;
    const float master = (ctx.brightness / 255.0f)
                         * lowrisk_ar::clamp01f(m_ar.audioPresence);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float dmid  = static_cast<float>(i) - mid;
        const float distN = fabsf(dmid) * invMid;

        // Polar coordinates from centre (LED 79/80)
        const float x = dmid;
        const float y = 0.0f; // 1D strip projected to x-axis
        const float r = fabsf(x);
        const float theta = (x >= 0.0f) ? 0.0f : 3.14159265358979323846f;

        // Rhodonea curve: r_curve = |cos(kf * theta)| scaled by strip extent
        const float kf = m_structure;
        const float rCurve = fabsf(cosf(kf * theta)) * mid * bloomMod;

        // Distance to curve band
        const float distToCurve = fabsf(r - rCurve) * invMid;
        const float band = expf(-distToCurve * distToCurve / (bandWidth * bandWidth));

        // Petal breathing along curve
        const float breathing = 0.90f + 0.10f * cosf(kTau * (distN + m_t * 0.25f));

        // Centre glue (stronger than Mach Diamonds)
        const float glue = 0.45f + 0.55f * expf(-(dmid * dmid) * 0.0018f);

        // Bed × bloom geometry
        float bloomGeom = band * breathing * glue;

        // Impact: additive flash at petal edges
        float impactShape = band * glue;
        float impactAdd = m_impact * impactShape * 0.40f;

        // Memory: gentle additive glow
        float memoryAdd = m_memory * (0.6f + 0.4f * band) * 0.22f;

        // Compose: bed × structure + impact + memory
        float brightness = bedBright * bloomGeom + impactAdd + memoryAdd;
        brightness = lowrisk_ar::clamp01f(brightness);

        const uint8_t br = luminanceToBr(brightness, master);

        // Tonal hue: chord-driven anchor + spatial offset
        const uint8_t hue = static_cast<uint8_t>(
            m_ar.tonalHue + band * 45.0f + sig.harmonic * 25.0f);

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

// =========================================================================
// cleanup / metadata / parameters
// =========================================================================

void LGPRoseBloomAREffect::cleanup() {}

const plugins::EffectMetadata& LGPRoseBloomAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Rose Bloom (5L-AR)",
        "Rhodonea curve blooming petals with 5-layer audio-reactive composition",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPRoseBloomAREffect::getParameterCount() const {
    return lowrisk_ar::Ar16Controls::parameterCount();
}

const plugins::EffectParameter* LGPRoseBloomAREffect::getParameter(uint8_t index) const {
    return lowrisk_ar::Ar16Controls::parameter(index);
}

bool LGPRoseBloomAREffect::setParameter(const char* name, float value) {
    return m_controls.set(name, value);
}

float LGPRoseBloomAREffect::getParameter(const char* name) const {
    return m_controls.get(name);
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
