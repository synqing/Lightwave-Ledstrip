/**
 * @file LGPCausticShardsEffect.h
 * @brief LGP Caustic Shards - Interference with prismatic glints
 *
 * Effect ID: 128
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN | DUAL_STRIP | SPECTRAL | PHYSICS
 *
 * Instance State:
 * - m_phase1/m_phase2/m_phase3: Layer phases
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPCausticShardsEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_CAUSTIC_SHARDS;

    LGPCausticShardsEffect();
    ~LGPCausticShardsEffect() override = default;

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
    float m_phase1;
    float m_phase2;
    float m_phase3;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
