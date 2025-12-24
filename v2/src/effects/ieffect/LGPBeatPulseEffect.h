/**
 * @file LGPBeatPulseEffect.h
 * @brief Beat-synchronized radial pulse from CENTER ORIGIN
 *
 * Pulse expands outward on each beat, intensity modulated by bass.
 * Falls back to 120 BPM when audio unavailable.
 *
 * Effect ID: 69 (audio demo)
 * Family: AUDIO_REACTIVE
 * Tags: CENTER_ORIGIN | AUDIO_SYNC | BEAT
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPBeatPulseEffect : public plugins::IEffect {
public:
    LGPBeatPulseEffect() = default;
    ~LGPBeatPulseEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_pulsePosition = 0.0f;   // Current pulse distance from center (0-1)
    float m_pulseIntensity = 0.0f;  // Decaying pulse brightness
    float m_fallbackPhase = 0.0f;   // For non-audio fallback
    float m_lastBeatPhase = 0.0f;   // For detecting beat crossings
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
