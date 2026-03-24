/**
 * @file LGPChimeraCrownAREffect.h
 * @brief Chimera Crown (5-Layer AR) — REWRITTEN
 *
 * Effect ID: 0x1C0F (EID_LGP_CHIMERA_CROWN_AR)
 * Direct ControlBus reads, single-stage smoothing, max follower normalisation.
 *
 * Kuramoto oscillator physics:
 *   theta[160] — phase per LED
 *   omega[160] — intrinsic frequency (heterogeneity)
 *   Rlocal[160] — local order parameter (windowed coherence)
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPChimeraCrownAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_CHIMERA_CROWN_AR;

    LGPChimeraCrownAREffect();
    ~LGPChimeraCrownAREffect() override = default;

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

    struct ChimeraPsram {
        float theta[STRIP_LENGTH];
        float omega[STRIP_LENGTH];
        float Rlocal[STRIP_LENGTH];
    };

    ChimeraPsram* m_ps = nullptr;
    float m_t = 0.0f;

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
