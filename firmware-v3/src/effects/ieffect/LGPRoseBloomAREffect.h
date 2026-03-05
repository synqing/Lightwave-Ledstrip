/**
 * @file LGPRoseBloomAREffect.h
 * @brief Rose Bloom (5-Layer Audio-Reactive)
 *
 * Effect ID: 0x1C0B (EID_LGP_ROSE_BLOOM_AR)
 * Family: FIVE_LAYER_AR
 * Category: QUANTUM
 * Tags: CENTER_ORIGIN | DUAL_STRIP | MATHEMATICS | AUDIO_REACTIVE
 *
 * 5-layer composition model (NOT flat lerp):
 *   Bed       - slow RMS-driven atmosphere (tau ~0.42s)
 *   Structure - petal count from harmonic + rhythmic (tau ~0.25s)
 *   Impact    - beat-triggered petal flash (decay ~0.18s)
 *   Tonal     - chord-driven hue anchor
 *   Memory    - bloom persistence accumulator (decay ~0.80s)
 *
 * Composition: brightness = bed * bloomGeom + impact + memory
 * Rhodonea curve: r = |cos(kf * theta)| with kf drifting 3-7 (petal count)
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"
#include "AudioReactiveLowRiskPackHelpers.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPRoseBloomAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_ROSE_BLOOM_AR;

    LGPRoseBloomAREffect();
    ~LGPRoseBloomAREffect() override = default;

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
    float m_structure = 5.0f;  // petal count kf (3-7 range)
    float m_impact    = 0.0f;
    float m_memory    = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
