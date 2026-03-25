/**
 * @file LGPBeatPrismOnsetRotateEffect.h
 * @brief Rotating Facets variant of LGP Beat Prism Onset.
 *
 * Spoke skeleton is nearly anchored. Motion lives in the inner
 * facet and refraction layers which rotate independently.
 * Feels like light rotating inside crystal, not structure
 * expanding/contracting.
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

class LGPBeatPrismOnsetRotateEffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_BEAT_PRISM_ONSET_ROTATE;
    LGPBeatPrismOnsetRotateEffect() = default;
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
