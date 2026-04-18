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
    static constexpr uint8_t kMaxZones = 4;
    static constexpr uint16_t STRIP_LENGTH = 160;

    // PSRAM-ALLOCATED -- large buffers MUST NOT live in DRAM
#ifndef NATIVE_BUILD
    struct PsramData {
        float u[kMaxZones][STRIP_LENGTH];
        float v[kMaxZones][STRIP_LENGTH];
        float u2[kMaxZones][STRIP_LENGTH];
        float v2[kMaxZones][STRIP_LENGTH];
    };
    PsramData* m_ps = nullptr;
#else
    float m_u[kMaxZones][STRIP_LENGTH];
    float m_v[kMaxZones][STRIP_LENGTH];
    float m_u2[kMaxZones][STRIP_LENGTH];
    float m_v2[kMaxZones][STRIP_LENGTH];
#endif

    // Single-stage smoothed audio
    float m_bass[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_treble[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_chromaAngle[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};

    // Asymmetric max followers
    float m_bassMax[kMaxZones] = {0.15f, 0.15f, 0.15f, 0.15f};
    float m_trebleMax[kMaxZones] = {0.15f, 0.15f, 0.15f, 0.15f};

    // Impact
    float m_impact[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};

    // Gray-Scott parameters (modulated by audio)
    float m_F[kMaxZones] = {0.0380f, 0.0380f, 0.0380f, 0.0380f};
    float m_K[kMaxZones] = {0.0630f, 0.0630f, 0.0630f, 0.0630f};
    float m_meltK[kMaxZones] = {0.0018f, 0.0018f, 0.0018f, 0.0018f};
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
