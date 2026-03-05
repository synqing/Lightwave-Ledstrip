/**
 * @file LGPLorenzRibbonAREffect.h
 * @brief Lorenz Ribbon (5-Layer Audio-Reactive)
 *
 * Effect ID: 0x1C12 (EID_LGP_LORENZ_RIBBON_AR)
 * Family: FIVE_LAYER_AR
 * Category: QUANTUM
 * Tags: CENTER_ORIGIN | DUAL_STRIP | PHYSICS | AUDIO_REACTIVE
 *
 * 5-layer composition model (NOT flat lerp):
 *   Bed       - RMS-driven ribbon brightness base (tau ~0.40s)
 *   Structure - sub-steps + thickness from bass + flux (tau ~0.12s)
 *   Impact    - beat-triggered trajectory burst (decay ~0.20s)
 *   Tonal     - chaotic energy hue anchor from chord/root
 *   Memory    - tail persistence accumulator (decay ~0.85s)
 *
 * Composition: brightness = bed * ribbonGeom + impact + memory
 *
 * Lorenz attractor physics:
 *   dx/dt = sigma*(y-x), dy/dt = x*(rho-z)-y, dz/dt = x*y - beta*z
 *   96-sample trail projected radially onto 160 LEDs
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"
#include "AudioReactiveLowRiskPackHelpers.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPLorenzRibbonAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_LORENZ_RIBBON_AR;

    LGPLorenzRibbonAREffect();
    ~LGPLorenzRibbonAREffect() override = default;

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
    static constexpr int TRAIL_N = 96;

    struct LorenzPsram {
        float trail[TRAIL_N];
    };

    LorenzPsram* m_ps = nullptr;
    lowrisk_ar::Ar16Controls m_controls;
    lowrisk_ar::ArRuntimeState m_ar;

    float m_x = 1.0f;
    float m_y = 0.0f;
    float m_z = 0.0f;
    float m_t = 0.0f;
    uint8_t m_head = 0;

    // 5-Layer composition state
    float m_bed       = 0.3f;
    float m_structure = 0.5f;
    float m_impact    = 0.0f;
    float m_memory    = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
