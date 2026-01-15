/**
 * @file DynamicEnvelopeEffect.h
 * @brief Dynamic envelope effect - brightness follows musical dynamics (crescendo/diminuendo)
 *
 * Effect ID: TBD (audio demo)
 * Family: AUDIO_REACTIVE
 * Tags: CENTER_ORIGIN | AUDIO_SYNC | DYNAMICS
 *
 * DESIGN PRINCIPLE:
 * This effect uses dynamicSaliency - the ONLY audio feature at 0% utilization in the codebase.
 * Instead of reacting to frequency content (like most effects), this effect responds to
 * musical DYNAMICS - the emotional arc of volume changes over time.
 *
 * BEHAVIOR:
 * - HIGH dynamicSaliency (crescendo/sforzando/sudden quiet) -> Dramatic brightness changes
 * - LOW dynamicSaliency (steady-state volume) -> Stable, consistent brightness
 * - Creates an effect that "breathes" with the music's emotional arc
 *
 * RMS TREND TRACKING:
 * - Rolling 8-sample history splits into old (0-3) and new (4-7) averages
 * - Positive trend = crescendo (rising volume) -> brighten
 * - Negative trend = diminuendo (falling volume) -> dim
 * - Only applies changes when dynamicSaliency exceeds threshold
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../enhancement/SmoothingEngine.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class DynamicEnvelopeEffect : public plugins::IEffect {
public:
    DynamicEnvelopeEffect() = default;
    ~DynamicEnvelopeEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // ========================================================================
    // Brightness Envelope State
    // ========================================================================

    float m_targetBrightness = 0.7f;    ///< Target brightness (0.2 - 1.0)
    float m_currentBrightness = 0.7f;   ///< Smoothed current brightness

    // ========================================================================
    // Dynamic Tracking
    // ========================================================================

    /// Smoothed dynamic saliency value (filters out transient spikes)
    float m_smoothedDynamic = 0.0f;

    /// Rolling RMS history for trend detection (8 samples)
    static constexpr uint8_t RMS_HISTORY_SIZE = 8;
    float m_rmsHistory[RMS_HISTORY_SIZE] = {0.0f};
    uint8_t m_histIdx = 0;

    // ========================================================================
    // Base Animation
    // ========================================================================

    float m_phase = 0.0f;  ///< Animation phase accumulator

    // ========================================================================
    // Smoothing Followers (AsymmetricFollower for natural attack/release)
    // ========================================================================

    enhancement::AsymmetricFollower m_dynamicFollower{0.0f, 0.08f, 0.25f};  ///< Dynamic saliency smoother
    enhancement::AsymmetricFollower m_brightnessFollower{0.7f, 0.12f, 0.20f}; ///< Brightness envelope

    // ========================================================================
    // Hop Sequence Tracking
    // ========================================================================

    uint32_t m_lastHopSeq = 0;
    float m_targetDynamic = 0.0f;
    float m_targetRms = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
