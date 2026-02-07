/**
 * @file BeatPulseShockwaveEffect.h
 * @brief Beat Pulse (Shockwave) - HTML PARITY implementation
 *
 * VISUAL IDENTITY:
 * Single ring expanding OUTWARD from centre (or INWARD from edges) with
 * AMPLITUDE-DRIVEN motion. Same HTML core maths as Stack, different direction.
 *
 * HTML PARITY (LOCKED):
 * - beatIntensity slams to 1.0 on beat, decays *= 0.94^(dt*60)
 * - ringCentre = beatIntensity * 0.6 (amplitude-driven, not time-driven)
 * - Triangle profile: waveHit = 1 - min(1, abs(dist - ringCentre) * 3)
 * - intensity = max(0, waveHit) * beatIntensity
 * - brightness = 0.5 + intensity * 0.5
 * - whiteMix = intensity * 0.3
 *
 * Effect IDs:
 * - 111: Beat Pulse (Shockwave) [outward]
 * - 112: Beat Pulse (Shockwave In) [inward]
 */

#pragma once

#include "../CoreEffects.h"
#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos::effects::ieffect {

class BeatPulseShockwaveEffect final : public plugins::IEffect {
public:
    explicit BeatPulseShockwaveEffect(bool inward);
    ~BeatPulseShockwaveEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    bool m_inward = false;
    plugins::EffectMetadata m_meta;

    float m_beatIntensity = 0.0f;      // Amplitude-driven ring position (HTML parity)
    uint32_t m_lastBeatTimeMs = 0;     // Fallback metronome tracking
    float m_fallbackBpm = 128.0f;      // Fallback metronome BPM
};

} // namespace lightwaveos::effects::ieffect
