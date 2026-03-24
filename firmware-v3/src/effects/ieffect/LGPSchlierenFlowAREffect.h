/**
 * @file LGPSchlierenFlowAREffect.h
 * @brief Schlieren Flow (5-Layer Audio-Reactive) — REWRITTEN
 *
 * Effect ID: EID_LGP_SCHLIEREN_FLOW_AR (0x1C01)
 * Direct ControlBus reads, single-stage smoothing, max follower normalisation.
 * Unique: independent strip colours, 3 percussion layers (beat/snare/hihat).
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPSchlierenFlowAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_SCHLIEREN_FLOW_AR;

    LGPSchlierenFlowAREffect();
    ~LGPSchlierenFlowAREffect() override = default;

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
    float m_treble     = 0.0f;
    float m_chromaAngle = 0.0f;

    // Asymmetric max followers
    float m_bassMax    = 0.15f;
    float m_trebleMax  = 0.15f;

    // 3 percussion impact layers
    float m_impact      = 0.0f;
    float m_snareImpact = 0.0f;
    float m_hihatImpact = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
