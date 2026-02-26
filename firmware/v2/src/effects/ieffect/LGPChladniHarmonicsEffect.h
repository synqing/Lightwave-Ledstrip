/**
 * @file LGPChladniHarmonicsEffect.h
 * @brief LGP Chladni Harmonics - Resonant nodal patterns
 * 
 * Effect ID: 60
 * Family: NOVEL_PHYSICS
 * Tags: CENTER_ORIGIN | STANDING
 * 
 * Instance State:
 * - m_vibrationPhase/m_mixPhase: Phase accumulators
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPChladniHarmonicsEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_CHLADNI_HARMONICS;

    LGPChladniHarmonicsEffect();
    ~LGPChladniHarmonicsEffect() override = default;

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
    float m_vibrationPhase;
    float m_mixPhase;
    int m_modeNumber;
    float m_vibrationRate;
    float m_mixRate;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
