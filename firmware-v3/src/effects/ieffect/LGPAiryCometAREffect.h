/**
 * @file LGPAiryCometAREffect.h
 * @brief LGP Airy Comet (5-Layer AR) - Self-accelerating comet with 5-layer composition
 *
 * Effect ID: EID_LGP_AIRY_COMET_AR (0x1C03)
 * Family: FIVE_LAYER_AR
 * Tags: CENTER_ORIGIN | DUAL_STRIP | SPECTRAL | PHYSICS | 5LAYER_AR
 *
 * 5-layer audio-reactive composition over the original Airy Comet
 * parabolic physics. Bed/structure/impact/memory layers drive
 * the comet trajectory and tail coefficients directly.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"
#include "AudioReactiveLowRiskPackHelpers.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPAiryCometAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_AIRY_COMET_AR;

    LGPAiryCometAREffect();
    ~LGPAiryCometAREffect() override = default;

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
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
