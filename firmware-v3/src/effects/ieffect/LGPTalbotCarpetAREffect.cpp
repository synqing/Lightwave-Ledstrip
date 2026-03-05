/**
 * @file LGPTalbotCarpetAREffect.cpp
 * @brief Talbot Carpet (5-Layer Audio-Reactive)
 *
 * Fresnel self-imaging lattice with full 5-layer AR composition.
 * The Talbot effect produces periodic revivals of a grating pattern at
 * rational fractions of the Talbot distance — here driven by audio layers
 * that modulate grating pitch, propagation depth, and self-image locking.
 *
 * Centre-origin compliant. Dual-strip locked.
 */

#include "LGPTalbotCarpetAREffect.h"
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

LGPTalbotCarpetAREffect::LGPTalbotCarpetAREffect()
    : m_t(0.0f)
    , m_bed(0.3f)
    , m_structure(0.5f)
    , m_impact(0.0f)
    , m_memory(0.0f)
{
}

bool LGPTalbotCarpetAREffect::init(plugins::EffectContext& ctx) {
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

void LGPTalbotCarpetAREffect::render(plugins::EffectContext& ctx) {
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

    // Bed: low-contrast carpet luminance from RMS (tau 0.45s)
    m_bed = lowrisk_ar::smoothTo(m_bed,
        0.20f + 0.80f * sig.rms, dtSignal, 0.45f);

    // Structure: phase warp from rhythmicSaliency, pitch from mid (tau 0.18s)
    const float structTarget = lowrisk_ar::clamp01f(
        0.30f + 0.30f * sig.rhythmic + 0.25f * sig.mid + 0.15f * sig.harmonic);
    m_structure = lowrisk_ar::smoothTo(m_structure, structTarget, dtSignal, 0.18f);

    // Impact: downbeat self-image lock pulse (fast attack, decay 0.25s)
    if (sig.beatTick) m_impact = fmaxf(m_impact, 0.90f);
    m_impact = lowrisk_ar::decay(m_impact, dtSignal, 0.25f);

    // Memory: slow retention of last image clarity
    m_memory = lowrisk_ar::clamp01f(
        lowrisk_ar::decay(m_memory, dtSignal, 0.95f)
        + 0.08f * m_impact + 0.04f * sig.harmonic);

    // =================================================================
    // MOTION
    // =================================================================

    const float tRate = (1.10f + 4.20f * speedNorm) * m_controls.motionRate()
                        * (0.70f + 0.30f * m_structure);
    m_t += tRate * dtVisual;

    // =================================================================
    // EFFECT GEOMETRY -- Fresnel harmonic sum (Talbot self-imaging)
    // =================================================================

    const float mid    = (STRIP_LENGTH - 1) * 0.5f;
    const float invMid = 1.0f / mid;

    // Grating pitch driven by structure layer (6..22 px range)
    const float p = lowrisk_ar::clampf(10.0f - 4.0f * m_structure, 6.0f, 22.0f);

    // Propagation depth: time + impact triggers self-image lock
    const float z = m_t + 0.45f * m_impact;

    // Lock pulse: peaks near rational Talbot fractions, driven by impact
    const float lockPulse = expf(-fabsf(fract(z * 0.09f) - 0.5f) * 7.5f) * m_impact;

    // =================================================================
    // PER-PIXEL RENDER
    // =================================================================

    const float bedBright = 0.25f + 0.75f * m_bed;
    const float master = (ctx.brightness / 255.0f)
                         * lowrisk_ar::clamp01f(m_ar.audioPresence);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float dmid  = static_cast<float>(i) - mid;
        const float distN = fabsf(dmid) * invMid;

        // Centre glue (Gaussian fall-off from centre)
        const float glue = 0.35f + 0.65f * expf(-(dmid * dmid) * 0.0016f);

        // Fresnel harmonic sum -- 7 orders of diffraction
        const float phi = kTau * (static_cast<float>(i) / p);
        float sumC = 0.0f, sumS = 0.0f, norm = 0.0f;
        for (int k = 1; k <= 7; k++) {
            const float ak = 1.0f / static_cast<float>(k);
            const float phase = static_cast<float>(k) * phi
                              + static_cast<float>(k * k) * z * 0.55f;
            sumC += ak * cosf(phase);
            sumS += ak * sinf(phase);
            norm += ak;
        }
        const float amp = sqrtf(sumC * sumC + sumS * sumS) / (norm + 1e-6f);

        // Structured amplitude with edge fade
        const float structuredAmp = powf(lowrisk_ar::clamp01f(amp), 1.7f)
                                  * lowrisk_ar::lerpf(1.0f, 0.85f, distN);

        // Impact: additive lock pulse
        const float impactAdd = lockPulse * 0.38f;

        // Memory: gentle retention glow modulated by local amplitude
        const float memoryAdd = m_memory * amp * 0.16f;

        // Compose: bed x structure + impact + memory
        float brightness = bedBright * structuredAmp + impactAdd + memoryAdd;
        brightness = lowrisk_ar::clamp01f(brightness);

        const uint8_t br = luminanceToBr(brightness * glue, master);

        // Tonal hue: chord-driven + spatial offset + harmonic shimmer
        const uint8_t hue = static_cast<uint8_t>(
            m_ar.tonalHue + distN * 22.0f + sig.harmonic * 34.0f);

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

// =========================================================================
// cleanup / metadata / parameters
// =========================================================================

void LGPTalbotCarpetAREffect::cleanup() {}

const plugins::EffectMetadata& LGPTalbotCarpetAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Talbot Carpet (5L-AR)",
        "Self-imaging lattice with 5-layer audio-reactive composition",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPTalbotCarpetAREffect::getParameterCount() const {
    return lowrisk_ar::Ar16Controls::parameterCount();
}

const plugins::EffectParameter* LGPTalbotCarpetAREffect::getParameter(uint8_t index) const {
    return lowrisk_ar::Ar16Controls::parameter(index);
}

bool LGPTalbotCarpetAREffect::setParameter(const char* name, float value) {
    return m_controls.set(name, value);
}

float LGPTalbotCarpetAREffect::getParameter(const char* name) const {
    return m_controls.get(name);
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
