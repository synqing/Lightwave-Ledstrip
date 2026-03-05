/**
 * @file LGPSuperformulaGlyphAREffect.h
 * @brief Superformula Living Glyph (5-Layer Audio-Reactive)
 *
 * Effect ID: 0x1C09 (EID_LGP_SUPERFORMULA_GLYPH_AR)
 * Family: FIVE_LAYER_AR
 * Category: QUANTUM
 * Tags: CENTER_ORIGIN | DUAL_STRIP | ORGANIC | AUDIO_REACTIVE
 *
 * 5-layer composition model (NOT flat lerp):
 *   Bed       - slow RMS-driven atmosphere (tau ~0.42s)
 *   Structure - superformula param morphing from harmonic + flux (tau ~0.20s)
 *   Impact    - beat-triggered glyph brightening (decay ~0.18s)
 *   Tonal     - chord-driven hue anchor
 *   Memory    - sigil persistence accumulator (decay ~0.75s)
 *
 * Composition: brightness = bed * (band_wave * glue) + impact + memory
 *
 * Superformula: r(phi) = (|cos(m*phi/4)/a|^n2 + |sin(m*phi/4)/b|^n3)^(-1/n1)
 * Distance-to-curve band rendering with exponential falloff.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"
#include "AudioReactiveLowRiskPackHelpers.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPSuperformulaGlyphAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_SUPERFORMULA_GLYPH_AR;

    LGPSuperformulaGlyphAREffect();
    ~LGPSuperformulaGlyphAREffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    lowrisk_ar::Ar16Controls m_controls;
    lowrisk_ar::ArRuntimeState m_ar;
    float m_t = 0.0f;

    // 5-Layer composition state
    float m_bed       = 0.3f;
    float m_impact    = 0.0f;
    float m_memory    = 0.0f;

    // Morphing superformula parameters (driven by Structure layer)
    float m_param_m   = 6.0f;   // Symmetry (3-11)
    float m_param_n1  = 1.0f;   // Overall shape (0.7-1.6)
    float m_param_n2  = 1.5f;   // Cos exponent (0.8-2.4)
    float m_param_n3  = 1.5f;   // Sin exponent (0.8-2.4)
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
