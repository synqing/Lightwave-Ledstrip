/**
 * @file LGPMoireCurtainsEffect.h
 * @brief LGP Moire Curtains - Shifting moire interference layers
 * 
 * Effect ID: 26
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN | MOIRE
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

class LGPMoireCurtainsEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_MOIRE_CURTAINS;

    LGPMoireCurtainsEffect();
    ~LGPMoireCurtainsEffect() override = default;

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
    uint16_t m_phase;
    float m_baseFreq;
    float m_deltaFreq;
    float m_phaseStep;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
