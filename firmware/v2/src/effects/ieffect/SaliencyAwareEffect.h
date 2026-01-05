/**
 * @file SaliencyAwareEffect.h
 * @brief Effect that responds to Musical Intelligence System saliency metrics
 *
 * Demonstrates the full MIS feature set by adapting visual behavior based on
 * dominant saliency type (harmonic, rhythmic, timbral, dynamic).
 *
 * Effect ID: TBD (will be assigned in CoreEffects.cpp)
 * Family: AUDIO_REACTIVE
 * Tags: CENTER_ORIGIN | AUDIO_SYNC
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../enhancement/SmoothingEngine.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

class SaliencyAwareEffect : public plugins::IEffect {
public:
    SaliencyAwareEffect() = default;
    ~SaliencyAwareEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Visual Foundation: Multi-layer interference pattern phases
    float m_phase1 = 0.0f;              // Layer 1 phase (time-based)
    float m_phase2 = 0.0f;              // Layer 2 phase (time-based)
    float m_phase3 = 0.0f;              // Layer 3 phase (time-based)
    
    // Audio Enhancement: Saliency-driven modulation
    float m_rhythmicPulse = 0.0f;      // Pulse intensity for rhythmic saliency
    float m_timbralTexture = 0.0f;     // Texture intensity for timbral saliency
    float m_dynamicEnergy = 0.0f;      // Energy level for dynamic saliency
    
    // Hop sequence tracking (prevents per-frame noise)
    uint32_t m_lastHopSeq = 0;
    
    // Saliency target values (updated only on new audio hops)
    float m_targetHarmonic = 0.0f;
    float m_targetRhythmic = 0.0f;
    float m_targetTimbral = 0.0f;
    float m_targetDynamic = 0.0f;
    float m_targetOverall = 0.0f;
    float m_targetRms = 0.0f;
    
    // History buffer for spike filtering (Sensory Bridge pattern: 4-frame rolling average BEFORE smoothing)
    static constexpr uint8_t SALIENCY_HISTORY_SIZE = 4;
    float m_saliencyHistory[SALIENCY_HISTORY_SIZE][6] = {{0.0f}};  // 4 frames × 6 values (harmonic, rhythmic, timbral, dynamic, overall, rms)
    uint8_t m_saliencyHistIdx = 0;
    
    // Smoothed saliency values (using simple symmetric 0.75 interpolation like Sensory Bridge)
    float m_smoothHarmonic = 0.0f;
    float m_smoothRhythmic = 0.0f;
    float m_smoothTimbral = 0.0f;
    float m_smoothDynamic = 0.0f;
    float m_smoothOverall = 0.0f;
    float m_smoothRms = 0.0f;
    
    // Timbral texture tracking
    float m_lastTimbralSaliency = 0.0f;
    
    // Mode switching with hysteresis
    float m_currentMode = 0.0f;        // Current mode (0.0-3.0)
    float m_modeTransition = 0.0f;     // Smooth transition (0.0-3.0)
    float m_currentModeStrength = 0.0f; // Strength of current mode (for hysteresis)
    
    // Frequency smoothing (prevents visual jumps)
    float m_smoothFreq1 = 0.16f;       // Smoothed frequency 1
    float m_smoothFreq2 = 0.28f;       // Smoothed frequency 2
    float m_smoothFreq3 = 0.12f;       // Smoothed frequency 3
    
    // Center boost smoothing (prevents white flash)
    float m_saliencyBoostSmooth = 0.0f; // Smoothed center boost
    
    // History buffer for center boost (filters single-frame spikes)
    static constexpr uint8_t BOOST_HISTORY_SIZE = 4;
    float m_boostHistory[BOOST_HISTORY_SIZE] = {0.0f};
    float m_boostHistorySum = 0.0f;
    uint8_t m_boostHistIdx = 0;
    
    // Audio smoothing (AsymmetricFollower for mood-adjusted smoothing)
    enhancement::AsymmetricFollower m_harmonicFollower{0.0f, 0.20f, 0.50f};
    enhancement::AsymmetricFollower m_rhythmicFollower{0.0f, 0.20f, 0.50f};
    enhancement::AsymmetricFollower m_timbralFollower{0.0f, 0.20f, 0.50f};
    enhancement::AsymmetricFollower m_dynamicFollower{0.0f, 0.20f, 0.50f};
    enhancement::AsymmetricFollower m_overallFollower{0.0f, 0.20f, 0.50f};
    enhancement::AsymmetricFollower m_rmsFollower{0.0f, 0.05f, 0.30f};
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

