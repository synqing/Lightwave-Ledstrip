/**
 * @file LGPStarBurstAmbientEffect.h
 * @brief LGP Star Burst (Ambient) - Time-driven radial star patterns
 * 
 * Original clean implementation from c691ea3, restored as ambient variant.
 * Simple radial star patterns with pulsing, without audio dependencies.
 * 
 * Family: GEOMETRIC
 * Tags: CENTER_ORIGIN
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPStarBurstAmbientEffect : public plugins::IEffect {
public:
    LGPStarBurstAmbientEffect();
    ~LGPStarBurstAmbientEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_phase;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
