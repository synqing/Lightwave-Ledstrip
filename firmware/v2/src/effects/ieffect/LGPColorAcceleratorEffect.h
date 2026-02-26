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
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPColorAcceleratorEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_COLOR_ACCELERATOR;

    LGPColorAcceleratorEffect();
    ~LGPColorAcceleratorEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    float m_redParticle;
    float m_blueParticle;
    bool m_collision;
    float m_debrisRadius;
    float m_accelScale;
    float m_debrisRate;
    uint8_t m_trailLength;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
