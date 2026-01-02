/**
 * @file LGPHolographicVortexEffect.h
 * @brief LGP Holographic Vortex - Deep 3D vortex illusion
 * 
 * Effect ID: 28
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN | DEPTH
 * 
 * Instance State:
 * - m_time: Phase accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPHolographicVortexEffect : public plugins::IEffect {
public:
    LGPHolographicVortexEffect();
    ~LGPHolographicVortexEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    uint16_t m_time;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
