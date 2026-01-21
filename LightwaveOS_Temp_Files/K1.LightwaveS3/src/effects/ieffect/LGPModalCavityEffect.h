/**
 * @file LGPModalCavityEffect.h
 * @brief LGP Modal Cavity - Resonant optical cavity modes
 * 
 * Effect ID: 31
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN | STANDING
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

class LGPModalCavityEffect : public plugins::IEffect {
public:
    LGPModalCavityEffect();
    ~LGPModalCavityEffect() override = default;

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
