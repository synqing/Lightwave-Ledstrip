/**
 * @file LGPRule30CathedralAREffect.cpp
 * @brief Rule 30 Cathedral (5-Layer Audio-Reactive)
 *
 * Cellular automaton with Rule 30 (l ^ (c | r)) rendered as glowing cathedral
 * arches with full 5-layer AR composition. Steps per frame scale with bass+rhythmic.
 * Blur neighbours for rendering, neighbourhood-based hue shift.
 *
 * Centre-origin compliant. Dual-strip locked. PSRAM CA buffers.
 */

#include "LGPRule30CathedralAREffect.h"
#include "LGPFilmPost.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#endif

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
    float out = lowrisk_ar::clamp01f(base + (1.0f - base) * lowrisk_ar::clamp01f(wave01)) * master;
    return static_cast<uint8_t>(255.0f * out);
}

// =========================================================================
// Construction / destruction
// =========================================================================

LGPRule30CathedralAREffect::LGPRule30CathedralAREffect()
    : m_t(0.0f)
    , m_stepAccum(0.0f)
    , m_bed(0.3f)
    , m_structure(0.5f)
    , m_impact(0.0f)
    , m_memory(0.0f)
#ifndef NATIVE_BUILD
    , m_ps(nullptr)
#endif
{
}

LGPRule30CathedralAREffect::~LGPRule30CathedralAREffect() {
    cleanup();
}

// =========================================================================
// CA helpers
// =========================================================================

void LGPRule30CathedralAREffect::seedCA() {
#ifndef NATIVE_BUILD
    if (!m_ps) return;

    // Clear field
    for (int i = 0; i < STRIP_LENGTH; i++) {
        m_ps->cells[i] = 0;
        m_ps->next[i] = 0;
    }

    // Centre seed
    const int mid = (STRIP_LENGTH - 1) / 2;
    m_ps->cells[mid] = 1;
#endif
}

void LGPRule30CathedralAREffect::stepCA() {
#ifndef NATIVE_BUILD
    if (!m_ps) return;

    // Rule 30: new = l ^ (c | r)
    for (int i = 0; i < STRIP_LENGTH; i++) {
        const uint8_t l = (i > 0) ? m_ps->cells[i - 1] : 0;
        const uint8_t c = m_ps->cells[i];
        const uint8_t r = (i < STRIP_LENGTH - 1) ? m_ps->cells[i + 1] : 0;

        m_ps->next[i] = l ^ (c | r);
    }

    // Swap buffers
    for (int i = 0; i < STRIP_LENGTH; i++) {
        m_ps->cells[i] = m_ps->next[i];
    }
#endif
}

// =========================================================================
// Init / cleanup
// =========================================================================

bool LGPRule30CathedralAREffect::init(plugins::EffectContext& ctx) {
    m_controls.resetDefaults();
    m_ar = lowrisk_ar::ArRuntimeState{};
    m_ar.tonalHue = static_cast<float>(ctx.gHue);
    m_ar.modeSmooth = 2.0f;

    m_t           = 0.0f;
    m_stepAccum   = 0.0f;
    m_bed         = 0.3f;
    m_structure   = 0.5f;
    m_impact      = 0.0f;
    m_memory      = 0.0f;

#ifndef NATIVE_BUILD
    // Allocate PSRAM CA buffers
    if (!m_ps) {
        m_ps = static_cast<Rule30Psram*>(
            heap_caps_malloc(sizeof(Rule30Psram), MALLOC_CAP_SPIRAM)
        );
    }
    if (m_ps) {
        seedCA();
    }
#endif

    lightwaveos::effects::cinema::reset();
    return true;
}

void LGPRule30CathedralAREffect::cleanup() {
#ifndef NATIVE_BUILD
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
#endif
}

// =========================================================================
// render() — 5-Layer composition
// =========================================================================

void LGPRule30CathedralAREffect::render(plugins::EffectContext& ctx) {
#ifndef NATIVE_BUILD
    if (!m_ps) return;
#endif

    // --- Timing ---
    const float dtSignal = AudioReactivePolicy::signalDt(ctx);
    const float dtVisual = AudioReactivePolicy::visualDt(ctx);
    const float speedNorm = ctx.speed / 50.0f;

    // --- Signals ---
    const lowrisk_ar::ArSignalSnapshot sig =
        lowrisk_ar::updateSignals(m_ar, ctx, m_controls, dtSignal);

    // =================================================================
    // LAYER UPDATES
    // =================================================================

    // Bed: slow RMS-driven base brightness (tau 0.40s)
    m_bed = lowrisk_ar::smoothTo(m_bed,
        0.20f + 0.80f * sig.rms, dtSignal, 0.40f);

    // Structure: CA step rate from rhythmic + bass (tau 0.12s)
    const float structTarget = lowrisk_ar::clamp01f(
        0.25f + 0.45f * sig.bass + 0.30f * sig.rhythmic);
    m_structure = lowrisk_ar::smoothTo(m_structure, structTarget, dtSignal, 0.12f);

    // Impact: beat-triggered reset burst (decay 0.22s)
    if (sig.beatTick) {
        m_impact = fmaxf(m_impact, 1.0f);
#ifndef NATIVE_BUILD
        // Beat reseeds the CA
        seedCA();
#endif
    }
    m_impact = lowrisk_ar::decay(m_impact, dtSignal, 0.22f);

    // Memory: textile persistence (tau 0.70s)
    m_memory = lowrisk_ar::clamp01f(
        lowrisk_ar::decay(m_memory, dtSignal, 0.70f)
        + 0.15f * m_impact + 0.08f * sig.flux);

    // =================================================================
    // CA EVOLUTION
    // =================================================================

    // Step rate: 0.5 to 12 steps per frame at speed 100
    const float baseStepsPerFrame = (0.5f + 11.5f * speedNorm) * m_controls.motionRate();
    const float structureMod = (0.60f + 0.40f * m_structure);
    const float stepsPerFrame = baseStepsPerFrame * structureMod;

    m_stepAccum += stepsPerFrame * dtVisual;

    while (m_stepAccum >= 1.0f) {
        stepCA();
        m_stepAccum -= 1.0f;
    }

    m_t += dtVisual;

    // =================================================================
    // RENDER WITH NEIGHBOURHOOD BLUR
    // =================================================================

    const float mid    = (STRIP_LENGTH - 1) * 0.5f;
    const float bedBright = 0.20f + 0.80f * m_bed;
    const float master = (ctx.brightness / 255.0f)
                         * lowrisk_ar::clamp01f(m_ar.audioPresence);

    for (int i = 0; i < STRIP_LENGTH; i++) {
#ifndef NATIVE_BUILD
        // 3-tap neighbourhood blur for smooth arches
        const float l = (i > 0) ? static_cast<float>(m_ps->cells[i - 1]) : 0.0f;
        const float c = static_cast<float>(m_ps->cells[i]);
        const float r = (i < STRIP_LENGTH - 1) ? static_cast<float>(m_ps->cells[i + 1]) : 0.0f;

        const float caValue = (0.25f * l + 0.50f * c + 0.25f * r);
#else
        const float caValue = 0.5f;
#endif

        const float dmid  = static_cast<float>(i) - mid;

        // Centre glue (cathedral foundation)
        const float glue = 0.30f + 0.70f * expf(-(dmid * dmid) * 0.0012f);

        // Neighbourhood-based hue shift (local CA density)
        const float neighbourhoodSum = l + c + r;
        const float hueShift = neighbourhoodSum * 15.0f;

        // Bed × CA geometry
        float structuredGeom = caValue * glue;

        // Impact: additive burst at CA cells
        float impactAdd = m_impact * caValue * 0.40f;

        // Memory: gentle glow
        float memoryAdd = m_memory * (0.4f + 0.6f * caValue) * 0.20f;

        // Compose: bed × structure + impact + memory
        float brightness = bedBright * structuredGeom + impactAdd + memoryAdd;
        brightness = lowrisk_ar::clamp01f(brightness);

        const uint8_t br = luminanceToBr(brightness, master);

        // Tonal hue: chord anchor + neighbourhood density
        const uint8_t hue = static_cast<uint8_t>(
            m_ar.tonalHue + hueShift + sig.harmonic * 18.0f);

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

// =========================================================================
// Metadata / parameters
// =========================================================================

const plugins::EffectMetadata& LGPRule30CathedralAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Rule 30 Cathedral (5L-AR)",
        "Cellular automaton cathedral with 5-layer audio-reactive composition",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPRule30CathedralAREffect::getParameterCount() const {
    return lowrisk_ar::Ar16Controls::parameterCount();
}

const plugins::EffectParameter* LGPRule30CathedralAREffect::getParameter(uint8_t index) const {
    return lowrisk_ar::Ar16Controls::parameter(index);
}

bool LGPRule30CathedralAREffect::setParameter(const char* name, float value) {
    return m_controls.set(name, value);
}

float LGPRule30CathedralAREffect::getParameter(const char* name) const {
    return m_controls.get(name);
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
