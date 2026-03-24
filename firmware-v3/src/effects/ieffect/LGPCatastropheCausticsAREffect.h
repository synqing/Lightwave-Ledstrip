/**
 * @file LGPCatastropheCausticsAREffect.h
 * @brief Catastrophe Caustics (5-Layer AR) -- REWRITTEN
 *
 * Effect ID: 0x1C10 (EID_LGP_CATASTROPHE_CAUSTICS_AR)
 * Direct ControlBus reads, single-stage smoothing, max follower normalisation.
 * PSRAM-backed intensity histogram buffer (160 floats).
 * Ray-envelope histogram with catastrophe optics.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPCatastropheCausticsAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_CATASTROPHE_CAUSTICS_AR;

    LGPCatastropheCausticsAREffect();
    ~LGPCatastropheCausticsAREffect() override;

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

    // Impact
    float m_impact     = 0.0f;

#ifndef NATIVE_BUILD
    // PSRAM allocation for intensity histogram
    struct CausticsPsram {
        float I[160];  // Ray-envelope intensity accumulator
    };
    CausticsPsram* m_ps = nullptr;
#else
    // NATIVE_BUILD fallback (testing only)
    float m_I[160];
#endif
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
