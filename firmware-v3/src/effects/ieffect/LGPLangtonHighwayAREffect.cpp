/**
 * @file LGPLangtonHighwayAREffect.cpp
 * @brief Langton Highway (5-Layer Audio-Reactive)
 *
 * Langton's ant cellular automaton on 64x64 grid, projected to 1D via drifting
 * diagonal slice. Full 5-layer AR composition:
 *   Bed       - RMS brightness (tau 0.45s)
 *   Structure - step count from rhythmic+rms (tau 0.10s)
 *   Impact    - beat ant-spark amplification (decay 0.20s)
 *   Memory    - highway persistence (tau 0.85s)
 *   Tonal     - chord hue
 *
 * Centre-origin compliant. Dual-strip locked. PSRAM grid allocation.
 */

#include "LGPLangtonHighwayAREffect.h"
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
// Construction / destruction
// =========================================================================

LGPLangtonHighwayAREffect::LGPLangtonHighwayAREffect()
    : m_bed(0.3f)
    , m_structure(0.5f)
    , m_impact(0.0f)
    , m_memory(0.0f)
    , m_grid(nullptr)
    , m_antX(32)
    , m_antY(32)
    , m_antDir(0)
    , m_antStepAccum(0.0f)
    , m_sliceOffset(0.0f)
{
}

LGPLangtonHighwayAREffect::~LGPLangtonHighwayAREffect() {
    cleanup();
}

// =========================================================================
// Init / cleanup
// =========================================================================

bool LGPLangtonHighwayAREffect::init(plugins::EffectContext& ctx) {
    m_controls.resetDefaults();
    m_ar = lowrisk_ar::ArRuntimeState{};
    m_ar.tonalHue = static_cast<float>(ctx.gHue);
    m_ar.modeSmooth = 2.0f;

    m_bed         = 0.3f;
    m_structure   = 0.5f;
    m_impact      = 0.0f;
    m_memory      = 0.0f;

    m_antX = 32;
    m_antY = 32;
    m_antDir = 0;
    m_antStepAccum = 0.0f;
    m_sliceOffset = 0.0f;

    // Allocate PSRAM grid (64*64 = 4096 bytes)
    #ifndef NATIVE_BUILD
    m_grid = (uint8_t*)heap_caps_malloc(W * H, MALLOC_CAP_SPIRAM);
    if (!m_grid) return false;
    #else
    m_grid = new uint8_t[W * H];
    #endif

    // Clear grid (all white)
    for (uint16_t i = 0; i < W * H; i++) {
        m_grid[i] = 255;
    }

    lightwaveos::effects::cinema::reset();
    return true;
}

void LGPLangtonHighwayAREffect::cleanup() {
    if (m_grid) {
        #ifndef NATIVE_BUILD
        heap_caps_free(m_grid);
        #else
        delete[] m_grid;
        #endif
        m_grid = nullptr;
    }
}

// =========================================================================
// Langton's ant step
// =========================================================================

void LGPLangtonHighwayAREffect::stepAnt() {
    if (!m_grid) return;

    // Bounds check
    if (m_antX < 0 || m_antX >= W || m_antY < 0 || m_antY >= H) {
        // Wrap or reset
        m_antX = (m_antX + W) % W;
        m_antY = (m_antY + H) % H;
        return;
    }

    const uint16_t idx = m_antY * W + m_antX;
    const uint8_t cell = m_grid[idx];

    // White → turn right, flip to black, move
    // Black → turn left, flip to white, move
    if (cell > 127) {
        m_antDir = (m_antDir + 1) & 3; // turn right
        m_grid[idx] = 0; // flip to black
    } else {
        m_antDir = (m_antDir + 3) & 3; // turn left (same as -1 mod 4)
        m_grid[idx] = 255; // flip to white
    }

    // Move forward
    switch (m_antDir) {
        case 0: m_antY--; break; // N
        case 1: m_antX++; break; // E
        case 2: m_antY++; break; // S
        case 3: m_antX--; break; // W
    }
}

// =========================================================================
// Drifting diagonal slice projection
// =========================================================================

float LGPLangtonHighwayAREffect::sampleProjection(float offset) {
    if (!m_grid) return 0.0f;

    // Diagonal slice: y = x + offset (wrapped)
    float x = offset;
    x = x - floorf(x / static_cast<float>(W)) * static_cast<float>(W); // wrap to [0, W)

    int xi = static_cast<int>(x);
    int yi = (xi + static_cast<int>(m_sliceOffset)) % H;

    if (xi < 0 || xi >= W) return 0.0f;
    if (yi < 0 || yi >= H) yi = (yi + H) % H;

    const uint16_t idx = yi * W + xi;
    return m_grid[idx] / 255.0f;
}

// =========================================================================
// render() — 5-Layer composition
// =========================================================================

void LGPLangtonHighwayAREffect::render(plugins::EffectContext& ctx) {
    if (!m_grid) return;

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
        0.25f + 0.75f * sig.rms, dtSignal, 0.45f);

    // Structure: step rate from rhythmic + rms (tau 0.10s)
    const float structTarget = lowrisk_ar::clamp01f(
        0.30f + 0.45f * sig.rhythmic + 0.25f * sig.rms);
    m_structure = lowrisk_ar::smoothTo(m_structure, structTarget, dtSignal, 0.10f);

    // Impact: beat-triggered ant spark (decay 0.20s)
    if (sig.beatTick) m_impact = fmaxf(m_impact, 1.0f);
    m_impact = lowrisk_ar::decay(m_impact, dtSignal, 0.20f);

    // Memory: highway persistence (slow decay, fed by structure)
    m_memory = lowrisk_ar::clamp01f(
        lowrisk_ar::decay(m_memory, dtSignal, 0.85f)
        + 0.08f * m_structure);
    lowrisk_ar::applyBedImpactMemoryMix(
        m_controls,
        static_cast<float>(ctx.rawTotalTimeMs),
        m_bed,
        m_impact,
        m_memory);

    // =================================================================
    // ANT STEPPING
    // =================================================================

    // Step count controlled by structure layer (0.5-8 steps/frame at speed 50)
    const float stepsPerSec = (0.5f + 7.5f * m_structure) * speedNorm;
    m_antStepAccum += stepsPerSec * dtVisual;

    while (m_antStepAccum >= 1.0f) {
        stepAnt();
        m_antStepAccum -= 1.0f;
    }

    // Slice drift
    const float driftRate = (0.15f + 0.35f * speedNorm) * mod.motionRate;
    m_sliceOffset += driftRate * dtVisual;
    m_sliceOffset = fract(m_sliceOffset / static_cast<float>(H)) * static_cast<float>(H);

    // =================================================================
    // PER-PIXEL RENDER
    // =================================================================

    const float mid    = (STRIP_LENGTH - 1) * 0.5f;
    const float invMid = 1.0f / mid;
    const float master = (ctx.brightness / 255.0f)
                         * lowrisk_ar::clamp01f(m_ar.audioPresence) * mod.brightnessScale;

    // Ant position in grid space
    const float antXf = static_cast<float>(m_antX);
    const float antYf = static_cast<float>(m_antY);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float dmid  = static_cast<float>(i) - mid;
        const float distN = fabsf(dmid) * invMid;

        // Map strip position to grid diagonal
        const float gridPos = distN * static_cast<float>(W);

        // Sample grid at this position (with neighbourhood blur)
        float highway = 0.0f;
        for (int blur = -1; blur <= 1; blur++) {
            float samplePos = gridPos + static_cast<float>(blur) * 1.5f;
            highway += sampleProjection(samplePos) * (blur == 0 ? 0.5f : 0.25f);
        }
        highway = lowrisk_ar::clamp01f(highway);

        // Centre glue (stronger adhesion near origin)
        const float glue = 0.40f + 0.60f * expf(-(dmid * dmid) * 0.0018f);

        // Ant spark (proximity to ant position in grid)
        const float antDist = sqrtf(
            (gridPos - antXf) * (gridPos - antXf) +
            (m_sliceOffset - antYf) * (m_sliceOffset - antYf));
        const float antSpark = expf(-antDist * 0.12f);

        // Bed × highway field
        float structuredField = m_bed * highway * glue;

        // Impact × ant spark (additive burst)
        float impactAdd = m_impact * antSpark * 0.45f;

        // Memory (gentle additive glow)
        float memoryAdd = m_memory * (0.4f + 0.6f * highway) * 0.15f;

        // Compose: bed × highway + impact + memory
        float brightness = structuredField + impactAdd + memoryAdd;
        brightness = lowrisk_ar::clamp01f(brightness);

        const uint8_t br = luminanceToBr(brightness, master);

        // Tonal hue: chord-driven + highway modulation
        const uint8_t hue = static_cast<uint8_t>(
            m_ar.tonalHue + highway * 40.0f + sig.harmonic * 15.0f);

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

// =========================================================================
// Metadata / parameters
// =========================================================================

const plugins::EffectMetadata& LGPLangtonHighwayAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Langton Highway (5L-AR)",
        "Langton's ant cellular automaton with 5-layer audio-reactive composition",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPLangtonHighwayAREffect::getParameterCount() const {
    return lowrisk_ar::Ar16Controls::parameterCount();
}

const plugins::EffectParameter* LGPLangtonHighwayAREffect::getParameter(uint8_t index) const {
    return lowrisk_ar::Ar16Controls::parameter(index);
}

bool LGPLangtonHighwayAREffect::setParameter(const char* name, float value) {
    return m_controls.set(name, value);
}

float LGPLangtonHighwayAREffect::getParameter(const char* name) const {
    return m_controls.get(name);
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
