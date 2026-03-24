/**
 * @file LGPAiryCometAREffect.cpp
 * @brief LGP Airy Comet (5-Layer AR) — REWRITTEN
 *
 * Self-accelerating parabolic comet with Gaussian head and oscillatory
 * Airy-ish tail lobes. Audio drives brightness and motion directly.
 *
 * Divergence fixes: Direct ControlBus reads, single-stage smoothing (50ms),
 * asymmetric max follower (58ms/500ms, floor 0.04), no brightness floors,
 * beatStrength() continuous, squared brightness for punch.
 *
 * Centre-origin compliant. Dual-strip locked.
 * NOTE: Comet is inherently asymmetric — keeps linear iteration + writeDualLocked.
 */

#include "LGPAiryCometAREffect.h"
#include "LGPFilmPost.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

// Constants
static constexpr float kTwoPi = 6.28318530717958647692f;
static constexpr float kPi    = 3.14159265358979323846f;
static constexpr float kBassTau    = 0.050f;
static constexpr float kTrebleTau  = 0.040f;
static constexpr float kChromaTau  = 0.300f;
static constexpr float kFollowerAttackTau = 0.058f;
static constexpr float kFollowerDecayTau  = 0.500f;
static constexpr float kFollowerFloor     = 0.04f;
static constexpr float kImpactDecayTau = 0.180f;

// Helpers
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

static inline float gaussian(float x, float sigma) {
    return expf(-(x * x) / (2.0f * sigma * sigma));
}

static inline void writeDualLocked(plugins::EffectContext& ctx, int i, const CRGB& c) {
    ctx.leds[i] = c;
    int j = i + STRIP_LENGTH;
    if (j < static_cast<int>(ctx.ledCount)) ctx.leds[j] = c;
}

// Constructor
LGPAiryCometAREffect::LGPAiryCometAREffect()
    : m_t(0.0f), m_bass(0.0f), m_treble(0.0f), m_chromaAngle(0.0f),
      m_bassMax(0.15f), m_trebleMax(0.15f), m_impact(0.0f) {}

bool LGPAiryCometAREffect::init(plugins::EffectContext& ctx) {
    m_t = 0.0f; m_bass = 0.0f; m_treble = 0.0f; m_chromaAngle = 0.0f;
    m_bassMax = 0.15f; m_trebleMax = 0.15f; m_impact = 0.0f;
    lightwaveos::effects::cinema::reset();
    return true;
}

void LGPAiryCometAREffect::render(plugins::EffectContext& ctx) {
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

    // STEP 5: Comet visual parameters
    const float beatMod = 0.3f + 0.7f * beatStr;
    const float mid = (STRIP_LENGTH - 1) * 0.5f;

    // Comet position: parabolic self-acceleration, bass drives speed
    float s = fract(m_t * (0.08f + 0.08f * normBass + 0.04f * m_impact));
    float parab = s * s;
    float x0 = -mid * 0.90f + 2.0f * mid * 0.90f * parab;

    // Direction flip (rhythmic via treble variation)
    bool flip = (fract(m_t * 0.06f) > 0.5f);
    float dir = flip ? -1.0f : 1.0f;

    // Tail parameters driven by normalised treble
    float tailDecay = 0.12f - 0.04f * normTreble;   // More treble = longer tail
    float lobeFreq = 1.25f + 0.50f * normTreble;    // More treble = tighter lobes

    // Head width driven by bass
    float sigma = 3.0f + 1.5f * normBass;

    // Motion accumulation
    float tRate = 1.0f + 4.5f * speedNorm;
    m_t += tRate * dtVis;

    // Hue from chroma
    uint8_t baseHue = static_cast<uint8_t>(m_chromaAngle * (255.0f / kTwoPi)) + ctx.gHue;

    // Trail persistence: more energy = shorter trails
    uint8_t fadeAmt = static_cast<uint8_t>(clampf(18.0f + 35.0f * (1.0f - normBass), 12.0f, 55.0f));
    fadeToBlackBy(ctx.leds, ctx.ledCount, fadeAmt);

    // STEP 6: Per-pixel render (linear — comet is asymmetric)
    for (int i = 0; i < STRIP_LENGTH; i++) {
        float x = static_cast<float>(i) - mid;
        float dx = x - x0;

        // Gaussian head
        float head = gaussian(dx, sigma);

        // Airy tail: oscillatory lobes behind the head, decaying
        float behind = (dx * dir > 0.0f) ? (dx * dir) : 0.0f;
        float tail = expf(-behind * tailDecay) *
                     (0.55f + 0.45f * cosf(behind * lobeFreq - m_t * 0.9f));
        tail = clamp01(tail);

        // Impact thrust at head
        float impactAdd = gaussian(dx, sigma * 1.5f) * m_impact * 0.4f;

        // Compose brightness: audio magnitude x geometry x beat x silence
        float wave = clamp01(head + 0.70f * tail * normBass);
        float brightness = (wave * normBass + impactAdd) * beatMod * silScale;

        // Squared for punch
        brightness *= brightness;

        uint8_t val = static_cast<uint8_t>(clamp01(brightness) * 255.0f);
        val = scale8(val, ctx.brightness);

        // Hue: warm head (base), cool tail (+60)
        float headness = gaussian(dx, sigma * 2.0f);
        uint8_t hue = baseHue + static_cast<uint8_t>(60.0f * (1.0f - headness));

        writeDualLocked(ctx, i, CHSV(hue, ctx.saturation, val));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

// Cleanup / metadata / parameters
void LGPAiryCometAREffect::cleanup() {}

const plugins::EffectMetadata& LGPAiryCometAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Airy Comet (5L-AR)",
        "Self-accelerating comet with direct audio-reactive composition",
        plugins::EffectCategory::QUANTUM, 1
    };
    return meta;
}
uint8_t LGPAiryCometAREffect::getParameterCount() const { return 0; }
const plugins::EffectParameter* LGPAiryCometAREffect::getParameter(uint8_t) const { return nullptr; }
bool LGPAiryCometAREffect::setParameter(const char*, float) { return false; }
float LGPAiryCometAREffect::getParameter(const char*) const { return 0.0f; }

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
