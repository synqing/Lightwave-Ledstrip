/**
 * @file LGPPerceptualBlendEffect.h
 * @brief LGP Perceptual Blend - Lab colour space mixing via gradient kernel
 *
 * Effect ID: 59 (0x0709)
 * Family: COLOR_MIXING
 * Tags: CENTER_ORIGIN
 *
 * Uses the shared gradient kernel for multi-stop palette ramp composition.
 * Strip 1 blends hue1→hue2, Strip 2 blends hue2→hue3 (complementary pair).
 *
 * Instance State:
 * - m_phase: Blend phase accumulator
 * - m_rampA / m_rampB: Gradient ramps for strip 1 and strip 2
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"
#include "../gradient/GradientRamp.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPPerceptualBlendEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_PERCEPTUAL_BLEND;

    LGPPerceptualBlendEffect();
    ~LGPPerceptualBlendEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_phase;
    gradient::GradientRamp m_rampA;  ///< Strip 1 ramp (hue1 → hue2)
    gradient::GradientRamp m_rampB;  ///< Strip 2 ramp (hue2 → hue3)
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
