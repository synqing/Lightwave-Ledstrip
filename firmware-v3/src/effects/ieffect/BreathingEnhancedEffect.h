/**
 * @file BreathingEnhancedEffect.h
 * @brief Breathing Enhanced - Enhanced version with 64-bin sub-bass, beatPhase sync, snare triggers
 *
 * Effect ID: 89
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN | AUDIO_REACTIVE
 *
 * Enhancements over BreathingEffect (ID 11):
 * - Uses heavy_chroma for chromatic color (not raw chroma)
 * - 64-bin sub-bass (bins 0-5) for kick-driven pulse intensity
 * - Uses beatPhase() for beat-synced breathing when tempo confidence high
 * - Adds isSnareHit() for sharp pulse triggers
 * - Improved AsymmetricFollower smoothing parameters
 * - Better fallback: Slow breathing animation when audio unavailable
 * - Simplified: Removed AudioBehaviorSelector (uses saliency internally)
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../enhancement/SmoothingEngine.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class BreathingEnhancedEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_BREATHING_ENHANCED;

    BreathingEnhancedEffect();
    ~BreathingEnhancedEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Core state
    float m_currentRadius;      // Current radius (frame persistence)
    float m_prevRadius;         // Previous frame radius (for persistence)
    float m_pulseIntensity;     // Decaying pulse from beats/snare

    // Motion state (TIME-BASED, not audio-reactive)
    float m_phase;              // Time-based phase accumulator for breathing cycle

    // Fallback state (for when beat tracking is unreliable)
    float m_fallbackPhase;      // Free-running phase when tempo confidence low

    // Multi-stage smoothing
    float m_chromaSmoothed[12]; // Smoothed chromagram for color calculation
    float m_energySmoothed;      // Smoothed energy envelope for brightness
    
    // AsymmetricFollower for chromagram and RMS smoothing
    enhancement::AsymmetricFollower m_chromaFollowers[12];
    enhancement::AsymmetricFollower m_rmsFollower{0.0f, 0.05f, 0.30f};
    enhancement::AsymmetricFollower m_subBassFollower{0.0f, 0.05f, 0.30f};
    
    // Hop sequence tracking
    uint32_t m_lastHopSeq = 0;
    float m_chromaTargets[12] = {0.0f};
    float m_targetRms = 0.0f;
    float m_targetSubBass = 0.0f;

    // History buffer for spike filtering (4-frame rolling average)
    static constexpr uint8_t HISTORY_SIZE = 4;
    float m_radiusTargetHist[HISTORY_SIZE];
    float m_radiusTargetSum;
    uint8_t m_histIdx;
    
    // Tempo lock hysteresis (Schmitt trigger: 0.6 lock / 0.4 unlock)
    bool m_tempoLocked = false;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
