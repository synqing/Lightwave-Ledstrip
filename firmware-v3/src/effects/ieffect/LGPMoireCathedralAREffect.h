/**
 * @file LGPMoireCathedralAREffect.h
 * @brief Moire Cathedral (5-Layer Audio-Reactive)
 *
 * Effect ID: 0x1C08 (EID_LGP_MOIRE_CATHEDRAL_AR)
 * Family: FIVE_LAYER_AR
 * Category: QUANTUM
 * Tags: CENTER_ORIGIN | DUAL_STRIP | PHYSICS | AUDIO_REACTIVE
 *
 * 5-layer composition model (NOT flat lerp):
 *   Bed       - slow RMS-driven atmosphere (tau ~0.40s)
 *   Structure - grating pitch modulation from bass+mid (tau ~0.14s)
 *   Impact    - beat arch brightening (decay ~0.20s)
 *   Memory    - arch persistence accumulator (decay ~0.80s)
 *   Tonal     - chord-driven hue anchor
 *
 * Composition: brightness = bed * (moire_ribs * glue) + impact + memory
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"
#include "AudioReactiveLowRiskPackHelpers.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPMoireCathedralAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_MOIRE_CATHEDRAL_AR;

    LGPMoireCathedralAREffect();
    ~LGPMoireCathedralAREffect() override = default;

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
    float m_structure = 0.5f;
    float m_impact    = 0.0f;
    float m_memory    = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
