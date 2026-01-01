/**
 * @file BPMEffect.cpp
 * @brief BPM effect v2 - Dual-layer beat-synced traveling waves + expanding rings
 *
 * v2 COMPLETE REWRITE - Fixes fundamental design flaw
 *
 * ORIGINAL BUG: No wave propagation at all!
 *   - `intensity = beat - distFromCenter*3` was a STATIC gradient
 *   - All LEDs pulsed simultaneously (nothing traveled)
 *   - chromagram color sum produced muddy/washed out colors
 *
 * v2 FIX: Proper dual-layer architecture
 *   - LAYER 1: Background traveling sine wave from center
 *   - LAYER 2: Beat-triggered expanding rings overlay
 *   - Palette-based colors (no chromagram)
 *   - v8 Spring pattern for smooth speed modulation
 */

#include "BPMEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

BPMEffect::BPMEffect()
    : m_phase(0.0f)
    , m_nextRing(0)
    , m_tempoLocked(false)
{
}

bool BPMEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Initialize phase
    m_phase = 0.0f;

    // Initialize Spring with stiffness=50, critically damped (matches v8 pattern)
    m_speedSpring.init(50.0f, 1.0f);
    m_speedSpring.reset(1.0f);

    // Clear ring buffer
    for (int r = 0; r < MAX_RINGS; r++) {
        m_ringRadius[r] = 0.0f;
        m_ringIntensity[r] = 0.0f;
    }
    m_nextRing = 0;

    m_tempoLocked = false;

    return true;
}

void BPMEffect::render(plugins::EffectContext& ctx) {
    // =========================================================================
    // SAFE DELTA TIME (clamped for physics stability)
    // =========================================================================
    float dt = ctx.getSafeDeltaSeconds();
    float speedNorm = ctx.speed / 50.0f;

    // Default values for no-audio mode
    float speedMult = 1.0f;
    float expansionRate = 80.0f;

#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        // =====================================================================
        // v8 SPEED MODULATION: heavy_bands → Spring (NO stacked smoothing!)
        // =====================================================================
        float heavyEnergy = (ctx.audio.controlBus.heavy_bands[1] +
                             ctx.audio.controlBus.heavy_bands[2]) / 2.0f;
        float targetSpeed = 0.6f + 0.8f * heavyEnergy;  // 0.6-1.4x range
        speedMult = m_speedSpring.update(targetSpeed, dt);
        if (speedMult > 1.6f) speedMult = 1.6f;
        if (speedMult < 0.3f) speedMult = 0.3f;

        // =====================================================================
        // TEMPO LOCK HYSTERESIS
        // =====================================================================
        float tempoConf = ctx.audio.tempoConfidence();
        if (tempoConf > 0.6f) m_tempoLocked = true;
        else if (tempoConf < 0.4f) m_tempoLocked = false;

        // Expansion rate scales with tempo confidence
        expansionRate = 80.0f * (0.5f + tempoConf);

        // =====================================================================
        // BEAT RING SPAWNING
        // =====================================================================
        if (ctx.audio.isOnBeat()) {
            float strength = ctx.audio.beatStrength();
            // Scale by confidence for gentler response on uncertain beats
            float weightedStrength = strength * (0.5f + 0.5f * tempoConf);

            // Spawn new ring
            m_ringRadius[m_nextRing] = 0.0f;
            m_ringIntensity[m_nextRing] = weightedStrength;
            m_nextRing = (m_nextRing + 1) % MAX_RINGS;
        }
    }
#endif

    // =========================================================================
    // PHASE ACCUMULATION (standard formula from working effects)
    // =========================================================================
    m_phase += speedNorm * 240.0f * speedMult * dt;
    if (m_phase > 628.3f) m_phase -= 628.3f;  // Wrap at ~100*2π

    // =========================================================================
    // UPDATE RING RADII (expand outward, fade as they go)
    // =========================================================================
    for (int r = 0; r < MAX_RINGS; r++) {
        if (m_ringIntensity[r] > 0.01f) {
            m_ringRadius[r] += expansionRate * dt;
            m_ringIntensity[r] *= 0.97f;  // Gradual fade

            // Kill ring when it reaches edge
            if (m_ringRadius[r] > HALF_LENGTH) {
                m_ringIntensity[r] = 0.0f;
            }
        }
    }

    // =========================================================================
    // DUAL-LAYER RENDER LOOP
    // =========================================================================
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float dist = (float)centerPairDistance(i);

        // =====================================================================
        // LAYER 1: Background traveling sine wave from center
        // sin(dist * freq - phase) → OUTWARD motion when phase increases
        // =====================================================================
        float waveFreq = 0.12f;  // ~52 LED wavelength
        float wave = sinf(dist * waveFreq - m_phase);
        // Map [-1,1] to [0.2, 0.8] for subtle background
        uint8_t baseIntensity = (uint8_t)((wave * 0.3f + 0.5f) * ctx.brightness);

        // =====================================================================
        // LAYER 2: Beat rings overlay
        // Each ring is a Gaussian-ish bump that expands from center
        // =====================================================================
        uint8_t ringBoost = 0;
        for (int r = 0; r < MAX_RINGS; r++) {
            if (m_ringIntensity[r] > 0.01f) {
                float delta = fabsf(dist - m_ringRadius[r]);
                if (delta < 6.0f) {
                    // Soft falloff within 6 LEDs of ring center
                    float ringBright = (1.0f - delta / 6.0f) * m_ringIntensity[r];
                    ringBoost = qadd8(ringBoost, (uint8_t)(ringBright * 180.0f));
                }
            }
        }

        // Combine layers
        uint8_t intensity = qadd8(baseIntensity, ringBoost);

        // =====================================================================
        // PALETTE-BASED COLOR (NO chromagram - causes muddy colors!)
        // =====================================================================
        uint8_t hue = ctx.gHue + (uint8_t)(dist / 3);
        CRGB color = ctx.palette.getColor(hue, intensity);

        ctx.leds[i] = color;

        // Strip 2: Complementary color (+128 hue offset)
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(
                (uint8_t)(hue + 128), intensity);
        }
    }
}

void BPMEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& BPMEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "BPM",
        "v2: Traveling waves + beat-triggered expanding rings from center",
        plugins::EffectCategory::PARTY,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
