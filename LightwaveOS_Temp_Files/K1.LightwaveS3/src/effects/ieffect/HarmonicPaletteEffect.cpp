/**
 * @file HarmonicPaletteEffect.cpp
 * @brief Harmonic Palette effect implementation
 *
 * MUSICAL INTELLIGENCE DEMONSTRATION:
 * This effect showcases the correct way to use audio features for color:
 * 1. Detect meaningful musical events (harmonic saliency spikes)
 * 2. Trigger smooth color transitions only on those events
 * 3. Maintain stable colors during normal playback
 *
 * ANTI-PATTERN AVOIDED:
 * - NO constant hue cycling (gHue++)
 * - NO rainbow effects
 * - NO frequency-to-color bindings without musical context
 */

#include "HarmonicPaletteEffect.h"
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

HarmonicPaletteEffect::HarmonicPaletteEffect()
    : m_currentHue(0)
    , m_targetHue(0)
    , m_hueTransition(1.0f)  // Start fully transitioned (stable)
    , m_lastSaliency(0.0f)
    , m_phase(0.0f)
    , m_cooldownTimer(0.0f)
{
}

bool HarmonicPaletteEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Initialize with a random starting hue for variety
    // Using totalTimeMs as seed for pseudo-randomness
    m_currentHue = (uint8_t)(ctx.totalTimeMs & 0xFF);
    m_targetHue = m_currentHue;
    m_hueTransition = 1.0f;
    m_lastSaliency = 0.0f;
    m_phase = 0.0f;
    m_cooldownTimer = 0.0f;

    return true;
}

void HarmonicPaletteEffect::render(plugins::EffectContext& ctx) {
    // Get safe delta time for frame-rate independent animation
    float dt = ctx.getSafeDeltaSeconds();

    // Update cooldown timer
    if (m_cooldownTimer > 0.0f) {
        m_cooldownTimer -= dt;
        if (m_cooldownTimer < 0.0f) m_cooldownTimer = 0.0f;
    }

    // ==========================================================================
    // HARMONIC SALIENCY DETECTION
    // ==========================================================================
    // This is the KEY innovation: we only shift colors when harmony changes,
    // not constantly. This creates meaningful audio-visual correlation.

    if (ctx.audio.available) {
        float saliency = ctx.audio.harmonicSaliency();

        // Rising edge detection: trigger when crossing threshold upward
        // AND cooldown has expired (prevents rapid-fire on sustained high saliency)
        bool risingEdge = (saliency > SALIENCY_THRESHOLD) &&
                          (m_lastSaliency <= SALIENCY_THRESHOLD);
        bool cooldownClear = (m_cooldownTimer <= 0.0f);

        if (risingEdge && cooldownClear) {
            // Harmonic event detected! Trigger palette shift.
            // Shift hue by fixed amount (creates color progression)
            m_targetHue = m_currentHue + (uint8_t)HUE_SHIFT_AMOUNT;
            m_hueTransition = 0.0f;  // Start transition

            // Reset cooldown to prevent immediate re-trigger
            m_cooldownTimer = COOLDOWN_DURATION_SEC;
        }

        // Store for next frame's edge detection
        m_lastSaliency = saliency;
    }

    // ==========================================================================
    // HUE TRANSITION (Smooth 300ms Crossfade)
    // ==========================================================================
    // Smooth interpolation prevents jarring color jumps

    if (m_hueTransition < 1.0f) {
        m_hueTransition += dt / TRANSITION_DURATION_SEC;
        if (m_hueTransition > 1.0f) {
            m_hueTransition = 1.0f;
        }

        // Use smooth easing curve (ease-out quad)
        float eased = 1.0f - (1.0f - m_hueTransition) * (1.0f - m_hueTransition);

        // Interpolate hue (handling wrap-around correctly)
        int16_t hueDiff = (int16_t)m_targetHue - (int16_t)m_currentHue;

        // Handle wrap-around: take shortest path
        if (hueDiff > 127) hueDiff -= 256;
        if (hueDiff < -128) hueDiff += 256;

        m_currentHue = m_currentHue + (int8_t)(hueDiff * eased);
    } else {
        // Transition complete: snap to target
        m_currentHue = m_targetHue;
    }

    // ==========================================================================
    // BREATHING WAVE ANIMATION (Time-Based, Center Origin)
    // ==========================================================================
    // Gentle visual interest without relying on audio (always looks good)

    // Speed-controlled phase accumulation
    float speedNorm = ctx.speed / 50.0f;
    m_phase += speedNorm * 0.05f * dt * 60.0f;  // Normalize to ~60 FPS base

    // Prevent phase overflow
    if (m_phase > 6.28318f) m_phase -= 6.28318f;

    // ==========================================================================
    // LED RENDERING (CENTER ORIGIN Compliant)
    // ==========================================================================

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        // Distance from center (0 at center, HALF_LENGTH-1 at edges)
        uint16_t dist = centerPairDistance(i);

        // Breathing wave: sinusoidal modulation based on distance from center
        // Wave propagates outward from center
        float waveInput = (float)dist * 0.1f - m_phase;
        float wave = sinf(waveInput);

        // Map wave to brightness range [55, 255]
        // This ensures LEDs are never completely off
        uint8_t brightness = (uint8_t)((wave + 1.0f) * 100.0f + 55.0f);

        // Apply intensity parameter
        brightness = (uint8_t)((brightness * ctx.brightness) >> 8);

        // Color calculation: base hue + distance-based variation
        // Small distance variation adds subtle depth without cycling
        uint8_t distHueOffset = (uint8_t)(dist >> 3);  // 0-9 range for 80 LEDs

        // STRIP 1: Current harmony-driven hue
        uint8_t strip1Hue = m_currentHue + distHueOffset;
        ctx.leds[i] = ctx.palette.getColor(strip1Hue, brightness);

        // STRIP 2: +90 hue offset (triadic harmony relationship)
        // This creates visual depth while maintaining harmonic color theory
        if (i + STRIP_LENGTH < ctx.ledCount) {
            uint8_t strip2Hue = m_currentHue + distHueOffset + STRIP2_HUE_OFFSET;
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(strip2Hue, brightness);
        }
    }
}

void HarmonicPaletteEffect::cleanup() {
    // No dynamically allocated resources to free
}

const plugins::EffectMetadata& HarmonicPaletteEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Harmonic Palette",
        "Colors shift only on harmonic changes - musical intelligence",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
