/**
 * @file ChevronWavesAmbientEffect.h
 * @brief Chevron Waves (Ambient) - Time-driven V-shaped wave propagation
 * 
 * Original clean implementation from c691ea3, restored as ambient variant.
 * Simple V-shaped patterns without audio dependencies.
 * 
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

class ChevronWavesAmbientEffect : public plugins::IEffect {
public:
    ChevronWavesAmbientEffect();
    ~ChevronWavesAmbientEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_chevronPos;
    
    static constexpr float CHEVRON_COUNT = 6.0f;
    static constexpr float CHEVRON_ANGLE = 1.5f;
    static constexpr uint8_t FADE_AMOUNT = 40;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
