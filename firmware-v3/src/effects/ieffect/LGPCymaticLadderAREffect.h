/**
 * @file LGPCymaticLadderAREffect.h
 * @brief Cymatic Ladder (5-Layer Audio-Reactive) — REWRITTEN
 *
 * Effect ID: EID_LGP_CYMATIC_LADDER_AR (0x1C04)
 * Direct ControlBus reads, single-stage smoothing, max follower normalisation.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPCymaticLadderAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_CYMATIC_LADDER_AR;

    LGPCymaticLadderAREffect();
    ~LGPCymaticLadderAREffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    float m_t = 0.0f;

    // Single-stage smoothed audio
    float m_bass       = 0.0f;
    float m_mid        = 0.0f;
    float m_chromaAngle = 0.0f;

    // Asymmetric max followers
    float m_bassMax    = 0.15f;
    float m_midMax     = 0.15f;

    // Standing wave mode with hysteresis
    float m_modeSmooth = 3.0f;

    // Impact
    float m_impact     = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
