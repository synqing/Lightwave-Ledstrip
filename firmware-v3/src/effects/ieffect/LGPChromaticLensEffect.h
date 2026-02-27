/**
 * @file LGPChromaticLensEffect.h
 * @brief LGP Chromatic Lens - Simulated lens dispersion
 * 
 * Effect ID: 65
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN | SPECTRAL
 * 
 * Instance State:
 * - m_phase
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPChromaticLensEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_CHROMATIC_LENS;

    LGPChromaticLensEffect();
    ~LGPChromaticLensEffect() override = default;

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
