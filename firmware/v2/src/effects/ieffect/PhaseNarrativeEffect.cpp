/**
 * @file PhaseNarrativeEffect.cpp
 * @brief Phase Narrative effect implementation
 *
 * Phase accumulation traveling waves + narrative arc envelope.
 * CENTER ORIGIN compliant: all waves originate from LED 79/80.
 */

#include "PhaseNarrativeEffect.h"
#include "../CoreEffects.h"      // STRIP_LENGTH, HALF_LENGTH, centerPairDistance()
#include "../../config/features.h"
#include <FastLED.h>             // fadeToBlackBy, CRGB
#include <cmath>                 // sinf, fabsf, fminf, fmaxf

namespace lightwaveos {
namespace effects {
namespace ieffect {

// ============================================================================
// Constants
// ============================================================================

// Two-pi constant for wave calculations
static constexpr float TWO_PI_F = 6.283185307f;

// ============================================================================
// Helper Functions
// ============================================================================

static inline float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static inline uint8_t clampU8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

// Asymmetric one-pole smoother (fast rise, slower fall)
static float smoothValue(float current, float target, float rise, float fall) {
    const float alpha = (target > current) ? rise : fall;
    return current + (target - current) * alpha;
}

// ============================================================================
// PhaseNarrativeEffect Implementation
// ============================================================================

PhaseNarrativeEffect::PhaseNarrativeEffect()
    : m_narrativePhase(NarrativePhase::REST)
    , m_phaseTime(0.0f)
    , m_intensity(0.0f)
    , m_buildDur(0.8f)
    , m_holdDur(0.3f)
    , m_releaseDur(0.6f)
    , m_restDur(0.2f)
    , m_phase(0.0f)
    , m_wavelength(25.0f)
    , m_outward(true)
    , m_lastBeat(false)
    , m_speedSmooth(1.0f)
{
}

bool PhaseNarrativeEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Reset narrative state machine
    m_narrativePhase = NarrativePhase::REST;
    m_phaseTime = 0.0f;
    m_intensity = 0.0f;

    // Reset phase accumulator
    m_phase = 0.0f;

    // Reset audio state
    m_lastBeat = false;
    m_speedSmooth = 1.0f;

    return true;
}

void PhaseNarrativeEffect::render(plugins::EffectContext& ctx) {
    // =========================================================================
    // Time and Speed Normalization
    // =========================================================================
    const float dt = ctx.getSafeDeltaSeconds();
    const float speedNorm = ctx.speed / 50.0f;  // Typical range 0-2

    // Slew-limited speed to prevent jog-dial jitter
    const float speedAlpha = dt / (0.15f + dt);  // ~150ms smoothing
    m_speedSmooth = smoothValue(m_speedSmooth, speedNorm, speedAlpha, speedAlpha);

    // =========================================================================
    // Audio Processing: Beat-Triggered Narrative
    // =========================================================================
#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        const bool beat = ctx.audio.isOnBeat();

        // Edge detection: trigger BUILD phase on beat rising edge
        if (beat && !m_lastBeat) {
            // Only trigger if in REST phase and minimum rest time has passed
            if (m_narrativePhase == NarrativePhase::REST && m_phaseTime >= m_restDur) {
                m_narrativePhase = NarrativePhase::BUILD;
                m_phaseTime = 0.0f;
            }
        }
        m_lastBeat = beat;

        // Optional: Modulate wavelength with RMS energy
        // Higher energy = shorter wavelength (more waves visible)
        const float rms = ctx.audio.rms();
        m_wavelength = 20.0f + (1.0f - rms) * 20.0f;  // Range: 20-40 LEDs
    }
#else
    // No audio: auto-cycle narrative for visual interest
    if (m_narrativePhase == NarrativePhase::REST && m_phaseTime >= m_restDur * 3.0f) {
        m_narrativePhase = NarrativePhase::BUILD;
        m_phaseTime = 0.0f;
    }
#endif

    // =========================================================================
    // Update Narrative State Machine
    // =========================================================================
    updateNarrative(dt);

    // =========================================================================
    // Phase Accumulation
    // =========================================================================
    // Phase accumulates faster during HOLD phase for more dynamic feel
    const float phaseSpeedMod = (m_narrativePhase == NarrativePhase::HOLD) ? 1.5f : 1.0f;
    const float phaseRate = m_speedSmooth * 0.15f * phaseSpeedMod;  // Base rate tuned for visual appeal

    // Accumulate phase (direction depends on m_outward flag)
    m_phase += phaseRate;

    // Wrap phase to prevent float overflow (keep in 0 to 2*PI range)
    if (m_phase > TWO_PI_F) {
        m_phase -= TWO_PI_F;
    }

    // =========================================================================
    // Clear Buffer with Fade (creates trailing effect)
    // =========================================================================
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    // =========================================================================
    // Wave Rendering: Strip 1 (LEDs 0-159)
    // =========================================================================
    // Wave number k = 2*PI / wavelength
    const float k = TWO_PI_F / m_wavelength;

    for (uint16_t i = 0; i < STRIP_LENGTH && i < ctx.ledCount; ++i) {
        // Distance from center (CENTER ORIGIN pattern)
        const uint16_t dist = centerPairDistance(i);

        // Wave equation: y = sin(k * dist - phase) for outward propagation
        // or y = sin(k * dist + phase) for inward propagation
        const float waveArg = m_outward ? (k * (float)dist - m_phase)
                                        : (k * (float)dist + m_phase);
        const float wave = sinf(waveArg);

        // Map wave [-1, 1] to brightness [0, 255]
        // Apply narrative intensity as amplitude envelope
        const float normalizedWave = (wave + 1.0f) * 0.5f;  // [0, 1]
        const uint8_t brightness = clampU8((int)(normalizedWave * 255.0f * m_intensity));

        // Color from palette with distance-based hue variation
        // Hue shifts slightly with distance for visual depth
        const uint8_t hue = ctx.gHue + (dist >> 2);  // Subtle hue gradient

        ctx.leds[i] = ctx.palette.getColor(hue, brightness);
    }

    // =========================================================================
    // Wave Rendering: Strip 2 (LEDs 160-319) with +90 Hue Offset
    // =========================================================================
    for (uint16_t i = 0; i < STRIP_LENGTH; ++i) {
        const uint16_t strip2Idx = i + STRIP_LENGTH;
        if (strip2Idx >= ctx.ledCount) break;

        // Distance from center (same calculation as Strip 1)
        const uint16_t dist = centerPairDistance(i);

        // Same wave equation
        const float waveArg = m_outward ? (k * (float)dist - m_phase)
                                        : (k * (float)dist + m_phase);
        const float wave = sinf(waveArg);

        const float normalizedWave = (wave + 1.0f) * 0.5f;
        const uint8_t brightness = clampU8((int)(normalizedWave * 255.0f * m_intensity));

        // Strip 2 has +90 hue offset for complementary color
        const uint8_t hue = ctx.gHue + (dist >> 2) + 90;

        ctx.leds[strip2Idx] = ctx.palette.getColor(hue, brightness);
    }
}

void PhaseNarrativeEffect::updateNarrative(float dt) {
    // Advance time in current phase
    m_phaseTime += dt;

    switch (m_narrativePhase) {
        case NarrativePhase::REST:
            // Intensity stays at 0, waiting for trigger
            m_intensity = 0.0f;
            // Phase time accumulates for minimum rest duration check
            break;

        case NarrativePhase::BUILD:
            // Ramp intensity from 0 to 1 using easeInQuad
            {
                const float progress = clamp01(m_phaseTime / m_buildDur);
                m_intensity = easeInQuad(progress);

                // Transition to HOLD when BUILD complete
                if (m_phaseTime >= m_buildDur) {
                    m_narrativePhase = NarrativePhase::HOLD;
                    m_phaseTime = 0.0f;
                }
            }
            break;

        case NarrativePhase::HOLD:
            // Hold at full intensity
            m_intensity = 1.0f;

            // Transition to RELEASE when HOLD complete
            if (m_phaseTime >= m_holdDur) {
                m_narrativePhase = NarrativePhase::RELEASE;
                m_phaseTime = 0.0f;
            }
            break;

        case NarrativePhase::RELEASE:
            // Decay intensity from 1 to 0 using easeOutQuad (inverted)
            {
                const float progress = clamp01(m_phaseTime / m_releaseDur);
                m_intensity = 1.0f - easeOutQuad(progress);

                // Transition to REST when RELEASE complete
                if (m_phaseTime >= m_releaseDur) {
                    m_narrativePhase = NarrativePhase::REST;
                    m_phaseTime = 0.0f;
                }
            }
            break;
    }
}

float PhaseNarrativeEffect::easeInQuad(float t) {
    // Quadratic ease-in: slow start, accelerating
    return t * t;
}

float PhaseNarrativeEffect::easeOutQuad(float t) {
    // Quadratic ease-out: fast start, decelerating
    return t * (2.0f - t);
}

void PhaseNarrativeEffect::cleanup() {
    // No dynamic allocations to clean up
}

const plugins::EffectMetadata& PhaseNarrativeEffect::getMetadata() const {
    static const plugins::EffectMetadata meta{
        "Phase Narrative",
        "Traveling waves with BUILD/HOLD/RELEASE envelope",
        plugins::EffectCategory::GEOMETRIC,
        1,  // version
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
