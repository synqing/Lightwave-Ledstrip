/**
 * @file LGPLorenzRibbonAREffect.cpp
 * @brief Lorenz Ribbon (5-Layer Audio-Reactive)
 *
 * Lorenz attractor trail projected radially with full 5-layer AR composition.
 * Base maths from LGPLorenzRibbonEffect (HolyShitBangersPack), restructured
 * into explicit bed/structure/impact/tonal/memory layers.
 *
 * Physics: Classic Lorenz system (sigma=10, rho=28, beta=8/3)
 *   96-sample trail with age-dependent fade, projected as ribbon thickness
 *
 * Audio mapping:
 *   Bed       - RMS-driven ribbon brightness (tau 0.40s)
 *   Structure - sub-steps + thickness from bass + flux (tau 0.12s)
 *   Impact    - beat-triggered trajectory burst (decay 0.20s)
 *   Memory    - tail persistence (decay 0.85s)
 *   Tonal     - chaotic energy hue from chord
 *
 * Centre-origin compliant. Dual-strip locked.
 */

#include "LGPLorenzRibbonAREffect.h"
#include "LGPFilmPost.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#endif

#include "../../utils/Log.h"

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

static inline float distN_from_index(int i) {
    const float mid = (STRIP_LENGTH - 1) * 0.5f;
    return fabsf((float)i - mid) / mid;
}

static inline float centreGlue(int i) {
    const float mid = (STRIP_LENGTH - 1) * 0.5f;
    const float d = (float)i - mid;
    return 0.30f + 0.70f * expf(-(d * d) * 0.0016f);
}

static inline uint8_t luminanceToBr(float wave01, float master) {
    const float base = 0.055f;
    float out = lowrisk_ar::clamp01f(base + (1.0f - base) * lowrisk_ar::clamp01f(wave01));
    out = out / (1.0f + 0.60f * out);  // filmic shoulder
    out *= master;
    return static_cast<uint8_t>(lowrisk_ar::clampf(out * 255.0f, 0.0f, 255.0f));
}

// Rational approximation to exp(-x), accurate enough for ribbon falloff at a fraction of expf() cost.
static inline float fastExpFalloff(float x) {
    const float xClamped = (x < 0.0f) ? 0.0f : x;
    return 1.0f / (1.0f + xClamped + 0.48f * xClamped * xClamped);
}

// =========================================================================
// Construction / init / cleanup
// =========================================================================

LGPLorenzRibbonAREffect::LGPLorenzRibbonAREffect()
    : m_ps(nullptr)
    , m_x(1.0f), m_y(0.0f), m_z(0.0f), m_t(0.0f), m_head(0)
    , m_bed(0.3f), m_structure(0.5f), m_impact(0.0f), m_memory(0.0f)
{
}

bool LGPLorenzRibbonAREffect::init(plugins::EffectContext& ctx) {
    m_controls.resetDefaults();
    m_ar = lowrisk_ar::ArRuntimeState{};
    m_ar.tonalHue = static_cast<float>(ctx.gHue);
    m_ar.modeSmooth = 2.0f;
    m_x    = 1.0f;
    m_y    = 0.0f;
    m_z    = 0.0f;
    m_t    = 0.0f;
    m_head = 0;
    m_bed       = 0.3f;
    m_structure = 0.5f;
    m_impact    = 0.0f;
    m_memory    = 0.0f;

    lightwaveos::effects::cinema::reset();

#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<LorenzPsram*>(heap_caps_malloc(sizeof(LorenzPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            LW_LOGE("LGPLorenzRibbonAREffect: PSRAM alloc failed");
            return false;
        }
    }
    memset(m_ps, 0, sizeof(LorenzPsram));
#endif

    return true;
}

// =========================================================================
// render() - 5-Layer composition
// =========================================================================

void LGPLorenzRibbonAREffect::render(plugins::EffectContext& ctx) {
#ifndef NATIVE_BUILD
    if (!m_ps) return;
#endif

    // --- Timing ---
    const float dtSignal = AudioReactivePolicy::signalDt(ctx);
    const float dtVisual = AudioReactivePolicy::visualDt(ctx);
    (void)dtVisual;
    const float speedNorm = ctx.speed / 50.0f;

    // --- Signals ---
    const lowrisk_ar::ArSignalSnapshot sig =
        lowrisk_ar::updateSignals(m_ar, ctx, m_controls, dtSignal);
    const lowrisk_ar::ArModulationProfile mod =
        lowrisk_ar::buildModulation(m_controls, sig, m_ar, ctx);
    m_ar.tonalHue = mod.baseHue;

    // =================================================================
    // LAYER UPDATES
    // =================================================================

    // Bed: RMS-driven ribbon brightness (tau 0.40s)
    const float bedTarget = lowrisk_ar::clamp01f(0.25f + 0.75f * sig.rms);
    m_bed = lowrisk_ar::smoothTo(m_bed, bedTarget, dtSignal, 0.40f);

    // Structure: sub-step intensity + ribbon thickness from bass + flux (tau 0.12s)
    const float structTarget = lowrisk_ar::clamp01f(
        0.30f + 0.35f * sig.bass + 0.25f * sig.flux + 0.10f * sig.rhythmic);
    m_structure = lowrisk_ar::smoothTo(m_structure, structTarget, dtSignal, 0.12f);

    // Impact: beat-triggered trajectory burst (decay 0.20s)
    if (sig.beatTick) m_impact = fmaxf(m_impact, 0.85f);
    m_impact = lowrisk_ar::decay(m_impact, dtSignal, 0.20f);

    // Memory: accumulated tail persistence (slow decay, fed by beat + flux)
    m_memory = lowrisk_ar::clamp01f(
        lowrisk_ar::decay(m_memory, dtSignal, 0.85f)
        + 0.10f * m_impact + 0.05f * sig.flux);
    lowrisk_ar::applyBedImpactMemoryMix(
        m_controls,
        static_cast<float>(ctx.rawTotalTimeMs),
        m_bed,
        m_impact,
        m_memory);

    // =================================================================
    // LORENZ ATTRACTOR PHYSICS
    // =================================================================

    const float sigma = 10.0f;
    const float rho   = 28.0f;
    const float beta  = 8.0f / 3.0f;

    // Structure modulates integration speed and sub-step count
    float dt = (0.0065f + 0.010f * speedNorm) * (0.7f + 0.5f * m_structure) * mod.motionRate;
    int sub = 2 + static_cast<int>(3.5f * speedNorm * (0.6f + 0.4f * m_structure)
                                    * (0.75f + 0.50f * mod.motionDepth));
    if (sub > 7) sub = 7;

#ifndef NATIVE_BUILD
    for (int s = 0; s < sub; s++) {
        float dx = sigma * (m_y - m_x);
        float dy = m_x * (rho - m_z) - m_y;
        float dz = m_x * m_y - beta * m_z;

        m_x += dx * dt;
        m_y += dy * dt;
        m_z += dz * dt;
        m_t += dt;

        float r = (0.55f * fabsf(m_x) / 22.0f) + (0.45f * m_z / 55.0f);
        r = lowrisk_ar::clamp01f(r);

        m_ps->trail[m_head] = r;
        m_head = (uint8_t)((m_head + 1) % TRAIL_N);
    }
#endif

    // =================================================================
    // PER-PIXEL RENDER
    // =================================================================

    const float bedBright = 0.25f + 0.75f * m_bed;
    const float master = (ctx.brightness / 255.0f)
                         * lowrisk_ar::clamp01f(m_ar.audioPresence) * mod.brightnessScale;

    // Structure modulates ribbon thickness
    const float thickBase = 0.040f + 0.030f * (1.0f - speedNorm);
    const float thick = lowrisk_ar::clampf(
        thickBase * (0.65f + 0.55f * m_structure) * (0.75f + 0.50f * mod.motionDepth),
        0.020f, 0.140f);

    const int trailStride = (speedNorm > 1.50f) ? 3 : ((speedNorm > 0.95f) ? 2 : 1);
    static float trailR[TRAIL_N];
    static float trailFade[TRAIL_N];
    int trailSamples = 0;
    for (int k = 0; k < TRAIL_N; k += trailStride) {
        int idx = static_cast<int>(m_head) - 1 - k;
        while (idx < 0) idx += TRAIL_N;
        const float age = static_cast<float>(k) / static_cast<float>(TRAIL_N - 1);  // 0 newest .. 1 oldest
        float fade = 1.0f - age;
        fade = fade * fade * (0.70f + 0.30f * fade);  // ~pow(1-age, 1.6) without powf()
        if (fade < 0.02f) {
            break;
        }
        trailR[trailSamples] = m_ps->trail[idx];
        trailFade[trailSamples] = fade;
        ++trailSamples;
    }

#ifndef NATIVE_BUILD
    for (int i = 0; i < STRIP_LENGTH; i++) {
        float dn = distN_from_index(i);
        float glue = centreGlue(i);
        float best = 0.0f;

        for (int k = 0; k < trailSamples; k++) {
            const float g = fastExpFalloff(fabsf(dn - trailR[k]) / thick) * trailFade[k];
            best = fmaxf(best, g);
        }

        // Thin specular edge for ribbon feel
        float spec = best * best * sqrtf(best) * 0.55f;

        // Bed x structured geometry
        float ribbonGeom = lowrisk_ar::clamp01f((0.22f + 0.78f * best + spec) * glue);

        // Impact: additive trajectory burst
        float impactAdd = m_impact * best * glue * 0.30f;

        // Memory: gentle additive persistence
        float memoryAdd = m_memory * (0.4f + 0.6f * best) * 0.15f;

        // Compose: bed x ribbon + impact + memory
        float brightness = bedBright * ribbonGeom + impactAdd + memoryAdd;
        brightness = lowrisk_ar::clamp01f(brightness);

        uint8_t br = luminanceToBr(brightness, master);

        // Tonal: chaotic energy drives hue with chord anchor
        float energy = lowrisk_ar::clamp01f(
            (fabsf(m_x) / 22.0f) * 0.6f + (fabsf(m_y) / 28.0f) * 0.4f);
        float hueShift = 25.0f * energy + 45.0f * spec + sig.harmonic * 15.0f;
        uint8_t hue = static_cast<uint8_t>(m_ar.tonalHue + hueShift);

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }
#endif

    lightwaveos::effects::cinema::apply(ctx, speedNorm, lightwaveos::effects::cinema::QualityPreset::LIGHTWEIGHT);
}

// =========================================================================
// cleanup / metadata / parameters
// =========================================================================

void LGPLorenzRibbonAREffect::cleanup() {
#ifndef NATIVE_BUILD
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
#endif
}

const plugins::EffectMetadata& LGPLorenzRibbonAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Lorenz Ribbon (5L-AR)",
        "Chaotic attractor ribbon with 5-layer audio-reactive composition",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPLorenzRibbonAREffect::getParameterCount() const {
    return lowrisk_ar::Ar16Controls::parameterCount();
}

const plugins::EffectParameter* LGPLorenzRibbonAREffect::getParameter(uint8_t index) const {
    return lowrisk_ar::Ar16Controls::parameter(index);
}

bool LGPLorenzRibbonAREffect::setParameter(const char* name, float value) {
    return m_controls.set(name, value);
}

float LGPLorenzRibbonAREffect::getParameter(const char* name) const {
    return m_controls.get(name);
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
