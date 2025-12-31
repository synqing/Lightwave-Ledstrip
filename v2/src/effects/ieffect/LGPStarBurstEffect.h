/**
 * @file LGPStarBurstEffect.h
 * @brief LGP Star Burst - Explosive radial lines from center
 *
 * Effect ID: 24
 * Family: GEOMETRIC
 * Tags: CENTER_ORIGIN
 *
 * REPAIRED: Simplified to match Wave Collision's proven pattern.
 * Audio: snare→burst, heavyBass→speed, chroma→color
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../enhancement/SmoothingEngine.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPStarBurstEffect : public plugins::IEffect {
public:
    LGPStarBurstEffect();
    ~LGPStarBurstEffect() override = default;

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

    // Color state
    uint8_t m_dominantBin = 0;       // Dominant chroma bin
    float m_dominantBinSmooth = 0.0f; // Smoothed for stability

    // Speed state (Spring physics for natural momentum)
    enhancement::Spring m_phaseSpeedSpring;  // Critically damped - no overshoot
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
