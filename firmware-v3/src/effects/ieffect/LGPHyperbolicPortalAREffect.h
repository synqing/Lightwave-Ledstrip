/**
 * @file LGPHyperbolicPortalAREffect.h
 * @brief Hyperbolic Portal (5-Layer Audio-Reactive)
 *
 * Effect ID: 0x1C11 (EID_LGP_HYPERBOLIC_PORTAL_AR)
 * Family: FIVE_LAYER_AR
 * Category: QUANTUM
 * Tags: CENTER_ORIGIN | DUAL_STRIP | PHYSICS | AUDIO_REACTIVE
 *
 * 5-layer composition model (NOT flat lerp):
 *   Bed       - RMS portal glow (tau ~0.40s)
 *   Structure - rib frequencies w1/w2 from harmonic+rhythmic (tau ~0.18s)
 *   Impact    - beat rib brightening (decay ~0.20s)
 *   Memory    - portal persistence (tau ~0.70s)
 *   Tonal     - chord hue anchor
 *
 * Composition: brightness = bed * portalShape + impact * ribs + memory
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"
#include "AudioReactiveLowRiskPackHelpers.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPHyperbolicPortalAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_HYPERBOLIC_PORTAL_AR;

    LGPHyperbolicPortalAREffect();
    ~LGPHyperbolicPortalAREffect() override = default;

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
    float m_phi = 0.0f;

    // 5-Layer composition state
    float m_bed         = 0.3f;
    float m_w1          = 8.0f;
    float m_w2          = 13.0f;
    float m_impact      = 0.0f;
    float m_memory      = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
