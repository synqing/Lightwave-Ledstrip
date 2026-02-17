/**
 * @file LGPSchlierenFlowEffect.h
 * @brief LGP Schlieren Flow - Knife-edge gradient flow
 *
 * Effect ID: 133
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN | DUAL_STRIP | SPECTRAL | PHYSICS
 *
 * Instance State:
 * - m_t: Time accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPSchlierenFlowEffect : public plugins::IEffect {
public:
    LGPSchlierenFlowEffect();
    ~LGPSchlierenFlowEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_t;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
