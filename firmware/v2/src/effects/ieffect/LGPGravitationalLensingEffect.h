/**
 * @file LGPGravitationalLensingEffect.h
 * @brief LGP Gravitational Lensing - Light bending around mass
 * 
 * Effect ID: 41
 * Family: QUANTUM
 * Tags: CENTER_ORIGIN | PHYSICS
 * 
 * Instance State:
 * - m_time: Time accumulator
 * - m_massPos/m_massVel: Mass positions and velocities
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPGravitationalLensingEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_GRAVITATIONAL_LENSING;

    LGPGravitationalLensingEffect();
    ~LGPGravitationalLensingEffect() override = default;

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
    float m_massPos[3];
    float m_massVel[3];
    int m_massCount;
    float m_massStrengthScale;
    float m_deflectionGain;
    float m_phaseStep;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
