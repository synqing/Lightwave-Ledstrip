/**
 * @file LGPDiamondLatticeEffect.h
 * @brief LGP Diamond Lattice - Interwoven diamond patterns
 * 
 * Effect ID: 18
 * Family: GEOMETRIC
 * Tags: CENTER_ORIGIN
 * 
 * Instance State:
 * - m_phase: Phase accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPDiamondLatticeEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_DIAMOND_LATTICE;

    LGPDiamondLatticeEffect();
    ~LGPDiamondLatticeEffect() override = default;

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
