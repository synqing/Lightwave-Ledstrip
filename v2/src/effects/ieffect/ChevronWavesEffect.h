/**
 * @file ChevronWavesEffect.h
 * @brief LGP Chevron Waves - V-shaped wave propagation from center
 * 
 * Effect ID: 18
 * Family: GEOMETRIC
 * Tags: CENTER_ORIGIN | TRAVELING
 * 
 * Instance State:
 * - m_chevronPos: Phase accumulator for wave motion
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class ChevronWavesEffect : public plugins::IEffect {
public:
    ChevronWavesEffect();
    ~ChevronWavesEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Instance state (was: static float chevronPos)
    float m_chevronPos;
    
    // Constants
    static constexpr float CHEVRON_COUNT = 6.0f;
    static constexpr float CHEVRON_ANGLE = 1.5f;
    static constexpr uint8_t FADE_AMOUNT = 40;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

