/**
 * @file OceanEffect.h
 * @brief Ocean - Deep ocean wave patterns from centre point
 * 
 * Effect ID: 1
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN | TRAVELING
 * 
 * Instance State:
 * - m_waterOffset: Wave phase accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class OceanEffect : public plugins::IEffect {
public:
    OceanEffect();
    ~OceanEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Instance state (was: static uint32_t waterOffset)
    uint32_t m_waterOffset;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

