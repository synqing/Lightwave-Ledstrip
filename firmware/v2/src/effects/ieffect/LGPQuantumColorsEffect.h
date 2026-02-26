/**
 * @file LGPQuantumColorsEffect.h
 * @brief LGP Quantum Colors - Quantized energy levels
 * 
 * Effect ID: 53
 * Family: COLOR_MIXING
 * Tags: CENTER_ORIGIN
 * 
 * Instance State:
 * - m_waveFunction: Phase accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPQuantumColorsEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_QUANTUM_COLORS;

    LGPQuantumColorsEffect();
    ~LGPQuantumColorsEffect() override = default;

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
    float m_waveFunction;
    float m_phaseRate;
    int m_numStates;
    float m_uncertaintyScale;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
