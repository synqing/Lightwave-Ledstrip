/**
 * @file RippleEffect.h
 * @brief Ripple - Expanding water ripples
 * 
 * Effect ID: 8
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN | TRAVELING
 * 
 * Instance State:
 * - m_ripples[5]: Array of active ripple states
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class RippleEffect : public plugins::IEffect {
public:
    RippleEffect();
    ~RippleEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Instance state (was: static struct ripples[5])
    struct Ripple {
        float radius;
        float speed;
        uint8_t hue;
        bool active;
    };
    static constexpr uint8_t MAX_RIPPLES = 5;
    Ripple m_ripples[MAX_RIPPLES];
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

