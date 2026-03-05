/**
 * @file LGPCatastropheCausticsAREffect.h
 * @brief Catastrophe Caustics (5-Layer Audio-Reactive)
 *
 * Effect ID: 0x1C10 (EID_LGP_CATASTROPHE_CAUSTICS_AR)
 * Family: FIVE_LAYER_AR
 * Category: QUANTUM
 * Tags: CENTER_ORIGIN | DUAL_STRIP | PHYSICS | AUDIO_REACTIVE
 *
 * Ray-envelope histogram with catastrophe optics. PSRAM-backed intensity buffer.
 * Lens thickness field h(x,t) with 3 sinusoidal terms + centre bias. Rays deflect
 * by dh/dx, land at X=x+z*a. Histogram accumulation + decay. Cusp detection via
 * Laplacian. Filmic brightness `waveToBr()`.
 *
 * 5-layer composition model (NOT flat lerp):
 *   Bed       - RMS-driven caustic brightness (tau ~0.42s)
 *   Structure - focus z + strength from mid+flux (tau ~0.14s)
 *   Impact    - beat cusp gain burst (decay ~0.20s)
 *   Memory    - caustic persistence/glow accumulator (tau ~0.85s)
 *   Tonal     - chord-driven hue anchor
 *
 * Composition: brightness = bed * (caustic + cusps) + impact*cusps + memory
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"
#include "AudioReactiveLowRiskPackHelpers.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPCatastropheCausticsAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_CATASTROPHE_CAUSTICS_AR;

    LGPCatastropheCausticsAREffect();
    ~LGPCatastropheCausticsAREffect() override;

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

#ifndef NATIVE_BUILD
    // PSRAM allocation for intensity histogram
    struct CausticsPsram {
        float I[160];  // Ray-envelope intensity accumulator
    };
    CausticsPsram* m_ps = nullptr;
#else
    // NATIVE_BUILD fallback (testing only)
    float m_I[160];
#endif
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
