/**
 * @file LGPAuroraBorealisEffect.h
 * @brief LGP Aurora Borealis - Shimmering curtain lights
 * 
 * Effect ID: 34
 * Family: ORGANIC
 * Tags: CENTER_ORIGIN | SPECTRAL
 * 
 * Instance State:
 * - m_time: Time accumulator
 * - m_curtainPhase: Per-curtain phase offsets
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPAuroraBorealisEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_AURORA_BOREALIS;

    LGPAuroraBorealisEffect();
    ~LGPAuroraBorealisEffect() override = default;

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
    uint8_t m_curtainPhase[5];
    float m_phaseRate;
    float m_curtainDrift;
    float m_shimmerAmount;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
