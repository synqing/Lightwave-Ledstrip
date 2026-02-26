/**
 * @file LGPRGBPrismEffect.h
 * @brief LGP RGB Prism - RGB component splitting
 * 
 * Effect ID: 51
 * Family: COLOR_MIXING
 * Tags: CENTER_ORIGIN | SPECTRAL
 * 
 * Instance State:
 * - m_prismAngle: Dispersion phase accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPRGBPrismEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_RGB_PRISM;

    LGPRGBPrismEffect();
    ~LGPRGBPrismEffect() override = default;

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
    float m_prismAngle;
    float m_phaseRate;
    float m_dispersion;
    float m_centreBoost;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
