/**
 * @file LGPEvanescentDriftEffect.h
 * @brief LGP Evanescent Drift - Ghostly drifting particles
 * 
 * Effect ID: 29
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN
 * 
 * Instance State:
 * - m_phase1/m_phase2: Phase accumulators
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPEvanescentDriftEffect : public plugins::IEffect {
public:
    LGPEvanescentDriftEffect();
    ~LGPEvanescentDriftEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    uint16_t m_phase1;
    uint16_t m_phase2;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
