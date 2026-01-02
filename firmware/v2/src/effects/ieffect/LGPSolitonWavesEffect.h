/**
 * @file LGPSolitonWavesEffect.h
 * @brief LGP Soliton Waves - Self-reinforcing wave packets
 * 
 * Effect ID: 43
 * Family: QUANTUM
 * Tags: CENTER_ORIGIN | PHYSICS | TRAVELING
 * 
 * Instance State:
 * - m_pos/m_vel/m_amp/m_hue: Soliton parameters
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPSolitonWavesEffect : public plugins::IEffect {
public:
    LGPSolitonWavesEffect();
    ~LGPSolitonWavesEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_pos[4];
    float m_vel[4];
    uint8_t m_amp[4];
    uint8_t m_hue[4];
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
