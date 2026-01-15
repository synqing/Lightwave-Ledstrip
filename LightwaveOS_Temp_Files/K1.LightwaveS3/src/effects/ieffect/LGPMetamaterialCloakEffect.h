/**
 * @file LGPMetamaterialCloakEffect.h
 * @brief LGP Metamaterial Cloak - Invisibility cloak simulation
 * 
 * Effect ID: 44
 * Family: QUANTUM
 * Tags: CENTER_ORIGIN | PHYSICS
 * 
 * Instance State:
 * - m_time: Time accumulator
 * - m_pos/m_vel: Cloak position and velocity
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPMetamaterialCloakEffect : public plugins::IEffect {
public:
    LGPMetamaterialCloakEffect();
    ~LGPMetamaterialCloakEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    uint16_t m_time;
    float m_pos;
    float m_vel;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
