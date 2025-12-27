/**
 * @file LGPInterferenceScannerEffect.h
 * @brief LGP Interference Scanner - Scanning beam with interference fringes
 * 
 * Effect ID: 16
 * Family: INTERFERENCE
 * Tags: CENTER_ORIGIN | DUAL_STRIP | TRAVELING
 * 
 * Instance State:
 * - m_scanPhase: Phase accumulator for synchronized ring and wave motion
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPInterferenceScannerEffect : public plugins::IEffect {
public:
    LGPInterferenceScannerEffect();
    ~LGPInterferenceScannerEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_scanPhase;
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

    // Speed slew limiting (jog-dial fix)
    float m_speedScaleSmooth = 1.0f;  // Smoothed speed value

    // Validation instrumentation
    float m_prevPhaseDelta = 0.0f;    // Previous frame phase delta for reversal detection
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
