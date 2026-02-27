/**
 * @file LGPCausticFanEffect.h
 * @brief LGP Caustic Fan - Focused light caustics
 * 
 * Effect ID: 46
 * Family: QUANTUM
 * Tags: CENTER_ORIGIN | PHYSICS
 * 
 * Instance State:
 * - m_time: Time accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPCausticFanEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_CAUSTIC_FAN;

    LGPCausticFanEffect();
    ~LGPCausticFanEffect() override = default;

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
