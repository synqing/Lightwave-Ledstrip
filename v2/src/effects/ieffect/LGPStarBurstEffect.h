/**
 * @file LGPStarBurstEffect.h
 * @brief LGP Star Burst - Explosive radial lines
 * 
 * Effect ID: 24
 * Family: GEOMETRIC
 * Tags: CENTER_ORIGIN
 * 
 * Instance State:
 * - m_phase: Phase accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

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
    float m_phase;
    float m_burst = 0.0f;
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
    float m_hihatFlash = 0.0f;  // Hi-hat brightness overlay

    // Slew-limited speed (prevents jog-dial jitter - ChevronWaves golden pattern)
    float m_speedSmooth = 1.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
