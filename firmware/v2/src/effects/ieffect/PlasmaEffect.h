/**
 * @file PlasmaEffect.h
 * @brief Plasma - Smoothly shifting color plasma
 * 
 * Effect ID: 2
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN
 * 
 * Instance State:
 * - m_plasmaTime: Time accumulator for plasma animation
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include <FastLED.h>
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class PlasmaEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_PLASMA;

    PlasmaEffect();
    ~PlasmaEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Instance state (was: static uint32_t plasmaTime)
    uint32_t m_plasmaTime;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

