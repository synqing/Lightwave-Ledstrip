/**
 * @file LGPRadialRippleEffect.h
 * @brief LGP Radial Ripple - Complex radial wave interference
 * 
 * Effect ID: 27
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN | TRAVELING
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

class LGPRadialRippleEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_RADIAL_RIPPLE;

    LGPRadialRippleEffect();
    ~LGPRadialRippleEffect() override = default;

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
    int m_ringCount;
    float m_ringSpeedScale;
    float m_distScale;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
