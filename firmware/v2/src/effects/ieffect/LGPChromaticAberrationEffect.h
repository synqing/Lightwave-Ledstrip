/**
 * @file LGPChromaticAberrationEffect.h
 * @brief LGP Chromatic Aberration - Lens dispersion edge effects
 * 
 * Effect ID: 58
 * Family: COLOR_MIXING
 * Tags: CENTER_ORIGIN | SPECTRAL
 * 
 * Instance State:
 * - m_lensPosition: Lens phase accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPChromaticAberrationEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_CHROMATIC_ABERRATION;

    LGPChromaticAberrationEffect();
    ~LGPChromaticAberrationEffect() override = default;

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
    float m_lensPosition;
    float m_phaseRate;
    float m_aberration;
    float m_focusSpread;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
