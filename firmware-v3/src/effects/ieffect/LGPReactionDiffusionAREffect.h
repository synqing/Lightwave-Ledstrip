/**
 * @file LGPReactionDiffusionAREffect.h
 * @brief Reaction Diffusion (5-Layer Audio-Reactive)
 *
 * Effect ID: 0x1C06 (EID_LGP_REACTION_DIFFUSION_AR)
 * Family: FIVE_LAYER_AR
 * Category: QUANTUM
 * Tags: CENTER_ORIGIN | DUAL_STRIP | PHYSICS | AUDIO_REACTIVE
 *
 * 5-layer composition model (NOT flat lerp):
 *   Bed       - Gray-Scott reaction brightness driven by RMS (tau ~0.45s)
 *   Structure - F/K parameter modulation from bass+flux (tau ~0.15s)
 *   Impact    - beat triggers V-concentration spike at centre (decay ~0.22s)
 *   Tonal     - chord-driven hue anchor replacing ctx.gHue
 *   Memory    - accumulated glow from reaction activity (tau ~0.90s)
 *
 * Composition: brightness = bed * (v_concentration * melt_glue) + impact + memory
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"
#include "AudioReactiveLowRiskPackHelpers.h"

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPReactionDiffusionAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_REACTION_DIFFUSION_AR;

    LGPReactionDiffusionAREffect();
    ~LGPReactionDiffusionAREffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    static constexpr uint16_t STRIP_LENGTH = 160;

    // PSRAM-allocated buffers for Gray-Scott simulation
    struct PsramData {
        float u[STRIP_LENGTH];   // U concentration
        float v[STRIP_LENGTH];   // V concentration
        float u2[STRIP_LENGTH];  // Next-step U
        float v2[STRIP_LENGTH];  // Next-step V
    };
    PsramData* m_ps = nullptr;

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
