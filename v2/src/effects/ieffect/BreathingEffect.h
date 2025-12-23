/**
 * @file BreathingEffect.h
 * @brief Breathing - Smooth expansion and contraction from center
 * 
 * Effect ID: 11
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN
 * 
 * Instance State:
 * - m_breathPhase: Breathing phase accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class BreathingEffect : public plugins::IEffect {
public:
    BreathingEffect();
    ~BreathingEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Instance state (was: static breathPhase in effectBreathing)
    float m_breathPhase;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
