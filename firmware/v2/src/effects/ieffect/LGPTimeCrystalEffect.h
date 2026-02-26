/**
 * @file LGPTimeCrystalEffect.h
 * @brief LGP Time Crystal - Periodic structure in time
 * 
 * Effect ID: 42
 * Family: QUANTUM
 * Tags: CENTER_ORIGIN | PHYSICS
 * 
 * Instance State:
 * - m_phase1/m_phase2/m_phase3: Phase accumulators
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPTimeCrystalEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_TIME_CRYSTAL;

    LGPTimeCrystalEffect();
    ~LGPTimeCrystalEffect() override = default;

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
    uint16_t m_phase1;
    uint16_t m_phase2;
    uint16_t m_phase3;
    float m_phase1Rate;
    float m_phase2Rate;
    float m_phase3Rate;
    float m_paletteSpread;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
