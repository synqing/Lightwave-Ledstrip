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
    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    float m_phase;
    float m_phaseRate;
    float m_diamondFreq;
    float m_radialFalloff;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
