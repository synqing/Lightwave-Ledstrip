/**
 * @file LGPPerceptualBlendEffect.h
 * @brief LGP Perceptual Blend - Lab color space mixing
 * 
 * Effect ID: 59
 * Family: COLOR_MIXING
 * Tags: CENTER_ORIGIN
 * 
 * Instance State:
 * - m_phase: Blend phase accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPPerceptualBlendEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_PERCEPTUAL_BLEND;

    LGPPerceptualBlendEffect();
    ~LGPPerceptualBlendEffect() override = default;

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
    float m_lumaScale;
    float m_chromaScale;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
