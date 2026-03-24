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
    , m_antX(32), m_antY(32), m_antDir(0)
    , m_antStepAccum(0.0f), m_sliceOffset(0.0f)
    , m_bass(0.0f), m_treble(0.0f), m_chromaAngle(0.0f)
    , m_bassMax(0.15f), m_trebleMax(0.15f)
    , m_impact(0.0f)
{
}

LGPLangtonHighwayAREffect::~LGPLangtonHighwayAREffect() {
    cleanup();
}

// =========================================================================
// Init / cleanup
// =========================================================================

bool LGPLangtonHighwayAREffect::init(plugins::EffectContext& ctx) {
    m_bass = 0.0f; m_treble = 0.0f; m_chromaAngle = 0.0f;
    m_bassMax = 0.15f; m_trebleMax = 0.15f;
    m_impact = 0.0f;

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
// Langton's ant step (PRESERVED EXACTLY)
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

    // White -> turn right, flip to black, move
    // Black -> turn left, flip to white, move
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
// Drifting diagonal slice projection (PRESERVED EXACTLY)
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
// render() — direct audio-reactive composition
// =========================================================================

void LGPLangtonHighwayAREffect::render(plugins::EffectContext& ctx) {
    if (!m_grid) return;

    const float dt = ctx.getSafeRawDeltaSeconds();
    const float dtVis = ctx.getSafeDeltaSeconds();
    const float speedNorm = ctx.speed / 50.0f;

    // STEP 1: Direct ControlBus reads
    const float rawBass = ctx.audio.available ? ctx.audio.bass() : 0.0f;
    const float rawTreble = ctx.audio.available ? ctx.audio.treble() : 0.0f;
    const float beatStr = ctx.audio.available ? ctx.audio.beatStrength() : 0.0f;
    const float silScale = ctx.audio.available ? ctx.audio.silentScale() : 0.0f;
    const float* chroma = ctx.audio.available ? ctx.audio.chroma() : nullptr;

    // STEP 2: Single-stage smoothing
    m_bass += (rawBass - m_bass) * (1.0f - expf(-dt / kBassTau));
    m_treble += (rawTreble - m_treble) * (1.0f - expf(-dt / kTrebleTau));

    // Circular chroma EMA
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
            float delta = target - m_chromaAngle;
            while (delta > kPi) delta -= kTwoPi;
            while (delta < -kPi) delta += kTwoPi;
            m_chromaAngle += delta * (1.0f - expf(-dt / kChromaTau));
            if (m_chromaAngle < 0.0f) m_chromaAngle += kTwoPi;
            if (m_chromaAngle >= kTwoPi) m_chromaAngle -= kTwoPi;
        }
    }

    // STEP 3: Max followers
    {
        float aA = 1.0f - expf(-dt / kFollowerAttackTau);
        float dA = 1.0f - expf(-dt / kFollowerDecayTau);
        if (m_bass > m_bassMax) m_bassMax += (m_bass - m_bassMax) * aA;
        else m_bassMax += (m_bass - m_bassMax) * dA;
        if (m_bassMax < kFollowerFloor) m_bassMax = kFollowerFloor;

        if (m_treble > m_trebleMax) m_trebleMax += (m_treble - m_trebleMax) * aA;
        else m_trebleMax += (m_treble - m_trebleMax) * dA;
        if (m_trebleMax < kFollowerFloor) m_trebleMax = kFollowerFloor;
    }
    const float normBass = clamp01(m_bass / m_bassMax);
    const float normTreble = clamp01(m_treble / m_trebleMax);

    // STEP 4: Impact (continuous beatStrength rise, exponential decay)
    if (beatStr > m_impact) m_impact = beatStr;
    m_impact *= expf(-dt / kImpactDecayTau);

    const float beatMod = 0.3f + 0.7f * beatStr;

    // =================================================================
    // ANT STEPPING (PRESERVED EXACTLY)
    // =================================================================

    // Step count controlled by normalised audio (rhythmic drive from bass + treble)
    const float rhythmicDrive = clamp01(0.30f + 0.45f * normBass + 0.25f * normTreble);
    const float stepsPerSec = (0.5f + 7.5f * rhythmicDrive) * speedNorm;
    m_antStepAccum += stepsPerSec * dtVis;

    while (m_antStepAccum >= 1.0f) {
        stepAnt();
        m_antStepAccum -= 1.0f;
    }

    // Slice drift — motionRate replaced with bass-driven rate
    const float motionRate = 0.6f + 0.8f * normBass + 0.3f * m_impact;
    const float driftRate = (0.15f + 0.35f * speedNorm) * motionRate;
    m_sliceOffset += driftRate * dtVis;
    m_sliceOffset = fract(m_sliceOffset / static_cast<float>(H)) * static_cast<float>(H);

    // =================================================================
    // PER-PIXEL RENDER
    // =================================================================

    const float mid    = (STRIP_LENGTH - 1) * 0.5f;
    const float invMid = 1.0f / mid;

    // Hue from chroma
    uint8_t baseHue = static_cast<uint8_t>(m_chromaAngle * (255.0f / kTwoPi)) + ctx.gHue;

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
        highway = clamp01(highway);

        // Centre glue (stronger adhesion near origin)
        const float glue = 0.40f + 0.60f * expf(-(dmid * dmid) * 0.0018f);

        // Ant spark (proximity to ant position in grid)
        const float antDist = sqrtf(
            (gridPos - antXf) * (gridPos - antXf) +
            (m_sliceOffset - antYf) * (m_sliceOffset - antYf));
        const float antSpark = expf(-antDist * 0.12f);

        // Geometry: highway field modulated by glue
        float geometry = highway * glue;

        // Impact x ant spark (additive burst)
        float impactAdd = m_impact * antSpark * 0.45f;

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
