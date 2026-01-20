/**
 * @file LGPStarBurstEnhancedEffect.h
 * @brief LGP Star Burst Enhanced - Enhanced version with 64-bin sub-bass, enhanced snare/hi-hat triggers
 *
 * Effect ID: 95
 * Family: GEOMETRIC
 * Tags: CENTER_ORIGIN
 *
 * Enhancements over LGPStarBurstEffect (ID 24):
 * - 64-bin sub-bass (bins 0-5) for bass-driven burst intensity
 * - Enhanced isSnareHit() burst intensity and decay
 * - Uses beatPhase() for radial line synchronization
 * - Improved Spring smoothing for speed modulation
 * - Adds isHihatHit() for additional sparkle bursts
 * - Uses heavy_chroma consistently for color
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../enhancement/SmoothingEngine.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPStarBurstEnhancedEffect : public plugins::IEffect {
public:
    LGPStarBurstEnhancedEffect();
    ~LGPStarBurstEnhancedEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Core state (simplified - matches Wave Collision pattern)
    float m_phase;              // Phase accumulator
    float m_burst = 0.0f;       // Snare-driven burst intensity
    uint32_t m_lastHopSeq = 0;  // Audio hop sequence tracking

    // Chromagram smoothing (AsymmetricFollower for natural attack/release)
    enhancement::AsymmetricFollower m_chromaFollowers[12];
    float m_chromaSmoothed[12] = {0.0f};
    float m_chromaTargets[12] = {0.0f};
    
    // Color state
    uint8_t m_dominantBin = 0;       // Dominant chroma bin
    float m_dominantBinSmooth = 0.0f; // Smoothed for stability

    // Speed state (Spring physics for natural momentum)
    enhancement::Spring m_phaseSpeedSpring;  // Critically damped - no overshoot
    
    // Enhanced: 64-bin sub-bass tracking
    enhancement::AsymmetricFollower m_subBassFollower{0.0f, 0.05f, 0.30f};
    float m_subBassEnergy = 0.0f;
    float m_targetSubBass = 0.0f;
    
    // Enhanced: Hi-hat sparkle burst
    float m_hihatSparkle = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
