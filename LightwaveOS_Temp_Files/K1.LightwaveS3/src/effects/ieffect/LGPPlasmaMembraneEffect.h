/**
 * @file LGPPlasmaMembraneEffect.h
 * @brief LGP Plasma Membrane - Cellular membrane fluctuations
 * 
 * Effect ID: 36
 * Family: ORGANIC
 * Tags: CENTER_ORIGIN
 * 
 * Instance State:
 * - m_time: Time accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPPlasmaMembraneEffect : public plugins::IEffect {
public:
    LGPPlasmaMembraneEffect();
    ~LGPPlasmaMembraneEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    uint16_t m_time;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
