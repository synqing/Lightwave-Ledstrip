/**
 * @file LGPColorTemperatureEffect.h
 * @brief LGP Color Temperature - Blackbody radiation gradients
 * 
 * Effect ID: 50
 * Family: COLOR_MIXING
 * Tags: CENTER_ORIGIN
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPColorTemperatureEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_COLOR_TEMPERATURE;

    LGPColorTemperatureEffect() = default;
    ~LGPColorTemperatureEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
