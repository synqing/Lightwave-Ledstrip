/**
 * @file LGPSpiralVortexEffect.h
 * @brief LGP Spiral Vortex - Rotating spiral arms
 * 
 * Effect ID: 20
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

class LGPSpiralVortexEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_SPIRAL_VORTEX;

    LGPSpiralVortexEffect();
    ~LGPSpiralVortexEffect() override = default;

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
    int m_spiralArms;
    float m_radialFade;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
