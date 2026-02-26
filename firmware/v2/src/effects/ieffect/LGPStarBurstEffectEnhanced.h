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
#include "ChromaUtils.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPStarBurstEnhancedEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_STAR_BURST_ENHANCED;

    LGPStarBurstEnhancedEffect();
    ~LGPStarBurstEnhancedEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    // Core state (simplified - matches Wave Collision pattern)
    float m_phase;              // Phase accumulator
    float m_burst = 0.0f;       // Snare-driven burst intensity
    uint32_t m_lastHopSeq = 0;  // Audio hop sequence tracking

    // Chromagram smoothing (AsymmetricFollower for natural attack/release)
    enhancement::AsymmetricFollower m_chromaFollowers[12];
    float m_chromaSmoothed[12] = {0.0f};
    float m_chromaTargets[12] = {0.0f};
    
    // Circular chroma hue state (replaces argmax + linear EMA)
    float m_chromaAngle = 0.0f;      // Persistent angle for circular EMA (radians)

    // Speed state (Spring physics for natural momentum)
    enhancement::Spring m_phaseSpeedSpring;  // Critically damped - no overshoot
    
    // Enhanced: 64-bin sub-bass tracking
    enhancement::AsymmetricFollower m_subBassFollower{0.0f, 0.05f, 0.30f};
    float m_subBassEnergy = 0.0f;
    float m_targetSubBass = 0.0f;
    
    // Enhanced: Hi-hat sparkle burst
    float m_hihatSparkle = 0.0f;
    
    // Tempo lock hysteresis (Schmitt trigger: 0.6 lock / 0.4 unlock)
    bool m_tempoLocked = false;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
