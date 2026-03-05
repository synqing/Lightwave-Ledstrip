/**
 * @file LGPLangtonHighwayAREffect.h
 * @brief Langton Highway (5-Layer Audio-Reactive)
 *
 * Effect ID: 0x1C0E (EID_LGP_LANGTON_HIGHWAY_AR)
 * Family: FIVE_LAYER_AR
 * Category: QUANTUM
 * Tags: CENTER_ORIGIN | DUAL_STRIP | CELLULAR_AUTOMATON | AUDIO_REACTIVE
 *
 * 5-layer composition model (NOT flat lerp):
 *   Bed       - slow RMS-driven atmosphere (tau ~0.45s)
 *   Structure - ant step rate from rhythmic+rms (tau ~0.10s)
 *   Impact    - beat-triggered ant spark amplification (decay ~0.20s)
 *   Tonal     - chord-driven highway hue
 *   Memory    - highway persistence glow accumulator (decay ~0.85s)
 *
 * Composition: brightness = bed * highwayField + antSpark * impact + memory
 *
 * Langton's ant on 64x64 grid, projected to 1D via drifting diagonal slice.
 * Neighbourhood blur + centre glue. PSRAM-allocated grid.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"
#include "AudioReactiveLowRiskPackHelpers.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPLangtonHighwayAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_LANGTON_HIGHWAY_AR;

    LGPLangtonHighwayAREffect();
    ~LGPLangtonHighwayAREffect() override;

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

    // 5-Layer composition state
    float m_bed         = 0.3f;
    float m_structure   = 0.5f;
    float m_impact      = 0.0f;
    float m_memory      = 0.0f;

    // Langton's ant state
    static constexpr uint8_t W = 64;
    static constexpr uint8_t H = 64;
    uint8_t* m_grid = nullptr; // PSRAM allocation (W*H bytes)

    int8_t m_antX = 32;
    int8_t m_antY = 32;
    int8_t m_antDir = 0; // 0=N, 1=E, 2=S, 3=W

    float m_antStepAccum = 0.0f;
    float m_sliceOffset = 0.0f;

    void stepAnt();
    float sampleProjection(float offset);
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
