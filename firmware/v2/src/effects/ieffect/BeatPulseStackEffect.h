/**
 * @file BeatPulseStackEffect.h
 * @brief Beat Pulse (Stack) - HTML PARITY implementation
 *
 * VISUAL IDENTITY:
 * Single ring contracting inward (edge to centre) with AMPLITUDE-DRIVEN motion.
 * Clean. Definitive. The kick drum. HTML parity locked.
 *
 * HTML PARITY (LOCKED):
 * - beatIntensity slams to 1.0 on beat, decays *= 0.94^(dt*60)
 * - ringCentre = beatIntensity * 0.6 (amplitude-driven, not time-driven)
 * - Triangle profile: waveHit = 1 - min(1, abs(dist - ringCentre) * 3)
 * - intensity = max(0, waveHit) * beatIntensity
 * - brightness = 0.5 + intensity * 0.5
 * - whiteMix = intensity * 0.3
 *
 * Notes:
 * - Uses real beat ticks when audio is available
 * - Falls back to 128 BPM metronome otherwise
 *
 * Effect ID: 110
 */

#pragma once

#include "../CoreEffects.h"
#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "BeatPulseCore.h"

namespace lightwaveos::effects::ieffect {

class BeatPulseStackEffect final : public plugins::IEffect {
public:
    BeatPulseStackEffect() = default;
    ~BeatPulseStackEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    BeatPulseCore::State m_state{};
};

} // namespace lightwaveos::effects::ieffect
