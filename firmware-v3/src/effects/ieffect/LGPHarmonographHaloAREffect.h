/**
 * @file LGPHarmonographHaloAREffect.h
 * @brief Harmonograph Halo (5-Layer Audio-Reactive)
 *
 * Effect ID: 0x1C0C (EID_LGP_HARMONOGRAPH_HALO_AR)
 * Family: FIVE_LAYER_AR
 * Category: QUANTUM
 * Tags: CENTER_ORIGIN | DUAL_STRIP | PHYSICS | AUDIO_REACTIVE
 *
 * 5-layer composition model (NOT flat lerp):
 *   Bed       - slow RMS-driven atmosphere (tau ~0.40s)
 *   Structure - Lissajous freq ratios a/b/delta from harmonic + mid (tau ~0.20s)
 *   Impact    - beat-triggered orbital flash (decay ~0.20s)
 *   Tonal     - chord-driven jewel hue anchor
 *   Memory    - post-impact halo persistence (decay ~0.75s)
 *
 * Composition: brightness = bed * distanceBand + impact * orbitalFlash + memory
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"
#include "AudioReactiveLowRiskPackHelpers.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPHarmonographHaloAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_HARMONOGRAPH_HALO_AR;

    LGPHarmonographHaloAREffect();
    ~LGPHarmonographHaloAREffect() override = default;

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
    float m_freqA       = 3.0f;  // Structure: Lissajous x frequency ratio
    float m_freqB       = 2.5f;  // Structure: Lissajous y frequency ratio
    float m_delta       = 0.0f;  // Structure: phase offset (drifts)
    float m_impact      = 0.0f;  // Impact: beat-triggered orbital flash
    float m_memory      = 0.0f;  // Memory: halo persistence accumulator
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
