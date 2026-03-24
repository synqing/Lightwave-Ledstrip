/**
 * @file LGPLorenzRibbonAREffect.h
 * @brief Lorenz Ribbon (5-Layer AR) — REWRITTEN
 *
 * Effect ID: 0x1C12 (EID_LGP_LORENZ_RIBBON_AR)
 * Direct ControlBus reads, single-stage smoothing, max follower normalisation.
 *
 * Lorenz attractor physics:
 *   dx/dt = sigma*(y-x), dy/dt = x*(rho-z)-y, dz/dt = x*y - beta*z
 *   96-sample trail projected radially onto 160 LEDs
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

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

    float m_x = 1.0f;
    float m_y = 0.0f;
    float m_z = 0.0f;
    float m_t = 0.0f;
    uint8_t m_head = 0;

    // Single-stage smoothed audio
    float m_bass       = 0.0f;
    float m_treble     = 0.0f;
    float m_chromaAngle = 0.0f;

    // Asymmetric max followers
    float m_bassMax    = 0.15f;
    float m_trebleMax  = 0.15f;

    // Impact
    float m_impact     = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
