/**
 * @file PulseEffect.h
 * @brief Pulse - Sharp energy pulses from centre
 * 
 * Effect ID: 12
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN | TRAVELING
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class PulseEffect : public plugins::IEffect {
public:
    PulseEffect() = default;
    ~PulseEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
