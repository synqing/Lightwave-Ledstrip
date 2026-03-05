/**
 * @file LGPAiryCometAREffect.cpp
 * @brief LGP Airy Comet (5-Layer AR) implementation
 *
 * Self-accelerating parabolic comet with oscillatory Airy-ish tail lobes,
 * restructured with 5-layer audio-reactive composition. Impact accelerates
 * the head, structure modulates tail decay and lobe spacing, memory
 * persists luminance in the tail region.
 *
 * Original comet physics preserved from LGPAiryCometEffect (Shape Bangers).
 * Centre-origin compliant. Dual-strip locked.
 */

#include "LGPAiryCometAREffect.h"
#include "LGPFilmPost.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

// =========================================================================
// Local helpers (no heap, no state)
// =========================================================================

static constexpr float kTau = 6.28318530717958647692f;

static inline float fract(float x) { return x - floorf(x); }

static inline float gaussian(float x, float sigma) {
    const float s2 = sigma * sigma;
    return expf(-(x * x) / (2.0f * s2));
}

static inline float smoothstep(float a, float b, float x) {
    float t = lowrisk_ar::clamp01f((x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}

static inline float sbLerp(float a, float b, float t) { return a + (b - a) * t; }

static inline void writeDualLocked(plugins::EffectContext& ctx, int i, const CRGB& c) {
    ctx.leds[i] = c;
    int j = i + STRIP_LENGTH;
    if (j < static_cast<int>(ctx.ledCount)) ctx.leds[j] = c;
}

static inline uint8_t luminanceToBr(float wave01, float master) {
    const float base = 0.06f;
    float out = lowrisk_ar::clamp01f(base + (1.0f - base) * lowrisk_ar::clamp01f(wave01)) * master;
    return static_cast<uint8_t>(255.0f * out);
}

// =========================================================================
// Construction / init / cleanup
// =========================================================================

LGPAiryCometAREffect::LGPAiryCometAREffect()
    : m_controls()
    , m_ar()
    , m_t(0.0f)
    , m_bed(0.3f)
    , m_structure(0.5f)
    , m_impact(0.0f)
    , m_memory(0.0f)
{
}

bool LGPAiryCometAREffect::init(plugins::EffectContext& ctx) {
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
// render() -- 5-Layer composition
// =========================================================================

void LGPAiryCometAREffect::render(plugins::EffectContext& ctx) {
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

    // Bed: soft glow ribbon brightness from RMS (tau 0.42s)
    m_bed = lowrisk_ar::smoothTo(m_bed,
        0.20f + 0.80f * sig.rms, dtSignal, 0.42f);

    // Structure: head trajectory speed, tail decay, lobe spacing (tau 0.14s)
    const float structTarget = lowrisk_ar::clamp01f(
        0.30f + 0.35f * sig.treble + 0.20f * sig.flux + 0.15f * sig.rhythmic);
    m_structure = lowrisk_ar::smoothTo(m_structure, structTarget, dtSignal, 0.14f);

    // Impact: beat-driven thrust pulse (fast attack, decay 0.18s)
    if (sig.beatTick) m_impact = fmaxf(m_impact, 0.92f);
    m_impact = lowrisk_ar::decay(m_impact, dtSignal, 0.18f);

    // Memory: tail persistence (slow decay, fed by impact + treble)
    m_memory = lowrisk_ar::clamp01f(
        lowrisk_ar::decay(m_memory, dtSignal, 1.00f)
        + 0.14f * m_impact + 0.05f * sig.treble);

    // =================================================================
    // MOTION — time accumulator modulated by structure layer
    // =================================================================

    const float tRate = (1.20f + 5.40f * speedNorm) * m_controls.motionRate()
                        * (0.72f + 0.28f * m_structure);
    m_t += tRate * dtVisual;

    // =================================================================
    // COMET GEOMETRY — parabolic self-accelerating bounce
    // =================================================================

    const float mid = (STRIP_LENGTH - 1) * 0.5f;

    // Impact accelerates the head through parabolic speed
    float s = fract(m_t * (0.10f + 0.06f * m_impact));
    float parab = s * s;
    float x0 = sbLerp(-mid * 0.92f, mid * 0.92f, parab);

    // Direction flip includes rhythmic modulation
    bool flip = (fract(m_t * 0.06f + 0.14f * sig.rhythmic) > 0.5f);
    float dir = flip ? -1.0f : 1.0f;

    // =================================================================
    // PER-PIXEL RENDER
    // =================================================================

    const float bedBright = 0.25f + 0.75f * m_bed;
    const float master = (ctx.brightness / 255.0f)
                         * lowrisk_ar::clamp01f(m_ar.audioPresence);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float x = static_cast<float>(i) - mid;
        float dmid = x;

        // Centre glue envelope
        float glue = expf(-(dmid * dmid) * 0.0018f);

        float dx = x - x0;

        // Head gaussian — RMS widens/narrows
        const float sigma = 3.2f * (1.0f + 0.30f * (sig.rms - 0.5f));
        float head = gaussian(dx, sigma);

        // Airy-ish tail: oscillatory lobes behind the head, decaying
        float behind = (dx * dir > 0.0f) ? (dx * dir) : 0.0f;

        // Structure extends tail decay and modulates lobe spacing
        float tail = expf(-behind * (0.12f - 0.04f * m_structure)) *
                     (0.55f + 0.45f * cosf(behind * (1.25f + 0.35f * m_structure) - m_t * 0.9f));

        // Tail mask for memory layer spatial gating
        float tail_mask = lowrisk_ar::clamp01f(expf(-behind * 0.08f));

        // 5-layer per-pixel composition
        float structuredWave = powf(lowrisk_ar::clamp01f(head + 0.75f * tail), 1.25f);
        float impactAdd = expf(-behind * 0.18f) * m_impact * 0.38f;   // thrust at head
        float memoryAdd = m_memory * tail_mask * 0.16f;                // persist in tail region

        // Melt into wings (no strip rivalry)
        float glued = sbLerp(structuredWave, structuredWave * (0.45f + 0.55f * glue), 0.85f);

        float brightness = bedBright * glued + impactAdd + memoryAdd;
        brightness = lowrisk_ar::clamp01f(brightness);

        uint8_t br = luminanceToBr(brightness, master);

        // Hue: warm head, cool tail — tonal anchor drives base
        float hue = m_ar.tonalHue
                    + (60.0f * (1.0f - smoothstep(0.0f, 1.0f, head)))
                    + 15.0f * glue;
        uint8_t hueU8 = static_cast<uint8_t>(hue);

        writeDualLocked(ctx, i, ctx.palette.getColor(hueU8, br));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

// =========================================================================
// cleanup / metadata / parameters
// =========================================================================

void LGPAiryCometAREffect::cleanup() {
    // No resources to free (no PSRAM allocation)
}

const plugins::EffectMetadata& LGPAiryCometAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Airy Comet (5L-AR)",
        "Self-accelerating comet with 5-layer audio-reactive composition",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPAiryCometAREffect::getParameterCount() const {
    return lowrisk_ar::Ar16Controls::parameterCount();
}

const plugins::EffectParameter* LGPAiryCometAREffect::getParameter(uint8_t index) const {
    return lowrisk_ar::Ar16Controls::parameter(index);
}

bool LGPAiryCometAREffect::setParameter(const char* name, float value) {
    return m_controls.set(name, value);
}

float LGPAiryCometAREffect::getParameter(const char* name) const {
    return m_controls.get(name);
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
