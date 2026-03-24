/**
 * @file LGPMachDiamondsAREffect.h
 * @brief Mach Diamonds (5-Layer Audio-Reactive) — REWRITTEN
 *
 * Effect ID: 0x1C05 (EID_LGP_MACH_DIAMONDS_AR)
 * Family: FIVE_LAYER_AR
 * Category: QUANTUM
 * Tags: CENTER_ORIGIN | DUAL_STRIP | PHYSICS | AUDIO_REACTIVE
 *
 * Rewrite: Direct ControlBus reads, single-stage smoothing (30-60ms),
 * asymmetric max follower normalisation, audio drives brightness directly.
 * No updateSignals() helper layer.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPMachDiamondsAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_MACH_DIAMONDS_AR;

    LGPMachDiamondsAREffect();
    ~LGPMachDiamondsAREffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    float m_t = 0.0f;              // Phase accumulator

    // Single-stage smoothed audio (one EMA per signal)
    float m_bass       = 0.0f;     // Smoothed bass energy
    float m_treble     = 0.0f;     // Smoothed treble energy
    float m_chromaAngle = 0.0f;    // Circular chroma hue (radians)

    // Asymmetric max followers for dynamic gain normalisation
    float m_bassMax    = 0.15f;    // Bass max tracker
    float m_trebleMax  = 0.15f;    // Treble max tracker

    // Beat/percussion impact
    float m_impact     = 0.0f;     // Beat-driven impulse with exponential decay
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
