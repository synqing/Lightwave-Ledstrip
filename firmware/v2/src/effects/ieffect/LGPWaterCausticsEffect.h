/**
 * @file LGPWaterCausticsEffect.h
 * @brief LGP Water Caustics - Ray-envelope caustic sheet
 *
 * Effect ID: 132
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN | DUAL_STRIP | SPECTRAL | PHYSICS
 *
 * Instance State:
 * - m_t1/m_t2: Time accumulators
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPWaterCausticsEffect : public plugins::IEffect {
public:
    LGPWaterCausticsEffect();
    ~LGPWaterCausticsEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_t1;
    float m_t2;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
