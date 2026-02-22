/**
 * @file LGPSpiralVortexEffect.h
 * @brief LGP Spiral Vortex - Rotating spiral arms
 * 
 * Effect ID: 20
 * Family: GEOMETRIC
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

class LGPSpiralVortexEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_SPIRAL_VORTEX;

    LGPSpiralVortexEffect();
    ~LGPSpiralVortexEffect() override = default;

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
