/**
 * @file LGPWaveCollisionEffect.h
 * @brief LGP Wave Collision - Colliding wave fronts from center
 * 
 * Effect ID: 17
 * Family: INTERFERENCE
 * Tags: CENTER_ORIGIN | DUAL_STRIP | TRAVELING
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPWaveCollisionEffect : public plugins::IEffect {
public:
    LGPWaveCollisionEffect() = default;
    ~LGPWaveCollisionEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
