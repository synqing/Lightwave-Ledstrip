/**
 * @file LGPRule30CathedralAREffect.h
 * @brief Rule 30 Cathedral (5-Layer Audio-Reactive)
 *
 * Effect ID: 0x1C0D (EID_LGP_RULE30_CATHEDRAL_AR)
 * Family: FIVE_LAYER_AR
 * Category: QUANTUM
 * Tags: CENTER_ORIGIN | DUAL_STRIP | CELLULAR_AUTOMATON | AUDIO_REACTIVE
 *
 * 5-layer composition model (NOT flat lerp):
 *   Bed       - slow RMS-driven base brightness (tau ~0.40s)
 *   Structure - CA step rate from rhythmic + bass (tau ~0.12s)
 *   Impact    - beat-triggered CA reset/seed burst (decay ~0.22s)
 *   Tonal     - chord-driven hue anchor
 *   Memory    - textile persistence glow (tau ~0.70s)
 *
 * Composition: brightness = bed * (blur_wave * glue) + impact + memory
 *
 * Rule 30 CA: new = l ^ (c | r)
 * PSRAM storage: cells[160] + next[160]
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"
#include "AudioReactiveLowRiskPackHelpers.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPRule30CathedralAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_RULE30_CATHEDRAL_AR;

    LGPRule30CathedralAREffect();
    ~LGPRule30CathedralAREffect() override;

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

#ifndef NATIVE_BUILD
    // PSRAM storage for CA state
    struct Rule30Psram {
        uint8_t cells[160];
        uint8_t next[160];
    };
    Rule30Psram* m_ps = nullptr;
#endif

    float m_t = 0.0f;
    float m_stepAccum = 0.0f;

    // 5-Layer composition state
    float m_bed       = 0.3f;
    float m_structure = 0.5f;
    float m_impact    = 0.0f;
    float m_memory    = 0.0f;

    void seedCA();
    void stepCA();
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
