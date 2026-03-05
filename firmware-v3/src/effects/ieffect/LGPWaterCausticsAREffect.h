/**
 * @file LGPWaterCausticsAREffect.h
 * @brief LGP Water Caustics (5-Layer AR) - Ray-envelope caustic filaments
 *
 * Effect ID: EID_LGP_WATER_CAUSTICS_AR (0x1C00)
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN | DUAL_STRIP | SPECTRAL | PHYSICS | 5LAYER_AR
 *
 * 5-layer audio-reactive composition over the original water caustics
 * refractive physics. Bed/structure/impact/snare/memory layers drive
 * the ray-envelope coefficients instead of blendAmbientReactive.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"
#include "AudioReactiveLowRiskPackHelpers.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPWaterCausticsAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_WATER_CAUSTICS_AR;

    LGPWaterCausticsAREffect();
    ~LGPWaterCausticsAREffect() override = default;

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
    float m_t1 = 0.0f;
    float m_t2 = 0.0f;

    // 5-Layer composition state
    float m_bed         = 0.3f;
    float m_structure   = 0.5f;
    float m_impact      = 0.0f;
    float m_snareImpact = 0.0f;
    float m_memory      = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
