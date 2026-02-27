/**
 * @file InterferenceEffect.h
 * @brief Interference - Basic wave interference
 * 
 * Effect ID: 10
 * Family: INTERFERENCE
 * Tags: CENTER_ORIGIN
 * 
 * Instance State:
 * - m_wave1Phase: Phase accumulator for wave 1
 * - m_wave2Phase: Phase accumulator for wave 2
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class InterferenceEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_INTERFERENCE;

    InterferenceEffect();
    ~InterferenceEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Instance state (was: static wave phases in effectInterference)
    float m_wave1Phase;
    float m_wave2Phase;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
