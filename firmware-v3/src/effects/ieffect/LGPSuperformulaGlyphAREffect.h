/**
 * @file LGPSuperformulaGlyphAREffect.h
 * @brief Superformula Living Glyph (5-Layer Audio-Reactive) — REWRITTEN
 *
 * Effect ID: 0x1C09 (EID_LGP_SUPERFORMULA_GLYPH_AR)
 * Direct ControlBus reads, single-stage smoothing, max follower normalisation.
 * Superformula params (m, n1, n2, n3) still morph with audio.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

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
    float m_t = 0.0f;

    float m_bass       = 0.0f;
    float m_mid        = 0.0f;
    float m_chromaAngle = 0.0f;

    float m_bassMax    = 0.15f;
    float m_midMax     = 0.15f;

    float m_impact     = 0.0f;

    // Morphing superformula parameters
    float m_param_m   = 6.0f;
    float m_param_n1  = 1.0f;
    float m_param_n2  = 1.5f;
    float m_param_n3  = 1.5f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
