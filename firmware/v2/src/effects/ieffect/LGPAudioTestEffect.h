/**
 * @file LGPAudioTestEffect.h
 * @brief Audio-reactive test effect demonstrating Phase 2 audio integration
 *
 * This effect validates the audio pipeline by visualizing:
 * - RMS energy (overall brightness)
 * - 8-band spectrum (bass at center, treble at edges)
 * - Beat detection (pulse on beat)
 *
 * Falls back to time-based animation when audio is unavailable.
 *
 * Effect ID: TBD (audio test)
 * Family: AUDIO_REACTIVE
 * Tags: CENTER_ORIGIN | AUDIO_SYNC
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPAudioTestEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_AUDIO_TEST;

    LGPAudioTestEffect() = default;
    ~LGPAudioTestEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Beat pulse animation state
    float m_beatDecay = 0.0f;      // Current beat pulse intensity (decays over time)
    float m_lastBeatPhase = 0.0f;  // For detecting beat crossings

    // Fallback animation state
    float m_fallbackPhase = 0.0f;  // Fake beat phase when audio unavailable
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
