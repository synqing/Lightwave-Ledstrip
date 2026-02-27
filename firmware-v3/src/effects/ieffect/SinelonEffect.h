/**
 * @file SinelonEffect.h
 * @brief Sinelon - Bouncing particle with palette trails
 * 
 * Effect ID: 4
 * Family: GEOMETRIC
 * Tags: CENTER_ORIGIN | TRAVELING
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class SinelonEffect : public plugins::IEffect {
public:
    SinelonEffect();
    ~SinelonEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // No instance state needed - uses beatsin16 which is deterministic
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

