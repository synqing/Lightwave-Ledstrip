/**
 * @file LGPIFSBioRelicAREffect.cpp
 * @brief IFS Botanical Relic (5-Layer Audio-Reactive)
 *
 * Barnsley fern IFS with histogram projection and full 5-layer AR composition.
 * Base maths from LGPIFSBioRelicEffect (HolyShitBangersPack), restructured
 * into explicit bed/structure/impact/tonal/memory layers.
 *
 * Physics: 4-transform Barnsley IFS (probabilities 0.01/0.85/0.07/0.07)
 *   Points mirrored for centre-origin symmetry, projected onto 160-LED histogram
 *
 * Audio mapping:
 *   Bed       - RMS-driven relic luminance (tau 0.45s)
 *   Structure - points/frame + decay from flux + saliency (tau 0.15s)
 *   Impact    - beat-triggered vein pulse (decay 0.25s)
 *   Memory    - accumulated vein glow (decay 0.95s)
 *   Tonal     - museum-relic hue from chord
 *
 * Centre-origin compliant. Dual-strip locked.
 */

#include "LGPIFSBioRelicAREffect.h"
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

static inline uint32_t lcg_next(uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return state;
}

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

// =========================================================================
// Construction / init / cleanup
// =========================================================================

LGPIFSBioRelicAREffect::LGPIFSBioRelicAREffect()
    : m_ps(nullptr)
    , m_px(0.0f), m_py(0.0f), m_t(0.0f), m_rng(0xBADC0DEu)
    , m_bed(0.3f), m_structure(0.5f), m_impact(0.0f), m_memory(0.0f)
{
}

bool LGPIFSBioRelicAREffect::init(plugins::EffectContext& ctx) {
    m_controls.resetDefaults();
    m_ar = lowrisk_ar::ArRuntimeState{};
    m_ar.tonalHue = static_cast<float>(ctx.gHue);
    m_ar.modeSmooth = 2.0f;
    m_px   = 0.0f;
    m_py   = 0.0f;
    m_t    = 0.0f;
    m_rng  = 0xBADC0DEu;
    m_bed       = 0.3f;
    m_structure = 0.5f;
    m_impact    = 0.0f;
    m_memory    = 0.0f;

    lightwaveos::effects::cinema::reset();

#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<IFSPsram*>(heap_caps_malloc(sizeof(IFSPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            LW_LOGE("LGPIFSBioRelicAREffect: PSRAM alloc failed");
            return false;
        }
    }
    memset(m_ps, 0, sizeof(IFSPsram));
#endif

    return true;
}

// =========================================================================
// render() - 5-Layer composition
// =========================================================================

void LGPIFSBioRelicAREffect::render(plugins::EffectContext& ctx) {
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

    // Bed: RMS-driven relic luminance (tau 0.45s)
    const float bedTarget = lowrisk_ar::clamp01f(0.20f + 0.80f * sig.rms);
    m_bed = lowrisk_ar::smoothTo(m_bed, bedTarget, dtSignal, 0.45f);

    // Structure: points/frame + decay from flux + saliency (tau 0.15s)
    const float structTarget = lowrisk_ar::clamp01f(
        0.30f + 0.30f * sig.flux + 0.25f * sig.mid + 0.15f * sig.bass);
    m_structure = lowrisk_ar::smoothTo(m_structure, structTarget, dtSignal, 0.15f);

    // Impact: beat-triggered vein pulse (decay 0.25s)
    if (sig.beatTick) m_impact = fmaxf(m_impact, 0.85f);
    m_impact = lowrisk_ar::decay(m_impact, dtSignal, 0.25f);

    // Memory: accumulated vein glow (slow decay, fed by beat + flux)
    m_memory = lowrisk_ar::clamp01f(
        lowrisk_ar::decay(m_memory, dtSignal, 0.95f)
        + 0.08f * m_impact + 0.04f * sig.flux);
    lowrisk_ar::applyBedImpactMemoryMix(
        m_controls,
        static_cast<float>(ctx.rawTotalTimeMs),
        m_bed,
        m_impact,
        m_memory);

    // =================================================================
    // IFS BARNSLEY FERN PHYSICS
    // =================================================================

    m_t += (0.010f + 0.020f * speedNorm) * mod.motionRate;

    // Structure modulates histogram decay rate
    float decay = (0.92f + 0.06f * (1.0f - speedNorm))
                  * (0.90f + 0.10f * (1.0f - m_structure));

#ifndef NATIVE_BUILD
    for (int i = 0; i < STRIP_LENGTH; i++) m_ps->hist[i] *= decay;

    // Structure modulates points per frame
    int P = 220 + (int)(520.0f * speedNorm * (0.6f + 0.6f * m_structure));

    // Barnsley-style IFS (classic fern family), mirrored for centre-origin
    for (int k = 0; k < P; k++) {
        uint32_t r = lcg_next(m_rng);
        float u = (float)(r & 0xFFFFu) / 65535.0f;

        float x = m_px, y = m_py;
        float nx, ny;

        if (u < 0.01f) {
            nx = 0.0f;
            ny = 0.16f * y;
        } else if (u < 0.86f) {
            nx = 0.85f * x + 0.04f * y;
            ny = -0.04f * x + 0.85f * y + 1.6f;
        } else if (u < 0.93f) {
            nx = 0.20f * x - 0.26f * y;
            ny = 0.23f * x + 0.22f * y + 1.6f;
        } else {
            nx = -0.15f * x + 0.28f * y;
            ny = 0.26f * x + 0.24f * y + 0.44f;
        }

        m_px = nx;
        m_py = ny;

        // Mirror x for centre-origin symmetry
        float ax = fabsf(nx);
        float yy = ny;

        float yN = lowrisk_ar::clamp01f(yy / 10.0f);
        float xN = lowrisk_ar::clamp01f(ax / 3.0f);

        float radial = lowrisk_ar::clamp01f(0.10f + 0.78f * (0.70f * yN + 0.30f * xN));

        int bin = (int)lroundf(radial * (STRIP_LENGTH - 1));
        if (bin >= 0 && bin < STRIP_LENGTH) {
            float pulse = 0.85f + 0.15f * sinf(m_t * 0.7f + radial * 6.0f);
            // Impact adds a brightness burst to the IFS scatter
            float impactBoost = 1.0f + 0.5f * m_impact;
            m_ps->hist[bin] += 0.80f * pulse * impactBoost;
        }
    }

    // =================================================================
    // PER-PIXEL RENDER
    // =================================================================

    float maxH = 1e-6f;
    for (int i = 0; i < STRIP_LENGTH; i++) maxH = fmaxf(maxH, m_ps->hist[i]);
    float inv = 1.0f / maxH;

    const float bedBright = 0.20f + 0.80f * m_bed;
    const float master = (ctx.brightness / 255.0f)
                         * lowrisk_ar::clamp01f(m_ar.audioPresence) * mod.brightnessScale;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float dn = distN_from_index(i);
        float glue = centreGlue(i);

        int bin = (int)lroundf(dn * (STRIP_LENGTH - 1));
        bin = (bin < 0) ? 0 : (bin >= STRIP_LENGTH) ? STRIP_LENGTH - 1 : bin;

        float v = lowrisk_ar::clamp01f(m_ps->hist[bin] * inv);
        float veins = powf(v, 1.65f);
        float spec  = powf(veins, 2.2f) * 0.45f;

        // Bed x structured vein geometry
        float veinGeom = lowrisk_ar::clamp01f((0.18f + 0.82f * veins + spec) * glue);

        // Impact: additive vein pulse
        float impactAdd = m_impact * veins * glue * 0.25f;

        // Memory: gentle accumulated glow
        float memoryAdd = m_memory * (0.4f + 0.6f * veins) * 0.12f;

        // Compose: bed x vein + impact + memory
        float brightness = bedBright * veinGeom + impactAdd + memoryAdd;
        brightness = lowrisk_ar::clamp01f(brightness);

        uint8_t br = luminanceToBr(brightness, master);

        // Tonal: museum relic hue with chord anchor
        float hueShift = 10.0f * dn + 35.0f * spec + sig.harmonic * 12.0f;
        uint8_t hue = static_cast<uint8_t>(m_ar.tonalHue + hueShift);

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }
#endif

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

// =========================================================================
// cleanup / metadata / parameters
// =========================================================================

void LGPIFSBioRelicAREffect::cleanup() {
#ifndef NATIVE_BUILD
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
#endif
}

const plugins::EffectMetadata& LGPIFSBioRelicAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP IFS Bio Relic (5L-AR)",
        "Barnsley fern botanical relic with 5-layer audio-reactive composition",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPIFSBioRelicAREffect::getParameterCount() const {
    return lowrisk_ar::Ar16Controls::parameterCount();
}

const plugins::EffectParameter* LGPIFSBioRelicAREffect::getParameter(uint8_t index) const {
    return lowrisk_ar::Ar16Controls::parameter(index);
}

bool LGPIFSBioRelicAREffect::setParameter(const char* name, float value) {
    return m_controls.set(name, value);
}

float LGPIFSBioRelicAREffect::getParameter(const char* name) const {
    return m_controls.get(name);
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
