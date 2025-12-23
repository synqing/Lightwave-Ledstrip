/**
 * @file LGPInterferenceScannerEffect.h
 * @brief LGP Interference Scanner - Scanning beam with interference fringes
 * 
 * Effect ID: 16
 * Family: INTERFERENCE
 * Tags: CENTER_ORIGIN | DUAL_STRIP | TRAVELING
 * 
 * Instance State:
 * - m_scanPhase/m_scanPhase2: Phase accumulators
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPInterferenceScannerEffect : public plugins::IEffect {
public:
    LGPInterferenceScannerEffect();
    ~LGPInterferenceScannerEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_scanPhase;
    float m_scanPhase2;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
