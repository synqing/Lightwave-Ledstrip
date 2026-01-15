/**
 * @file LGPConcentricRingsEffect.h
 * @brief LGP Concentric Rings - Expanding circular rings
 * 
 * Effect ID: 23
 * Family: GEOMETRIC
 * Tags: CENTER_ORIGIN
 * 
 * Instance State:
 * - m_phase: Phase accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPConcentricRingsEffect : public plugins::IEffect {
public:
    LGPConcentricRingsEffect();
    ~LGPConcentricRingsEffect() override = default;

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
