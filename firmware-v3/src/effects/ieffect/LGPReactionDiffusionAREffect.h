/**
 * @file LGPReactionDiffusionAREffect.h
 * @brief Reaction Diffusion (5-Layer Audio-Reactive) — REWRITTEN
 *
 * Effect ID: 0x1C06 (EID_LGP_REACTION_DIFFUSION_AR)
 * Direct ControlBus reads, single-stage smoothing, max follower normalisation.
 * PSRAM-allocated Gray-Scott simulation buffers preserved.
 * Simulation generates spatial pattern; audio drives brightness directly.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPReactionDiffusionAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_REACTION_DIFFUSION_AR;

    LGPReactionDiffusionAREffect();
    ~LGPReactionDiffusionAREffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    static constexpr uint16_t kStripLen = 160;

    // PSRAM-allocated Gray-Scott simulation buffers
    struct PsramData {
        float u[kStripLen];
        float v[kStripLen];
        float u2[kStripLen];
        float v2[kStripLen];
    };
    PsramData* m_ps = nullptr;

    float m_t = 0.0f;

    float m_bass       = 0.0f;
    float m_chromaAngle = 0.0f;

    float m_bassMax    = 0.15f;

    float m_impact     = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
