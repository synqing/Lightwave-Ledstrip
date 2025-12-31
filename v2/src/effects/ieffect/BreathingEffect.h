/**
 * @file BreathingEffect.h
 * @brief Breathing - Bloom-inspired effect using Sensory Bridge architecture
 *
 * Effect ID: 11
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN | AUDIO_REACTIVE
 *
 * REDESIGNED based on Sensory Bridge Bloom mode principles:
 * - Audio → Color/Brightness (AUDIO-REACTIVE)
 * - Time → Motion Speed (TIME-BASED, USER-CONTROLLED)
 * - Frame persistence: Alpha blending (0.99) for smooth motion
 * - Chromatic color: 12-bin chromagram → RGB color
 * - Multi-stage smoothing: Chromagram + Energy envelope
 *
 * Instance State:
 * - Motion: m_phase (time-based), m_currentRadius, m_prevRadius (frame persistence)
 * - Audio: m_chromaSmoothed[12], m_energySmoothed
 * - Other: m_pulseIntensity, m_fallbackPhase, m_texturePhase
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../audio/AudioBehaviorSelector.h"
#include "../enhancement/SmoothingEngine.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

/**
 * @brief Bloom-inspired breathing effect with robust audio reactivity
 *
 * Three rendering modes, ALL audio-reactive:
 * - BLOOM_BREATHE: RMS→radius with beat-synced modulation (or fallback)
 * - BLOOM_PULSE: Sharp radial expansion on beat/flux, BPM-adaptive decay
 * - BLOOM_TEXTURE: Slow drift modulated by timbral saliency
 *
 * Key innovation: Exponential propagation (Sensory Bridge Bloom's secret)
 * creates visual acceleration toward edges that feels musical.
 */
class BreathingEffect : public plugins::IEffect {
public:
    BreathingEffect();
    ~BreathingEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Behavior-specific render methods (all audio-reactive)
    void renderBreathing(plugins::EffectContext& ctx);    // BLOOM_BREATHE
    void renderPulsing(plugins::EffectContext& ctx);      // BLOOM_PULSE
    void renderTexture(plugins::EffectContext& ctx);      // BLOOM_TEXTURE

    // Audio behavior selector (for mode switching)
    audio::AudioBehaviorSelector m_selector;

    // Core state
    float m_currentRadius;      // Current radius (frame persistence)
    float m_prevRadius;         // Previous frame radius (for persistence)
    float m_pulseIntensity;     // Decaying pulse from beats/flux

    // Motion state (TIME-BASED, not audio-reactive)
    float m_phase;              // Time-based phase accumulator for breathing cycle

    // Fallback state (for when beat tracking is unreliable)
    float m_fallbackPhase;      // Free-running phase when tempo confidence low
    float m_lastFlux;           // Previous flux for transient detection
    float m_fluxBoost;          // Brightness boost from flux transients

    // Texture state
    float m_texturePhase;       // Texture drift phase accumulator

    // Multi-stage smoothing (Sensory Bridge pattern)
    float m_chromaSmoothed[12]; // Smoothed chromagram for color calculation
    float m_energySmoothed;      // Smoothed energy envelope for brightness

    // Frame-rate independent radius smoothing
    enhancement::AsymmetricFollower m_radiusFollower;

    // History buffer for spike filtering (4-frame rolling average)
    static constexpr uint8_t HISTORY_SIZE = 4;
    float m_radiusTargetHist[HISTORY_SIZE];
    float m_radiusTargetSum;
    uint8_t m_histIdx;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
