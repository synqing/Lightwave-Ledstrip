/**
 * @file ConfettiEffect.h
 * @brief Confetti - Random colored speckles fading
 * 
 * Effect ID: 3
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN
 * 
 * Note: Stateful effect - uses buffer feedback (reads previous frame)
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include <FastLED.h>
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class ConfettiEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_CONFETTI;

    ConfettiEffect();
    ~ConfettiEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // No instance state needed - uses buffer feedback (stateful effect)
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

