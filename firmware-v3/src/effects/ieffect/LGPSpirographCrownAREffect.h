/**
 * @file LGPSpirographCrownAREffect.h
 * @brief Spirograph Crown (5-Layer Audio-Reactive)
 *
 * Effect ID: 0x1C0A (EID_LGP_SPIROGRAPH_CROWN_AR)
 * Family: FIVE_LAYER_AR
 * Category: QUANTUM
 * Tags: CENTER_ORIGIN | DUAL_STRIP | PHYSICS | AUDIO_REACTIVE
 *
 * 5-layer composition model (NOT flat lerp):
 *   Bed       - slow RMS-driven atmosphere (tau ~0.40s)
 *   Structure - r parameter + facet sparkle from rhythmic+mid (tau ~0.16s)
 *   Impact    - beat crown flash at hypotrochoid band (decay ~0.20s)
 *   Tonal     - chord-driven crown hue anchor
 *   Memory    - post-impact crown persistence (decay ~0.70s)
 *
 * Composition: brightness = bed * structuredGeom + impact + memory
 *
 * Source maths: Hypotrochoid parametric equations
 *   r(theta) = sqrt(x^2 + y^2)
 *   x = (R - r)*cos(t) + d*cos((R - r)/r * t)
 *   y = (R - r)*sin(t) - d*sin((R - r)/r * t)
 *   R = 1.0, r = 0.30 + drift, d = 0.78
 *
 * Distance-to-curve band creates the crown structure.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"
#include "AudioReactiveLowRiskPackHelpers.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPSpirographCrownAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_SPIROGRAPH_CROWN_AR;

    LGPSpirographCrownAREffect();
    ~LGPSpirographCrownAREffect() override = default;

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
    float m_bed         = 0.3f;
    float m_structure   = 0.5f;
    float m_impact      = 0.0f;
    float m_memory      = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
