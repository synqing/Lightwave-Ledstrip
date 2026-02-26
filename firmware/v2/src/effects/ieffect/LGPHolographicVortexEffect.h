/**
 * @file LGPHolographicVortexEffect.h
 * @brief LGP Holographic Vortex - Deep 3D vortex illusion
 * 
 * Effect ID: 28
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN | DEPTH
 * 
 * Instance State:
 * - m_time: Phase accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPHolographicVortexEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_HOLOGRAPHIC_VORTEX;

    LGPHolographicVortexEffect();
    ~LGPHolographicVortexEffect() override = default;

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
    uint16_t m_time;
    int m_spiralCount;
    float m_tightnessScale;
    float m_phaseStep;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
