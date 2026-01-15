/**
 * @file LGPColorAcceleratorEffect.h
 * @brief LGP Color Accelerator - Color cycling with momentum
 * 
 * Effect ID: 55
 * Family: COLOR_MIXING
 * Tags: CENTER_ORIGIN
 * 
 * Instance State:
 * - m_redParticle/m_blueParticle: Particle positions
 * - m_collision/m_debrisRadius: Collision state
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPColorAcceleratorEffect : public plugins::IEffect {
public:
    LGPColorAcceleratorEffect();
    ~LGPColorAcceleratorEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_redParticle;
    float m_blueParticle;
    bool m_collision;
    float m_debrisRadius;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
