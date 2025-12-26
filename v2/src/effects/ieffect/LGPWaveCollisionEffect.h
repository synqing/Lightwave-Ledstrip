/**
 * @file LGPWaveCollisionEffect.h
 * @brief LGP Wave Collision - Colliding wave fronts from center
 * 
 * Effect ID: 17
 * Family: INTERFERENCE
 * Tags: CENTER_ORIGIN | DUAL_STRIP | TRAVELING
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPWaveCollisionEffect : public plugins::IEffect {
public:
    LGPWaveCollisionEffect() = default;
    ~LGPWaveCollisionEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // CRITICAL FIX: Single phase accumulator for traveling waves (not radii for standing waves)
    float m_phase = 0.0f;
    uint32_t m_lastHopSeq = 0;
    static constexpr uint8_t CHROMA_HISTORY = 4;
    float m_chromaEnergyHist[CHROMA_HISTORY] = {0.0f};
    float m_chromaEnergySum = 0.0f;
    uint8_t m_chromaHistIdx = 0;
    float m_energyAvg = 0.0f;
    float m_energyDelta = 0.0f;
    uint8_t m_dominantBin = 0;
    float m_energyAvgSmooth = 0.0f;
    float m_energyDeltaSmooth = 0.0f;
    float m_dominantBinSmooth = 0.0f;
    float m_collisionBoost = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
