/**
 * @file BeatPulseResonantEffect.h
 * @brief Beat Pulse (Resonant) - Double ring contracting inward to centre
 *
 * Two concentric rings — a thin, bright "attack" ring and a wider, softer
 * "body" ring — both contract from the edges toward the centre on each beat.
 * The attack ring travels faster and arrives first, creating a percussive
 * snap followed by a warm resonant thud. Both rings are time-driven.
 *
 * Core maths:
 *  1. On beat: m_beatIntensity = 1.0, record timestamp
 *  2. Attack ring: travels edge→centre in 350ms, width 0.08
 *  3. Body ring: travels edge→centre in 550ms, width 0.22
 *  4. Envelope: exp(-ageMs / decayMs)
 *  5. Final intensity: max(attack * 1.0, body * 0.55) * envelope
 *  6. Colour: palette by distance, brightness 0.06 + intensity * 0.94
 *  7. White mix: attack * 0.40 (body gets no white — it's warm)
 *  8. No trail state.
 *
 * Effect ID: 114
 */

#pragma once

#include "../CoreEffects.h"
#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos::effects::ieffect {

class BeatPulseResonantEffect final : public plugins::IEffect {
public:
    BeatPulseResonantEffect() = default;
    ~BeatPulseResonantEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    float m_beatIntensity = 0.0f;
    uint32_t m_lastBeatTimeMs = 0;
    float m_fallbackBpm = 128.0f;
};

} // namespace lightwaveos::effects::ieffect
