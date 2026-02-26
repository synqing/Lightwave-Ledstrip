/**
 * @file LGPMoireSilkEffect.h
 * @brief LGP Moire Silk - Two-lattice beat pattern
 *
 * Effect ID: 127
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN | DUAL_STRIP | MOIRE
 *
 * Instance State:
 * - m_phaseA: First lattice phase
 * - m_phaseB: Second lattice phase
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPMoireSilkEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_MOIRE_SILK;

    LGPMoireSilkEffect();
    ~LGPMoireSilkEffect() override = default;

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
    float m_phaseA;
    float m_phaseB;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
