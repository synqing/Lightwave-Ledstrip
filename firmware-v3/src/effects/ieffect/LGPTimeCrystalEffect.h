/**
 * @file LGPTimeCrystalEffect.h
 * @brief LGP Time Crystal - Periodic structure in time
 * 
 * Effect ID: 42
 * Family: QUANTUM
 * Tags: CENTER_ORIGIN | PHYSICS
 * 
 * Instance State:
 * - m_phase1/m_phase2/m_phase3: Phase accumulators
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPTimeCrystalEffect : public plugins::IEffect {
public:
    LGPTimeCrystalEffect();
    ~LGPTimeCrystalEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    uint16_t m_phase1;
    uint16_t m_phase2;
    uint16_t m_phase3;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
