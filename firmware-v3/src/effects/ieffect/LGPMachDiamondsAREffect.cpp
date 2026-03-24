/**
 * @file LGPMachDiamondsAREffect.cpp
 * @brief Mach Diamonds (5-Layer Audio-Reactive) — REWRITTEN
 *
 * Shock-diamond standing-wave pulses. Audio drives brightness and colour
 * directly via ControlBus — no helper extraction layer.
 *
 * Divergence fixes applied:
 *   1. bands[] (not heavy_bands) — no pre-smoothing on source
 *   2. Single-stage EMA (50ms bass, 40ms treble) — one smoothing layer
 *   3. Asymmetric max follower (58ms/500ms, floor 0.04) — dynamic gain
 *   4. No brightness floors — silence = dark
 *   5. Audio → brightness directly (not via simulation coefficients)
 *   6. beatStrength() (continuous) instead of beatTick (boolean)
 *   7. SET_CENTER_PAIR (4-way symmetry) instead of writeDualLocked (2-way)
 *   8. Centre-outward iteration (dist 0→79) instead of linear (i 0→159)
 *
 * Centre-origin compliant. Dual-strip mirrored.
 */

#include "LGPMachDiamondsAREffect.h"
#include "LGPFilmPost.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

// =========================================================================
// Constants
// =========================================================================

static constexpr float kTwoPi   = 6.28318530717958647692f;
static constexpr float kPi      = 3.14159265358979323846f;

// Smoothing time constants (seconds)
static constexpr float kBassTau    = 0.050f;   // 50ms attack for bass
static constexpr float kTrebleTau  = 0.040f;   // 40ms attack for treble
static constexpr float kChromaTau  = 0.300f;   // 300ms for hue (slow, musical)

// Max follower time constants
static constexpr float kFollowerAttackTau = 0.058f;  // 58ms attack
static constexpr float kFollowerDecayTau  = 0.500f;  // 500ms decay
static constexpr float kFollowerFloor     = 0.04f;   // Minimum follower value

// Beat impact decay
static constexpr float kImpactDecayTau = 0.180f;     // 180ms decay

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

static inline float tri01(float x) {
    float f = fract(x);
    return clamp01(1.0f - fabsf(2.0f * f - 1.0f));
}

// =========================================================================
// Construction / init / cleanup
// =========================================================================

LGPMachDiamondsAREffect::LGPMachDiamondsAREffect()
    : m_t(0.0f)
    , m_bass(0.0f)
    , m_treble(0.0f)
    , m_chromaAngle(0.0f)
    , m_bassMax(0.15f)
    , m_trebleMax(0.15f)
    , m_impact(0.0f)
{
}

bool LGPMachDiamondsAREffect::init(plugins::EffectContext& ctx) {
    m_t           = 0.0f;
    m_bass        = 0.0f;
    m_treble      = 0.0f;
    m_chromaAngle = 0.0f;
    m_bassMax     = 0.15f;
    m_trebleMax   = 0.15f;
    m_impact      = 0.0f;
    lightwaveos::effects::cinema::reset();
    return true;
}

// =========================================================================
// render() — Direct ControlBus reads, single-stage smoothing
// =========================================================================

void LGPMachDiamondsAREffect::render(plugins::EffectContext& ctx) {

    // --- Timing (raw, not SPEED-scaled, for audio-coupled maths) ---
    const float dt = ctx.getSafeRawDeltaSeconds();
    const float dtVis = ctx.getSafeDeltaSeconds();
    const float speedNorm = ctx.speed / 50.0f;

    // =================================================================
    // STEP 1: Read ControlBus DIRECTLY — no helper layer
    // =================================================================

    // Bass: bands[0] + bands[1] (reactive, pipeline-smoothed only)
    const float rawBass = ctx.audio.available
        ? ctx.audio.bass()   // (bands[0] + bands[1]) * 0.5f
        : 0.0f;

    // Treble: bands[5] + bands[6] + bands[7] (reactive)
    const float rawTreble = ctx.audio.available
        ? ctx.audio.treble() // (bands[5..7]) / 3.0f
        : 0.0f;

    // Beat strength: continuous 0-1 float (NOT boolean tick)
    const float beatStr = ctx.audio.available
        ? ctx.audio.beatStrength()
        : 0.0f;

    // Snare hit: single-frame boolean pulse
    const bool snareHit = ctx.audio.available && ctx.audio.isSnareHit();

    // Silence scale: 1.0=active, 0.0=silent (from ControlBus hysteresis)
    const float silScale = ctx.audio.available
        ? ctx.audio.silentScale()
        : 0.0f;

    // Chroma: 12-bin pitch class energy (reactive, not heavy)
    const float* chroma = ctx.audio.available
        ? ctx.audio.chroma()
        : nullptr;

    // =================================================================
    // STEP 2: Single-stage smoothing (one EMA per signal)
    // =================================================================

    {
        const float bassAlpha = 1.0f - expf(-dt / kBassTau);
        m_bass += (rawBass - m_bass) * bassAlpha;
    }
    {
        const float trebleAlpha = 1.0f - expf(-dt / kTrebleTau);
        m_treble += (rawTreble - m_treble) * trebleAlpha;
    }

    // Circular chroma EMA → hue angle
    if (chroma) {
        float sx = 0.0f, sy = 0.0f;
        for (int i = 0; i < 12; i++) {
            const float angle = static_cast<float>(i) * (kTwoPi / 12.0f);
            sx += chroma[i] * cosf(angle);
            sy += chroma[i] * sinf(angle);
        }
        const float totalEnergy = sx * sx + sy * sy;
        if (totalEnergy > 0.0001f) {
            float targetAngle = atan2f(sy, sx);
            if (targetAngle < 0.0f) targetAngle += kTwoPi;

            // Circular shortest-path delta
            float delta = targetAngle - m_chromaAngle;
            while (delta > kPi) delta -= kTwoPi;
            while (delta < -kPi) delta += kTwoPi;

            const float chromaAlpha = 1.0f - expf(-dt / kChromaTau);
            m_chromaAngle += delta * chromaAlpha;

            // Wrap to [0, 2π)
            if (m_chromaAngle < 0.0f) m_chromaAngle += kTwoPi;
            if (m_chromaAngle >= kTwoPi) m_chromaAngle -= kTwoPi;
        }
    }

    // =================================================================
    // STEP 3: Asymmetric max followers for dynamic gain normalisation
    // =================================================================

    // Bass max follower
    {
        const float attackAlpha = 1.0f - expf(-dt / kFollowerAttackTau);
        const float decayAlpha  = 1.0f - expf(-dt / kFollowerDecayTau);

        if (m_bass > m_bassMax) {
            m_bassMax += (m_bass - m_bassMax) * attackAlpha;   // Fast rise
        } else {
            m_bassMax += (m_bass - m_bassMax) * decayAlpha;    // Slow fall
        }
        if (m_bassMax < kFollowerFloor) m_bassMax = kFollowerFloor;
    }

    // Treble max follower
    {
        const float attackAlpha = 1.0f - expf(-dt / kFollowerAttackTau);
        const float decayAlpha  = 1.0f - expf(-dt / kFollowerDecayTau);

        if (m_treble > m_trebleMax) {
            m_trebleMax += (m_treble - m_trebleMax) * attackAlpha;
        } else {
            m_trebleMax += (m_treble - m_trebleMax) * decayAlpha;
        }
        if (m_trebleMax < kFollowerFloor) m_trebleMax = kFollowerFloor;
    }

    // Normalised audio (dynamic gain: quiet content amplified, loud content capped)
    const float normBass   = clamp01(m_bass / m_bassMax);
    const float normTreble = clamp01(m_treble / m_trebleMax);

    // =================================================================
    // STEP 4: Beat/percussion impact (exponential decay)
    // =================================================================

    // Beat strength drives impact directly (continuous, not boolean)
    if (beatStr > m_impact) {
        m_impact = beatStr;  // Immediate rise to beat strength
    }
    // Snare accent stacks on top
    if (snareHit) {
        m_impact = fmaxf(m_impact, 0.80f);
    }
    // Exponential decay
    m_impact *= expf(-dt / kImpactDecayTau);

    // =================================================================
    // STEP 5: Visual parameters driven by normalised audio
    // =================================================================

    // Beat brightness modulation: 0.3 floor between beats, 1.0 on beat
    const float beatMod = 0.3f + 0.7f * beatStr;

    // Diamond cell spacing: bass tightens the pattern
    const float spacing = clampf(
        0.16f - 0.06f * normBass, 0.08f, 0.20f);

    // Diamond sharpness: treble sharpens peaks
    const float sharpExp = clampf(
        2.0f + 1.5f * normTreble + 0.5f * m_impact, 1.8f, 4.0f);

    // Motion drift (SPEED-scaled time, not audio time)
    const float tRate = (1.0f + 4.0f * speedNorm);
    m_t += tRate * dtVis;
    const float drift = m_t * (0.20f + 0.35f * speedNorm);

    // Hue from chroma angle
    const uint8_t baseHue = static_cast<uint8_t>(
        m_chromaAngle * (255.0f / kTwoPi)) + ctx.gHue;

    // Trail persistence: more energy = shorter trails (faster fade)
    const uint8_t fadeAmount = static_cast<uint8_t>(
        clampf(20.0f + 40.0f * (1.0f - normBass), 15.0f, 60.0f));
    fadeToBlackBy(ctx.leds, ctx.ledCount, fadeAmount);

    // =================================================================
    // STEP 6: Per-pixel render — centre-outward, 4-way symmetric
    // =================================================================

    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        const float progress = (HALF_LENGTH <= 1) ? 0.0f
            : (static_cast<float>(dist) / static_cast<float>(HALF_LENGTH - 1));

        // Shock diamond triangular wave
        const float cellPhase = (progress / spacing) - drift;
        const float tri = tri01(cellPhase);
        const float geom = powf(tri, sharpExp);

        // Diamond breathing (subtle cosine wobble)
        const float breathing = 0.90f + 0.10f * cosf(kTwoPi * cellPhase + m_t * 0.6f);

        // Impact accent at diamond peaks
        const float impactAdd = m_impact * powf(tri, 1.5f) * 0.4f;

        // Compose brightness: audio magnitude × geometry × beat × silence
        float brightness = (normBass * geom * breathing + impactAdd) * beatMod * silScale;

        // Squared response for perceptual punch (like BloomRef v*=v)
        brightness *= brightness;

        // Apply global brightness
        uint8_t val = static_cast<uint8_t>(clamp01(brightness) * 255.0f);
        val = scale8(val, ctx.brightness);

        // Hue: chroma-anchored base + spatial gradient
        const uint8_t hue = baseHue + static_cast<uint8_t>(progress * 48.0f);

        // 4-way symmetric write (both strips, both sides)
        SET_CENTER_PAIR(ctx, dist, CHSV(hue, ctx.saturation, val));
    }

    // Cinema post-processing (spatial softening, tone mapping, gamma)
    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

// =========================================================================
// cleanup / metadata / parameters
// =========================================================================

void LGPMachDiamondsAREffect::cleanup() {}

const plugins::EffectMetadata& LGPMachDiamondsAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Mach Diamonds (5L-AR)",
        "Shock-diamond jewellery with direct audio-reactive composition",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPMachDiamondsAREffect::getParameterCount() const { return 0; }
const plugins::EffectParameter* LGPMachDiamondsAREffect::getParameter(uint8_t) const { return nullptr; }
bool LGPMachDiamondsAREffect::setParameter(const char*, float) { return false; }
float LGPMachDiamondsAREffect::getParameter(const char*) const { return 0.0f; }

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
