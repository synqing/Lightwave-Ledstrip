/**
 * @file LGPStressGlassEffect.h
 * @brief LGP Stress Glass - Photoelastic fringe field
 *
 * Effect ID: 126
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN | DUAL_STRIP | SPECTRAL | PHYSICS
 *
 * Instance State:
 * - m_analyser: Polariser/analyser rotation phase
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPStressGlassEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_STRESS_GLASS;

    LGPStressGlassEffect();
    ~LGPStressGlassEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_analyser;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
