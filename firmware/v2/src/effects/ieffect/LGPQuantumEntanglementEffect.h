/**
 * @file LGPQuantumEntanglementEffect.h
 * @brief LGP Quantum Entanglement - Correlated state collapse
 * 
 * Effect ID: 62
 * Family: NOVEL_PHYSICS
 * Tags: CENTER_ORIGIN | PHYSICS
 * 
 * Instance State:
 * - m_collapseRadius/m_holdTime/m_measurementTimer/m_quantumPhase
 * - m_collapsing/m_collapsed/m_collapsedHue
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPQuantumEntanglementEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_QUANTUM_ENTANGLEMENT;

    LGPQuantumEntanglementEffect();
    ~LGPQuantumEntanglementEffect() override = default;

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
    float m_collapseRadius;
    bool m_collapsing;
    bool m_collapsed;
    float m_holdTime;
    uint8_t m_collapsedHue;
    float m_quantumPhase;
    float m_measurementTimer;
    float m_measurementRate;
    float m_collapseRate;
    float m_holdDuration;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
