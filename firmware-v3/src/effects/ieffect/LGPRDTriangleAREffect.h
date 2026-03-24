/**
 * @file LGPRDTriangleAREffect.h
 * @brief LGP Reaction Diffusion Triangle (5-Layer AR) -- REWRITTEN
 *
 * Effect ID: 0x1C07 (EID_LGP_RD_TRIANGLE_AR)
 * Direct ControlBus reads, single-stage smoothing, max follower normalisation.
 * PSRAM-backed Gray-Scott reaction-diffusion buffers (4x160 floats).
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

class LGPRDTriangleAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_RD_TRIANGLE_AR;

    LGPRDTriangleAREffect();
    ~LGPRDTriangleAREffect() override = default;

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

    // PSRAM-ALLOCATED -- large buffers MUST NOT live in DRAM
    struct PsramData {
        float u[STRIP_LENGTH];
        float v[STRIP_LENGTH];
        float u2[STRIP_LENGTH];
        float v2[STRIP_LENGTH];
    };
    PsramData* m_ps = nullptr;

    // Single-stage smoothed audio
    float m_bass       = 0.0f;
    float m_treble     = 0.0f;
    float m_chromaAngle = 0.0f;

    // Asymmetric max followers
    float m_bassMax    = 0.15f;
    float m_trebleMax  = 0.15f;

    // Impact
    float m_impact     = 0.0f;

    // Gray-Scott parameters (modulated by audio)
    float m_F     = 0.0380f;
    float m_K     = 0.0630f;
    float m_meltK = 0.0018f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
