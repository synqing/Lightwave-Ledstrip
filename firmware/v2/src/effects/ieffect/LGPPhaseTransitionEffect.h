/**
 * @file LGPPhaseTransitionEffect.h
 * @brief LGP Phase Transition - State change simulation
 * 
 * Effect ID: 57
 * Family: COLOR_MIXING
 * Tags: CENTER_ORIGIN
 * 
 * Instance State:
 * - m_phaseAnimation: Phase accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPPhaseTransitionEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_PHASE_TRANSITION;

    LGPPhaseTransitionEffect();
    ~LGPPhaseTransitionEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_phaseAnimation;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
