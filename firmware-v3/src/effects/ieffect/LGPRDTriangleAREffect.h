/**
 * @file LGPRDTriangleAREffect.h
 * @brief LGP Reaction Diffusion Triangle (5-Layer Audio-Reactive)
 *
 * Effect ID: 0x1C07 (EID_LGP_RD_TRIANGLE_AR)
 * Family: FIVE_LAYER_AR
 * Category: QUANTUM
 * Tags: CENTER_ORIGIN | DUAL_STRIP | PHYSICS | AUDIO_REACTIVE
 *
 * 5-layer composition model (NOT flat lerp):
 *   Bed       - RMS-driven base atmosphere (tau ~0.45s)
 *   Structure - F/K/meltK modulation from bass + mid + flux (tau ~0.18s)
 *   Impact    - beat V-spike at centre (decay ~0.20s)
 *   Memory    - accumulated glow (decay ~0.85s)
 *   Tonal     - chord-driven hue anchor
 *
 * Composition: brightness = bed * (v * melt) + impact + memory
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

class LGPRDTriangleAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_RD_TRIANGLE_AR;

    LGPRDTriangleAREffect();
    ~LGPRDTriangleAREffect() override = default;

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

    // ⚠️ PSRAM-ALLOCATED — large buffers MUST NOT live in DRAM
    struct PsramData {
        float u[STRIP_LENGTH];
        float v[STRIP_LENGTH];
        float u2[STRIP_LENGTH];
        float v2[STRIP_LENGTH];
    };
    PsramData* m_ps = nullptr;

    lowrisk_ar::Ar16Controls m_controls;
    lowrisk_ar::ArRuntimeState m_ar;

    // 5-Layer composition state
    float m_bed         = 0.3f;
    float m_structure   = 0.5f;
    float m_impact      = 0.0f;
    float m_memory      = 0.0f;

    // Gray-Scott parameters (modulated by structure layer)
    float m_F     = 0.0380f;
    float m_K     = 0.0630f;
    float m_meltK = 0.0018f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
