/**
 * @file LGPBeatPrismOnsetEffect.h
 * @brief Onset-driven variant of LGP Beat Prism.
 *
 * Identical visual to LGP Beat Prism but driven by the band-ratio onset
 * detector (kick/snare/hihat triggers) instead of the tempo-predicted beat
 * grid.  Kick triggers the beat-front pulse, snare modulates prism refraction
 * intensity, hihat adds high-frequency shimmer to the spoke field.
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

class LGPBeatPrismOnsetEffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_BEAT_PRISM_ONSET;
    LGPBeatPrismOnsetEffect() = default;
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_phase = 0.0f;
    float m_prism = 0.0f;
    float m_kickPulse = 0.0f;      ///< Kick-driven beat front
    float m_snareBurst = 0.0f;     ///< Snare-driven refraction boost
    float m_hihatShimmer = 0.0f;   ///< Hihat-driven spoke shimmer
    float m_hue = 24.0f;
    float m_audioPresence = 0.0f;
    bool  m_chordGateOpen = false;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
