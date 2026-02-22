/**
 * @file LGPComplementaryMixingEffect.h
 * @brief LGP Complementary Mixing - Complementary color gradients
 * 
 * Effect ID: 52
 * Family: COLOR_MIXING
 * Tags: CENTER_ORIGIN
 * 
 * Instance State:
 * - m_phase: Phase accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPComplementaryMixingEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_COMPLEMENTARY_MIXING;

    LGPComplementaryMixingEffect();
    ~LGPComplementaryMixingEffect() override = default;

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
