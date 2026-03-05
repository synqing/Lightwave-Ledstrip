/**
 * @file LGPWaterCausticsAREffect.cpp
 * @brief LGP Water Caustics (5-Layer AR) implementation
 *
 * Ray-envelope cusp filaments with 5-layer audio-reactive composition.
 * Base refractive maths preserved from the original LGPWaterCausticsEffect,
 * restructured so that bed/structure/impact/snare/memory layers drive the
 * physics coefficients directly rather than blendAmbientReactive.
 *
 * Centre-origin compliant. Dual-strip with per-strip hue/brightness offset.
 */

#include "LGPWaterCausticsAREffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

// =========================================================================
// Construction / init / cleanup
// =========================================================================

LGPWaterCausticsAREffect::LGPWaterCausticsAREffect()
    : m_controls()
    , m_ar()
    , m_t1(0.0f)
    , m_t2(0.0f)
    , m_bed(0.3f)
    , m_structure(0.5f)
    , m_impact(0.0f)
    , m_snareImpact(0.0f)
    , m_memory(0.0f)
{
}

bool LGPWaterCausticsAREffect::init(plugins::EffectContext& ctx) {
    m_controls.resetDefaults();
    m_ar = lowrisk_ar::ArRuntimeState{};
    m_ar.tonalHue = static_cast<float>(ctx.gHue);
    m_ar.modeSmooth = 2.0f;
    m_t1          = 0.0f;
    m_t2          = 0.0f;
    m_bed         = 0.3f;
    m_structure   = 0.5f;
    m_impact      = 0.0f;
    m_snareImpact = 0.0f;
    m_memory      = 0.0f;
    return true;
}

// =========================================================================
// render() -- 5-Layer composition
// =========================================================================

void LGPWaterCausticsAREffect::render(plugins::EffectContext& ctx) {
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

    // Bed: slow caustic sheet brightness from RMS (tau 0.45s)
    m_bed = lowrisk_ar::smoothTo(m_bed,
        0.25f + 0.75f * sig.rms, dtSignal, 0.45f);

    // Structure: modulates refractive field warp (A, B, k1, k2) (tau 0.12s)
    const float structTarget = lowrisk_ar::clamp01f(
        0.30f + 0.35f * sig.mid + 0.20f * sig.flux + 0.15f * sig.bass);
    m_structure = lowrisk_ar::smoothTo(m_structure, structTarget, dtSignal, 0.12f);

    // Impact: beat-triggered cusp gain burst (fast attack, decay 0.22s)
    if (sig.beatTick) m_impact = fmaxf(m_impact, 0.85f);
    m_impact = lowrisk_ar::decay(m_impact, dtSignal, 0.22f);

    // Snare: separate fast transient envelope (decay 0.28s)
    if (ctx.audio.available && ctx.audio.isSnareHit())
        m_snareImpact = fmaxf(m_snareImpact, 0.65f);
    m_snareImpact = lowrisk_ar::decay(m_snareImpact, dtSignal, 0.28f);

    // Memory: lingering glint (slow decay, fed by impact + flux)
    m_memory = lowrisk_ar::clamp01f(
        lowrisk_ar::decay(m_memory, dtSignal, 0.90f)
        + 0.10f * m_impact + 0.05f * sig.flux);
    lowrisk_ar::applyBedImpactMemoryMix(
        m_controls,
        static_cast<float>(ctx.rawTotalTimeMs),
        m_bed,
        m_impact,
        m_memory);

    // =================================================================
    // MOTION — two time accumulators modulated by structure layer
    // =================================================================

    const float t1Rate = (1.30f + 7.10f * speedNorm) * mod.motionRate
                         * (0.70f + 0.30f * m_structure);
    const float t2Rate = (0.80f + 4.80f * speedNorm) * mod.motionRate
                         * (0.70f + 0.30f * m_structure);
    m_t1 += t1Rate * dtVisual;
    m_t2 += t2Rate * dtVisual;

    // =================================================================
    // REFRACTIVE COEFFICIENTS — driven by structure layer
    // =================================================================

    const float A  = 0.75f + 0.30f * m_structure;   // refractive amplitude
    const float B  = 0.35f + 0.22f * m_structure;   // secondary amplitude
    const float k1 = 0.18f * (1.0f + 0.25f * m_structure);  // wave number 1
    const float k2 = 0.41f * (1.0f + 0.22f * m_structure);  // wave number 2

    // =================================================================
    // PER-PIXEL RENDER
    // =================================================================

    const float bedBright = 0.25f + 0.75f * m_bed;
    const float master = (ctx.brightness / 255.0f)
                         * lowrisk_ar::clamp01f(m_ar.audioPresence) * mod.brightnessScale;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float dist  = static_cast<float>(centerPairDistance(static_cast<uint16_t>(i)));
        const float distN = dist / static_cast<float>(HALF_LENGTH);

        // Ray-envelope cusp filament maths (preserved from original)
        const float y =
            dist
            + A * sinf(dist * k1 + m_t1)
            + B * sinf(dist * k2 - m_t2 * 1.3f);

        const float dydx =
            1.0f
            + A * k1 * cosf(dist * k1 + m_t1)
            + B * k2 * cosf(dist * k2 - m_t2 * 1.3f);

        float density = 1.0f / (0.20f + fabsf(dydx));
        density = lowrisk_ar::clamp01f(density * 0.85f);

        const float sheet = 0.5f + 0.5f * sinf(y * 0.22f + m_t2);

        // Cusp detection: impact drives the bright caustic lines
        const float cusp = expf(-fabsf(density - 0.90f) * 14.0f) * m_impact;

        // 5-layer per-pixel composition
        const float structuredDensity = 0.72f * density + 0.28f * sheet;
        const float impactAdd = cusp * 0.45f + m_snareImpact * cusp * 0.25f;
        const float memoryAdd = m_memory * density * 0.15f;

        float brightness = bedBright * structuredDensity + impactAdd + memoryAdd;
        brightness = lowrisk_ar::clamp01f(brightness);

        // Output brightness with base floor
        const float base = 0.08f;
        const float out = lowrisk_ar::clamp01f(base + (1.0f - base) * brightness) * master;
        const uint8_t brA = static_cast<uint8_t>(255.0f * out);

        // Hue: purely tonal anchor + spatial offset (no ambient/reactive blend)
        const uint8_t hueA = static_cast<uint8_t>(
            m_ar.tonalHue + distN * 20.0f + density * 32.0f);

        // Strip B: offset hue and brightness for depth
        const uint8_t hueB = static_cast<uint8_t>(
            hueA + 4u + static_cast<uint8_t>(sig.harmonic * 6.0f));
        const uint8_t brB = scale8_video(brA,
            static_cast<uint8_t>(239 + static_cast<uint8_t>(sig.bass * 12.0f)));

        // Write strip A
        ctx.leds[i] = ctx.palette.getColor(hueA, brA);

        // Write strip B
        const int j = i + STRIP_LENGTH;
        if (j < ctx.ledCount) {
            ctx.leds[j] = ctx.palette.getColor(hueB, brB);
        }
    }
}

// =========================================================================
// cleanup / metadata / parameters
// =========================================================================

void LGPWaterCausticsAREffect::cleanup() {
    // No resources to free (no PSRAM allocation)
}

const plugins::EffectMetadata& LGPWaterCausticsAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Water Caustics (5L-AR)",
        "Ray-envelope caustic filaments with 5-layer audio-reactive composition",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPWaterCausticsAREffect::getParameterCount() const {
    return lowrisk_ar::Ar16Controls::parameterCount();
}

const plugins::EffectParameter* LGPWaterCausticsAREffect::getParameter(uint8_t index) const {
    return lowrisk_ar::Ar16Controls::parameter(index);
}

bool LGPWaterCausticsAREffect::setParameter(const char* name, float value) {
    return m_controls.set(name, value);
}

float LGPWaterCausticsAREffect::getParameter(const char* name) const {
    return m_controls.get(name);
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
