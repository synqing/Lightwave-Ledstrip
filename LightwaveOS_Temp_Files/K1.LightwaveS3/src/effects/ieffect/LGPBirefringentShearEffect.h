/**
 * @file LGPBirefringentShearEffect.h
 * @brief LGP Birefringent Shear - Polarization splitting
 * 
 * Effect ID: 47
 * Family: QUANTUM
 * Tags: CENTER_ORIGIN | PHYSICS | SPECTRAL
 * 
 * Instance State:
 * - m_time: Time accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPBirefringentShearEffect : public plugins::IEffect {
public:
    LGPBirefringentShearEffect();
    ~LGPBirefringentShearEffect() override = default;

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
