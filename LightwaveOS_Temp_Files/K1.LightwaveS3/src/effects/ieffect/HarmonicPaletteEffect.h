/**
 * @file HarmonicPaletteEffect.h
 * @brief Harmonic Palette - Color shifts driven by musical harmony changes
 *
 * Effect ID: TBD (assign on registration)
 * Family: PARTY
 * Tags: CENTER_ORIGIN | AUDIO_REACTIVE
 *
 * INTELLIGENT COLOR PHILOSOPHY:
 * Amateur visualizers constantly cycle colors (rainbows, gHue++). This effect
 * demonstrates MUSICAL INTELLIGENCE: colors shift ONLY when harmonic content
 * changes (chord progressions, key changes, tonal shifts).
 *
 * The harmonicSaliency signal (0.0-1.0) spikes when harmony changes occur.
 * By detecting rising edges above a threshold, we trigger meaningful color
 * transitions tied to the music's harmonic structure.
 *
 * AUDIO FEATURE UTILIZATION:
 * - ctx.audio.harmonicSaliency() - PRIMARY driver (was 0% utilized!)
 * - Rising edge detection for transition triggers
 * - Smooth 300ms hue transitions between stable states
 *
 * VISUAL DESIGN:
 * - Stable base color during normal playback (no rainbow cycling)
 * - Gentle breathing wave animation from CENTER ORIGIN
 * - +90 hue offset for Strip 2 (complementary harmony)
 * - Smooth hue transitions on harmonic events
 *
 * Instance State:
 * - m_currentHue, m_targetHue: Current and target hue for transitions
 * - m_hueTransition: Transition progress (0.0-1.0)
 * - m_lastSaliency: Previous frame saliency for edge detection
 * - m_phase: Breathing wave phase accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../CoreEffects.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

class HarmonicPaletteEffect : public plugins::IEffect {
public:
    HarmonicPaletteEffect();
    ~HarmonicPaletteEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // ==========================================================================
    // Hue State (Harmony-Driven)
    // ==========================================================================

    uint8_t m_currentHue;       ///< Current rendered hue (0-255)
    uint8_t m_targetHue;        ///< Target hue for active transition
    float m_hueTransition;      ///< Transition progress: 0.0=start, 1.0=complete

    // ==========================================================================
    // Saliency Edge Detection
    // ==========================================================================

    float m_lastSaliency;       ///< Previous frame harmonic saliency

    /// Threshold for triggering color shift (0.4 = moderate sensitivity)
    /// Higher = fewer transitions, lower = more reactive
    static constexpr float SALIENCY_THRESHOLD = 0.4f;

    /// Hue shift amount on each harmonic event (40 = ~1/6 of color wheel)
    /// This creates distinct but not jarring color changes
    static constexpr float HUE_SHIFT_AMOUNT = 40.0f;

    /// Transition duration in seconds (0.3 = 300ms smooth crossfade)
    static constexpr float TRANSITION_DURATION_SEC = 0.3f;

    // ==========================================================================
    // Breathing Wave Animation (Time-Based)
    // ==========================================================================

    float m_phase;              ///< Phase accumulator for breathing wave

    /// Hue offset for Strip 2 (90 = triadic harmony offset)
    static constexpr uint8_t STRIP2_HUE_OFFSET = 90;

    // ==========================================================================
    // Hysteresis State (Prevents Rapid-Fire Triggers)
    // ==========================================================================

    float m_cooldownTimer;      ///< Time since last trigger (prevents rapid-fire)
    static constexpr float COOLDOWN_DURATION_SEC = 0.15f;  ///< 150ms minimum between triggers
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
