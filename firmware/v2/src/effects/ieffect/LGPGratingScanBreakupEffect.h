/**
 * @file LGPGratingScanBreakupEffect.h
 * @brief LGP Grating Scan (Breakup) - Halo spatter decay
 *
 * Effect ID: 131
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN | DUAL_STRIP | SPECTRAL | TRAVELING
 *
 * Instance State:
 * - m_pos: Scan position along centre distance axis
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPGratingScanBreakupEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_GRATING_SCAN_BREAKUP;

    LGPGratingScanBreakupEffect();
    ~LGPGratingScanBreakupEffect() override = default;

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
    float m_pos;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
