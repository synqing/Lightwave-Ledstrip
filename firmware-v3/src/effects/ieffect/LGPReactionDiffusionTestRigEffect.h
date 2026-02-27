/**
 * @file LGPReactionDiffusionTestRigEffect.h
 * @brief Reaction Diffusion Test Rig (12 diagnostic variants)
 *
 * Purpose:
 * - Isolate whether the "triangle" is:
 *   A) a true propagating-front wedge (reaction-diffusion dynamics), or
 *   B) an envelope/render mapping artefact.
 *
 * Controls:
 * - Mode select: mode = (ctx.gHue >> 4)  [0..15], uses 0..11, 12..15 => 0
 * - Speed: still controls simulation rate (dt/iters)
 * - Brightness: master dim
 *
 * Effect ID: 135
 * Family: QUANTUM
 * Tags: CENTER_ORIGIN | DUAL_STRIP | PHYSICS
 *
 * Instance State:
 * - m_u/m_v: Reaction-diffusion fields
 * - m_u2/m_v2: Next-step buffers
 * - m_lastMode/m_frame: Diagnostics
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPReactionDiffusionTestRigEffect : public plugins::IEffect {
public:
    LGPReactionDiffusionTestRigEffect();
    ~LGPReactionDiffusionTestRigEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

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

    uint8_t m_lastMode;
    uint32_t m_frame;

    void resetForMode(uint8_t mode);
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
