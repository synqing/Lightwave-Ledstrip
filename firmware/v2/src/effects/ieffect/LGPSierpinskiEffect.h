/**
 * @file LGPSierpinskiEffect.h
 * @brief LGP Sierpinski - Fractal triangle generation
 * 
 * Effect ID: 21
 * Family: GEOMETRIC
 * Tags: CENTER_ORIGIN
 * 
 * Instance State:
 * - m_iteration: Iteration counter
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPSierpinskiEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_SIERPINSKI;

    LGPSierpinskiEffect();
    ~LGPSierpinskiEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    uint16_t m_iteration;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
