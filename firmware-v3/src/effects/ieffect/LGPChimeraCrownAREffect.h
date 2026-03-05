/**
 * @file LGPChimeraCrownAREffect.h
 * @brief Chimera Crown (5-Layer Audio-Reactive)
 *
 * Effect ID: 0x1C0F (EID_LGP_CHIMERA_CROWN_AR)
 * Family: FIVE_LAYER_AR
 * Category: QUANTUM
 * Tags: CENTER_ORIGIN | DUAL_STRIP | PHYSICS | AUDIO_REACTIVE
 *
 * 5-layer composition model (NOT flat lerp):
 *   Bed       - RMS-driven crown brightness base (tau ~0.45s)
 *   Structure - coupling K + window W from bass + flux (tau ~0.15s)
 *   Impact    - beat-triggered crown spike (decay ~0.22s)
 *   Tonal     - chord-driven hue anchor
 *   Memory    - sync persistence accumulator (decay ~0.90s)
 *
 * Composition: brightness = bed * (crown + grain) + impact + memory
 *
 * Kuramoto oscillator physics:
 *   theta[160] — phase per LED
 *   omega[160] — intrinsic frequency (heterogeneity)
 *   Rlocal[160] — local order parameter (windowed coherence)
 *
 * Crown = smoothstep(Rlocal), Bed = sin(theta), Grain = hash-based texture
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"
#include "AudioReactiveLowRiskPackHelpers.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPChimeraCrownAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_CHIMERA_CROWN_AR;

    LGPChimeraCrownAREffect();
    ~LGPChimeraCrownAREffect() override = default;

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

    struct ChimeraPsram {
        float theta[STRIP_LENGTH];
        float omega[STRIP_LENGTH];
        float Rlocal[STRIP_LENGTH];
    };

    ChimeraPsram* m_ps = nullptr;
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
