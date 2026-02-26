/**
 * @file LGPAnisotropicCloakEffect.h
 * @brief LGP Anisotropic Cloak - Direction-dependent visibility
 * 
 * Effect ID: 48
 * Family: QUANTUM
 * Tags: CENTER_ORIGIN | PHYSICS
 * 
 * Instance State:
 * - m_time: Time accumulator
 * - m_pos/m_vel: Cloak position and velocity
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPAnisotropicCloakEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_ANISOTROPIC_CLOAK;

    LGPAnisotropicCloakEffect();
    ~LGPAnisotropicCloakEffect() override = default;

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
    float m_pos;
    float m_vel;
    float m_cloakRadius;
    float m_baseIndex;
    float m_anisotropy;
    float m_phaseStep;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
