/**
 * @file LGPBioluminescentWavesEffect.h
 * @brief LGP Bioluminescent Waves - Glowing plankton in waves
 * 
 * Effect ID: 35
 * Family: ORGANIC
 * Tags: CENTER_ORIGIN | TRAVELING
 * 
 * Instance State:
 * - m_wavePhase: Wave phase accumulator
 * - m_glowPoints/m_glowLife: Plankton emitters
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPBioluminescentWavesEffect : public plugins::IEffect {
public:
    LGPBioluminescentWavesEffect();
    ~LGPBioluminescentWavesEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    uint16_t m_wavePhase;
    uint8_t m_glowPoints[20];
    uint8_t m_glowLife[20];
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
