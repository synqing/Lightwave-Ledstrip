/**
 * @file LGPRule30CathedralAREffect.cpp
 * @brief Rule 30 Cathedral (5-Layer AR) -- REWRITTEN
 *
 * Cellular automaton with Rule 30 (l ^ (c | r)) rendered as glowing cathedral
 * arches with direct audio-reactive composition.
 *
 * Divergence fixes: Direct ControlBus reads, single-stage smoothing (50ms),
 * asymmetric max follower (58ms/500ms, floor 0.04), no brightness floors,
 * beatStrength() continuous, squared brightness for punch.
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

static inline void writeDualLocked(plugins::EffectContext& ctx, int i, const CRGB& c) {
    ctx.leds[i] = c;
    int j = i + STRIP_LENGTH;
    if (j < (int)ctx.ledCount) ctx.leds[j] = c;
}

// =========================================================================
// Construction / destruction
// =========================================================================

LGPRule30CathedralAREffect::LGPRule30CathedralAREffect()
    : m_t(0.0f)
    , m_stepAccum(0.0f)
    , m_bass(0.0f), m_treble(0.0f), m_chromaAngle(0.0f)
    , m_bassMax(0.15f), m_trebleMax(0.15f)
    , m_impact(0.0f)
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
    m_t           = 0.0f;
    m_stepAccum   = 0.0f;
    m_bass = 0.0f; m_treble = 0.0f; m_chromaAngle = 0.0f;
    m_bassMax = 0.15f; m_trebleMax = 0.15f;
    m_impact = 0.0f;

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
// render() -- direct audio-reactive composition
// =========================================================================

void LGPRule30CathedralAREffect::render(plugins::EffectContext& ctx) {
#ifndef NATIVE_BUILD
    if (!m_ps) return;
#endif

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

    // Beat modulation
    const float beatMod = 0.3f + 0.7f * beatStr;

    // =================================================================
    // CA EVOLUTION
    // =================================================================

    // Impact reseeds the CA on strong beats
    if (m_impact > 0.8f && beatStr > 0.7f) {
#ifndef NATIVE_BUILD
        seedCA();
#endif
    }

    // Step rate: bass + speed drive CA stepping
    const float baseStepsPerFrame = (0.5f + 11.5f * speedNorm) * (0.7f + 0.6f * normBass);
    const float structureMod = (0.60f + 0.40f * normTreble);
    const float stepsPerFrame = baseStepsPerFrame * structureMod;

    m_stepAccum += stepsPerFrame * dtVis;

    while (m_stepAccum >= 1.0f) {
        stepCA();
        m_stepAccum -= 1.0f;
    }

    m_t += dtVis;

    // =================================================================
    // PER-PIXEL RENDER
    // =================================================================

    const float mid = (STRIP_LENGTH - 1) * 0.5f;

    // Hue from chroma
    uint8_t baseHue = static_cast<uint8_t>(m_chromaAngle * (255.0f / kTwoPi)) + ctx.gHue;

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

        const float dmid = static_cast<float>(i) - mid;

        // Centre glue (cathedral foundation)
        const float glue = 0.30f + 0.70f * expf(-(dmid * dmid) * 0.0012f);

        // Neighbourhood-based hue shift
        const float neighbourhoodSum = (i > 0 && i < STRIP_LENGTH - 1) ?
#ifndef NATIVE_BUILD
            static_cast<float>(m_ps->cells[i-1]) + static_cast<float>(m_ps->cells[i]) + static_cast<float>(m_ps->cells[i+1])
#else
            1.5f
#endif
            : caValue * 3.0f;
        const float hueShift = neighbourhoodSum * 15.0f;

        // Geometry: CA value x centre glue
        float structuredGeom = caValue * glue;

        // Impact: additive burst at CA cells
        float impactAdd = m_impact * caValue * 0.40f;

        // Compose: geometry * normBass * silScale * beatMod
        float brightness = (structuredGeom * normBass + impactAdd) * beatMod * silScale;

        // Squared for punch
        brightness *= brightness;

        uint8_t val = static_cast<uint8_t>(clamp01(brightness) * 255.0f);
        val = scale8(val, ctx.brightness);

        // Tonal hue: chroma anchor + neighbourhood density
        uint8_t hue = baseHue + static_cast<uint8_t>(hueShift);

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, val));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

// =========================================================================
// Metadata / parameters
// =========================================================================

const plugins::EffectMetadata& LGPRule30CathedralAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Rule 30 Cathedral (5L-AR)",
        "Cellular automaton cathedral with direct audio-reactive composition",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPRule30CathedralAREffect::getParameterCount() const { return 0; }
const plugins::EffectParameter* LGPRule30CathedralAREffect::getParameter(uint8_t) const { return nullptr; }
bool LGPRule30CathedralAREffect::setParameter(const char*, float) { return false; }
float LGPRule30CathedralAREffect::getParameter(const char*) const { return 0.0f; }

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
