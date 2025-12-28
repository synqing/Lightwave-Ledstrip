/**
 * @file ChevronWavesEffect.h
 * @brief LGP Chevron Waves - V-shaped wave propagation from center
 * 
 * Effect ID: 18
 * Family: GEOMETRIC
 * Tags: CENTER_ORIGIN | TRAVELING
 * 
 * Instance State:
 * - m_chevronPos: Phase accumulator for wave motion
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class ChevronWavesEffect : public plugins::IEffect {
public:
    ChevronWavesEffect();
    ~ChevronWavesEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Instance state (was: static float chevronPos)
    float m_chevronPos;
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
    float m_speedSmooth = 1.0f;  // Slew-limited speed to prevent jitter

    // Constants
    static constexpr uint8_t FADE_AMOUNT = 40;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
