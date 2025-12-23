/**
 * @file BPMEffect.h
 * @brief BPM - Beat-synced pulsing sawtooth waves
 * 
 * Effect ID: 6
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

class BPMEffect : public plugins::IEffect {
public:
    BPMEffect();
    ~BPMEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // No instance state needed - uses beatsin8 which is deterministic
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

