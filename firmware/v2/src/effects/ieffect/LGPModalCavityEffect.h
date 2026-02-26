/**
 * @file LGPModalCavityEffect.h
 * @brief LGP Modal Cavity - Resonant optical cavity modes
 * 
 * Effect ID: 31
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN | STANDING
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

class LGPModalCavityEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_MODAL_CAVITY;

    LGPModalCavityEffect();
    ~LGPModalCavityEffect() override = default;

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
    int m_modeNumber;
    int m_beatOffset;
    float m_phaseStep;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
