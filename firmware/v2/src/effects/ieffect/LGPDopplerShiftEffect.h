/**
 * @file LGPDopplerShiftEffect.h
 * @brief LGP Doppler Shift - Red/Blue shift based on velocity
 * 
 * Effect ID: 54
 * Family: COLOR_MIXING
 * Tags: CENTER_ORIGIN | TRAVELING
 * 
 * Instance State:
 * - m_sourcePosition: Moving source position
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPDopplerShiftEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_DOPPLER_SHIFT;

    LGPDopplerShiftEffect();
    ~LGPDopplerShiftEffect() override = default;

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
    float m_sourcePosition;
    float m_motionScale;
    float m_velocityScale;
    float m_shiftStrength;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
