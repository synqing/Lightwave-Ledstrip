/**
 * @file LGPBeatPrismOnsetDriftEffect.h
 * @brief Outward Drift variant of LGP Beat Prism Onset.
 *
 * Replaces spring breathing with continuous one-way outward drift.
 * Centre emits energy outward — no contraction, no recoil.
 * All percussion semantics, chord/hue, palette, and brightness gating
 * are preserved from the base effect.
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

class LGPBeatPrismOnsetDriftEffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_BEAT_PRISM_ONSET_DRIFT;
    LGPBeatPrismOnsetDriftEffect() = default;
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
    float m_bgDriftPhase = 0.0f;   ///< One-way outward drift phase [0,1)
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
