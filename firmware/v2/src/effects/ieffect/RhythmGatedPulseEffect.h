/**
 * @file RhythmGatedPulseEffect.h
 * @brief Rhythm-gated pulse effect using rhythmicSaliency for intelligent beat response
 *
 * Effect ID: 90 (audio demo - saliency-aware)
 * Family: AUDIO_REACTIVE
 * Tags: CENTER_ORIGIN | AUDIO_SYNC | SALIENCY | MUSICAL_INTELLIGENCE
 *
 * KEY INNOVATION: Unlike amateur beat visualizers that pulse constantly,
 * this effect ONLY pulses when rhythm is musically salient. During melodic
 * or ambient sections, it gracefully fades to a gentle ambient pattern.
 *
 * rhythmicSaliency (0.0-1.0):
 * - HIGH (>0.35): Strong rhythmic content (drums, percussion, clear beat)
 * - LOW (<0.35): Melodic/ambient sections without prominent rhythm
 *
 * Behavior:
 * - When rhythmicSaliency > threshold: Sharp center-origin pulses on beat
 * - When rhythmicSaliency < threshold: Gentle ambient wave pattern
 * - Smooth crossfade between modes based on saliency level
 *
 * This is the FIRST effect in the codebase to utilize rhythmicSaliency (0% prior usage).
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../enhancement/SmoothingEngine.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class RhythmGatedPulseEffect : public plugins::IEffect {
public:
    RhythmGatedPulseEffect() = default;
    ~RhythmGatedPulseEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // ========================================================================
    // Pulse State (active during high rhythmic saliency)
    // ========================================================================

    float m_pulseIntensity = 0.0f;      ///< Current pulse brightness (0.0-1.0, decays after beat)
    bool m_lastBeat = false;             ///< Previous frame beat state for edge detection
    float m_lastBeatPhase = 0.0f;        ///< Previous beat phase for wrap detection

    // ========================================================================
    // Ambient State (active during low rhythmic saliency)
    // ========================================================================

    float m_ambientPhase = 0.0f;         ///< Phase accumulator for ambient wave animation

    // ========================================================================
    // Saliency Gating
    // ========================================================================

    /// Threshold for activating rhythm-responsive mode
    /// Below this: ambient mode. Above this: pulse mode.
    static constexpr float SALIENCY_THRESHOLD = 0.35f;

    /// Smoothed saliency value (100ms time constant prevents jitter)
    enhancement::AsymmetricFollower m_saliencyFollower{0.0f, 0.08f, 0.15f};

    // ========================================================================
    // Fallback State (for when audio unavailable)
    // ========================================================================

    float m_fallbackPhase = 0.0f;        ///< Free-running phase at 120 BPM
    uint32_t m_lastBeatTimeMs = 0;       ///< Debounce timer for beat detection
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
