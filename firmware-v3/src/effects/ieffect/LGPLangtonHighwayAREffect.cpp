/**
 * @file LGPLangtonHighwayAREffect.cpp
 * @brief Langton Highway (5-Layer AR) — REWRITTEN
 *
 * Langton's ant cellular automaton on 64x64 grid, projected to 1D via drifting
 * diagonal slice. Direct audio-reactive composition.
 *
 * Divergence fixes: Direct ControlBus reads, single-stage smoothing (50ms),
 * asymmetric max follower (58ms/500ms, floor 0.04), no brightness floors,
 * beatStrength() continuous, squared brightness for punch.
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
// Constants
// =========================================================================

static constexpr float kTwoPi = 6.28318530717958647692f;
static constexpr float kPi    = 3.14159265358979323846f;
static constexpr float kBassTau    = 0.050f;
static constexpr float kTrebleTau  = 0.040f;
static constexpr float kChromaTau  = 0.300f;
static constexpr float kFollowerAttackTau = 0.058f;
static constexpr float kFollowerDecayTau  = 0.500f;
static constexpr float kFollowerFloor     = 0.04f;
static constexpr float kImpactDecayTau = 0.180f;

// =========================================================================
// Local helpers
// =========================================================================

static inline float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}
static inline float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static inline float fract(float x) { return x - floorf(x); }

static inline void writeDualLocked(plugins::EffectContext& ctx, int i, const CRGB& c) {
    ctx.leds[i] = c;
    int j = i + STRIP_LENGTH;
    if (j < (int)ctx.ledCount) ctx.leds[j] = c;
}

// =========================================================================
// Construction / destruction
// =========================================================================

LGPLangtonHighwayAREffect::LGPLangtonHighwayAREffect()
    : m_grid(nullptr)
{
    // Per-zone arrays are default-initialised inline in the header.
}

LGPLangtonHighwayAREffect::~LGPLangtonHighwayAREffect() {
    cleanup();
}

// =========================================================================
// Init / cleanup
// =========================================================================

bool LGPLangtonHighwayAREffect::init(plugins::EffectContext& ctx) {
    // Reset ALL zone slots
    for (uint8_t zi = 0; zi < kMaxZones; ++zi) {
        m_bass[zi] = 0.0f;
        m_treble[zi] = 0.0f;
        m_chromaAngle[zi] = 0.0f;
        m_bassMax[zi] = 0.15f;
        m_trebleMax[zi] = 0.15f;
        m_impact[zi] = 0.0f;

        m_antX[zi] = 32;
        m_antY[zi] = 32;
        m_antDir[zi] = 0;
        m_antStepAccum[zi] = 0.0f;
        m_sliceOffset[zi] = 0.0f;
    }

    // Allocate per-zone PSRAM grid pool (kMaxZones * W * H bytes = 16 KB).
    #ifndef NATIVE_BUILD
    if (!m_grid) {
        m_grid = (uint8_t*)heap_caps_malloc(static_cast<size_t>(kMaxZones) * kGridBytes,
                                            MALLOC_CAP_SPIRAM);
        if (!m_grid) return false;
    }
    #else
    if (!m_grid) {
        m_grid = new uint8_t[static_cast<size_t>(kMaxZones) * kGridBytes];
    }
    #endif

    // Clear all zones' grids (all white)
    for (size_t i = 0; i < static_cast<size_t>(kMaxZones) * kGridBytes; i++) {
        m_grid[i] = 255;
    }

    lightwaveos::effects::cinema::reset();
    return true;
}

void LGPLangtonHighwayAREffect::cleanup() {
    // Retain PSRAM across effect lifetime; freeing on cleanup() fragments PSRAM under rapid cycling
    // and forces a fresh malloc on next init(). init() already guards with if (!m_grid).
    // Note: NATIVE_BUILD path (delete[] m_grid) also removed — native tests use static pool instances.
}

// =========================================================================
// Langton's ant step (per-zone — operates on zone z's grid slice)
// =========================================================================

void LGPLangtonHighwayAREffect::stepAnt(int z) {
    if (!m_grid) return;

    uint8_t* grid = m_grid + static_cast<size_t>(z) * kGridBytes;

    // Bounds check (per-zone ant coords)
    if (m_antX[z] < 0 || m_antX[z] >= W || m_antY[z] < 0 || m_antY[z] >= H) {
        // Wrap or reset
        m_antX[z] = (m_antX[z] + W) % W;
        m_antY[z] = (m_antY[z] + H) % H;
        return;
    }

    const uint16_t idx = static_cast<uint16_t>(m_antY[z]) * W + m_antX[z];
    const uint8_t cell = grid[idx];

    // White -> turn right, flip to black, move
    // Black -> turn left, flip to white, move
    if (cell > 127) {
        m_antDir[z] = (m_antDir[z] + 1) & 3; // turn right
        grid[idx] = 0; // flip to black
    } else {
        m_antDir[z] = (m_antDir[z] + 3) & 3; // turn left (same as -1 mod 4)
        grid[idx] = 255; // flip to white
    }

    // Move forward
    switch (m_antDir[z]) {
        case 0: m_antY[z]--; break; // N
        case 1: m_antX[z]++; break; // E
        case 2: m_antY[z]++; break; // S
        case 3: m_antX[z]--; break; // W
    }
}

// =========================================================================
// Drifting diagonal slice projection (per-zone)
// =========================================================================

float LGPLangtonHighwayAREffect::sampleProjection(int z, float offset) {
    if (!m_grid) return 0.0f;

    const uint8_t* grid = m_grid + static_cast<size_t>(z) * kGridBytes;

    // Diagonal slice: y = x + offset (wrapped)
    float x = offset;
    x = x - floorf(x / static_cast<float>(W)) * static_cast<float>(W); // wrap to [0, W)

    int xi = static_cast<int>(x);
    int yi = (xi + static_cast<int>(m_sliceOffset[z])) % H;

    if (xi < 0 || xi >= W) return 0.0f;
    if (yi < 0 || yi >= H) yi = (yi + H) % H;

    const uint16_t idx = static_cast<uint16_t>(yi) * W + xi;
    return grid[idx] / 255.0f;
}

// =========================================================================
// render() — direct audio-reactive composition
// =========================================================================

void LGPLangtonHighwayAREffect::render(plugins::EffectContext& ctx) {
    if (!m_grid) return;

    // Per-zone state selector. 0xFF (global render) falls back to slot 0.
    const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;

    const float dt = ctx.getSafeRawDeltaSeconds();
    const float dtVis = ctx.getSafeDeltaSeconds();
    const float speedNorm = ctx.speed / 50.0f;

    // STEP 1: Direct ControlBus reads
    const float rawBass = ctx.audio.available ? ctx.audio.bass() : 0.0f;
    const float rawTreble = ctx.audio.available ? ctx.audio.treble() : 0.0f;
    const float beatStr = ctx.audio.available ? ctx.audio.beatStrength() : 0.0f;
    const float silScale = ctx.audio.available ? ctx.audio.silentScale() : 0.0f;
    const float* chroma = ctx.audio.available ? ctx.audio.chroma() : nullptr;

    // STEP 2: Single-stage smoothing (per-zone)
    m_bass[z] += (rawBass - m_bass[z]) * (1.0f - expf(-dt / kBassTau));
    m_treble[z] += (rawTreble - m_treble[z]) * (1.0f - expf(-dt / kTrebleTau));

    // Circular chroma EMA (per-zone)
    if (chroma) {
        float sx = 0.0f, sy = 0.0f;
        for (int i = 0; i < 12; i++) {
            float angle = static_cast<float>(i) * (kTwoPi / 12.0f);
            sx += chroma[i] * cosf(angle);
            sy += chroma[i] * sinf(angle);
        }
        if (sx * sx + sy * sy > 0.0001f) {
            float target = atan2f(sy, sx);
            if (target < 0.0f) target += kTwoPi;
            float delta = target - m_chromaAngle[z];
            while (delta > kPi) delta -= kTwoPi;
            while (delta < -kPi) delta += kTwoPi;
            m_chromaAngle[z] += delta * (1.0f - expf(-dt / kChromaTau));
            if (m_chromaAngle[z] < 0.0f) m_chromaAngle[z] += kTwoPi;
            if (m_chromaAngle[z] >= kTwoPi) m_chromaAngle[z] -= kTwoPi;
        }
    }

    // STEP 3: Max followers (per-zone)
    {
        float aA = 1.0f - expf(-dt / kFollowerAttackTau);
        float dA = 1.0f - expf(-dt / kFollowerDecayTau);
        if (m_bass[z] > m_bassMax[z]) m_bassMax[z] += (m_bass[z] - m_bassMax[z]) * aA;
        else m_bassMax[z] += (m_bass[z] - m_bassMax[z]) * dA;
        if (m_bassMax[z] < kFollowerFloor) m_bassMax[z] = kFollowerFloor;

        if (m_treble[z] > m_trebleMax[z]) m_trebleMax[z] += (m_treble[z] - m_trebleMax[z]) * aA;
        else m_trebleMax[z] += (m_treble[z] - m_trebleMax[z]) * dA;
        if (m_trebleMax[z] < kFollowerFloor) m_trebleMax[z] = kFollowerFloor;
    }
    const float normBass = clamp01(m_bass[z] / m_bassMax[z]);
    const float normTreble = clamp01(m_treble[z] / m_trebleMax[z]);

    // STEP 4: Impact — continuous beatStrength rise, exponential decay (per-zone)
    if (beatStr > m_impact[z]) m_impact[z] = beatStr;
    m_impact[z] *= expf(-dt / kImpactDecayTau);

    const float beatMod = 0.3f + 0.7f * beatStr;

    // =================================================================
    // ANT STEPPING (per-zone — each zone has its own ant and grid)
    // =================================================================

    // Step count controlled by normalised audio (rhythmic drive from bass + treble)
    const float rhythmicDrive = clamp01(0.30f + 0.45f * normBass + 0.25f * normTreble);
    const float stepsPerSec = (0.5f + 7.5f * rhythmicDrive) * speedNorm;
    m_antStepAccum[z] += stepsPerSec * dtVis;

    while (m_antStepAccum[z] >= 1.0f) {
        stepAnt(z);
        m_antStepAccum[z] -= 1.0f;
    }

    // Slice drift — bass-driven rate (per-zone)
    const float motionRate = 0.6f + 0.8f * normBass + 0.3f * m_impact[z];
    const float driftRate = (0.15f + 0.35f * speedNorm) * motionRate;
    m_sliceOffset[z] += driftRate * dtVis;
    m_sliceOffset[z] = fract(m_sliceOffset[z] / static_cast<float>(H)) * static_cast<float>(H);

    // =================================================================
    // PER-PIXEL RENDER
    // =================================================================

    const float mid    = (STRIP_LENGTH - 1) * 0.5f;
    const float invMid = 1.0f / mid;

    // Hue from chroma (per-zone)
    uint8_t baseHue = static_cast<uint8_t>(m_chromaAngle[z] * (255.0f / kTwoPi)) + ctx.gHue;

    // Ant position in grid space (per-zone)
    const float antXf = static_cast<float>(m_antX[z]);
    const float antYf = static_cast<float>(m_antY[z]);
    const float sliceOffsetZ = m_sliceOffset[z];

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float dmid  = static_cast<float>(i) - mid;
        const float distN = fabsf(dmid) * invMid;

        // Map strip position to grid diagonal
        const float gridPos = distN * static_cast<float>(W);

        // Sample grid at this position (with neighbourhood blur, per-zone)
        float highway = 0.0f;
        for (int blur = -1; blur <= 1; blur++) {
            float samplePos = gridPos + static_cast<float>(blur) * 1.5f;
            highway += sampleProjection(z, samplePos) * (blur == 0 ? 0.5f : 0.25f);
        }
        highway = clamp01(highway);

        // Centre glue (stronger adhesion near origin)
        const float glue = 0.40f + 0.60f * expf(-(dmid * dmid) * 0.0018f);

        // Ant spark (proximity to ant position in grid, per-zone)
        const float antDist = sqrtf(
            (gridPos - antXf) * (gridPos - antXf) +
            (sliceOffsetZ - antYf) * (sliceOffsetZ - antYf));
        const float antSpark = expf(-antDist * 0.12f);

        // Geometry: highway field modulated by glue
        float geometry = highway * glue;

        // Impact x ant spark (additive burst, per-zone)
        float impactAdd = m_impact[z] * antSpark * 0.45f;

        // Compose brightness: geometry x normBass x silScale x beatMod
        float brightness = (geometry * normBass + impactAdd) * beatMod * silScale;

        // Squared for punch
        brightness *= brightness;

        uint8_t val = static_cast<uint8_t>(clamp01(brightness) * 255.0f);
        val = scale8(val, ctx.brightness);

        // Tonal hue: chord-driven + highway modulation
        uint8_t hue = baseHue + static_cast<uint8_t>(highway * 40.0f);

        writeDualLocked(ctx, i, CHSV(hue, ctx.saturation, val));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

// =========================================================================
// Metadata / parameters
// =========================================================================

const plugins::EffectMetadata& LGPLangtonHighwayAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Langton Highway (5L-AR)",
        "Langton's ant cellular automaton with direct audio-reactive composition",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPLangtonHighwayAREffect::getParameterCount() const { return 0; }
const plugins::EffectParameter* LGPLangtonHighwayAREffect::getParameter(uint8_t) const { return nullptr; }
bool LGPLangtonHighwayAREffect::setParameter(const char*, float) { return false; }
float LGPLangtonHighwayAREffect::getParameter(const char*) const { return 0.0f; }

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
