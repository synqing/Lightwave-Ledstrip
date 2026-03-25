/**
 * @file LGPBeatPrismOnsetIgniteEffect.h
 * @brief Progressive Ignition variant of LGP Beat Prism Onset.
 *
 * One-way opening/activation from centre to edge. Energy ignites
 * outward with fast rise and slow relaxation. A stable spoke
 * skeleton exists underneath, progressively revealed.
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

class LGPBeatPrismOnsetIgniteEffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_BEAT_PRISM_ONSET_IGNITE;
    LGPBeatPrismOnsetIgniteEffect() = default;
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
    float m_bgActivation = 0.0f;   ///< Asymmetric activation level (fast rise, slow fall)
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
