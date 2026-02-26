/**
 * @file LGPReactionDiffusionEffect.h
 * @brief LGP Reaction Diffusion - 1D Gray-Scott slime
 *
 * Effect ID: 134
 * Family: NOVEL_PHYSICS
 * Tags: CENTER_ORIGIN | DUAL_STRIP | PHYSICS
 *
 * Instance State:
 * - m_u/m_v: Reaction-diffusion fields
 * - m_u2/m_v2: Next-step buffers
 * - m_t: Time accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#include "../../config/effect_ids.h"
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPReactionDiffusionEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_REACTION_DIFFUSION;

    LGPReactionDiffusionEffect();
    ~LGPReactionDiffusionEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;


    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;
private:
    static constexpr uint16_t STRIP_LENGTH = 160;  // From CoreEffects.h

    // ⚠️ PSRAM-ALLOCATED — large buffers MUST NOT live in DRAM (see MEMORY_ALLOCATION.md)
    struct PsramData {
        float u[STRIP_LENGTH];
        float v[STRIP_LENGTH];
        float u2[STRIP_LENGTH];
        float v2[STRIP_LENGTH];
    };
    PsramData* m_ps = nullptr;

    float m_t;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
