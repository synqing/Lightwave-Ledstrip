/**
 * @file LGPStressGlassMeltEffect.h
 * @brief LGP Stress Glass (Melt) - Phase-locked wings
 *
 * Effect ID: 130
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN | DUAL_STRIP | SPECTRAL | PHYSICS
 *
 * Instance State:
 * - m_analyser: Polariser/analyser rotation phase
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPStressGlassMeltEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_STRESS_GLASS_MELT;

    LGPStressGlassMeltEffect();
    ~LGPStressGlassMeltEffect() override = default;

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
    float m_analyser;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
