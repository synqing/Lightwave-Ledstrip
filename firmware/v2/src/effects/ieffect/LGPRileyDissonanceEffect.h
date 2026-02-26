/**
 * @file LGPRileyDissonanceEffect.h
 * @brief LGP Riley Dissonance - Op-art visual vibration
 * 
 * Effect ID: 64
 * Family: NOVEL_PHYSICS
 * Tags: CENTER_ORIGIN
 * 
 * Instance State:
 * - m_patternPhase
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPRileyDissonanceEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_RILEY_DISSONANCE;

    LGPRileyDissonanceEffect();
    ~LGPRileyDissonanceEffect() override = default;

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
    float m_patternPhase;
    float m_phaseRate;
    float m_baseFreq;
    float m_freqMismatch;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
