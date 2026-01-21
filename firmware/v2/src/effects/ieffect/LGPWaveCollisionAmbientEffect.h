/**
 * @file LGPWaveCollisionAmbientEffect.h
 * @brief LGP Wave Collision (Ambient) - Time-driven wave packet interference
 * 
 * Original clean implementation from c691ea3, restored as ambient variant.
 * Simple wave packet interference without audio dependencies.
 * 
 * Family: INTERFERENCE
 * Tags: CENTER_ORIGIN | TRAVELING
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPWaveCollisionAmbientEffect : public plugins::IEffect {
public:
    LGPWaveCollisionAmbientEffect() = default;
    ~LGPWaveCollisionAmbientEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
