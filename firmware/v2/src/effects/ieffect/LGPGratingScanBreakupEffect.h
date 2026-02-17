/**
 * @file LGPGratingScanBreakupEffect.h
 * @brief LGP Grating Scan (Breakup) - Halo spatter decay
 *
 * Effect ID: 131
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN | DUAL_STRIP | SPECTRAL | TRAVELING
 *
 * Instance State:
 * - m_pos: Scan position along centre distance axis
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPGratingScanBreakupEffect : public plugins::IEffect {
public:
    LGPGratingScanBreakupEffect();
    ~LGPGratingScanBreakupEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_pos;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
