/**
 * @file LGPBeatPrismOnsetAdvectEffect.h
 * @brief Pressure Advection variant of LGP Beat Prism Onset.
 *
 * Stable crystal field locally pushed by travelling kick pressure.
 * The field is mostly anchored; the kick front locally displaces
 * the sampling position. Calm between hits, visibly deformed during
 * kick travel.
 *
 * Centre-origin, no heap in render(), British English.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPBeatPrismOnsetAdvectEffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_BEAT_PRISM_ONSET_ADVECT;
    LGPBeatPrismOnsetAdvectEffect() = default;
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_phase = 0.0f;
    float m_prism = 0.0f;
    float m_kickPulse = 0.0f;
    float m_snareBurst = 0.0f;
    float m_hihatShimmer = 0.0f;
    float m_hue = 24.0f;
    float m_audioPresence = 0.0f;
    bool  m_chordGateOpen = false;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
