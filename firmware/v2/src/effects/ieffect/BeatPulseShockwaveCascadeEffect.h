/**
 * @file BeatPulseShockwaveCascadeEffect.h
 * @brief Beat Pulse (Shockwave Cascade) - Outward expansion with echo rings
 *
 * A primary ring expands centre→edge, followed by two fainter echo rings
 * trailing behind at fixed offsets. Creates the illusion of a pressure wave
 * with reverberant ripples — like dropping a stone into still water.
 *
 * Core maths:
 *  1. On beat: m_beatIntensity = 1.0, record timestamp
 *  2. Primary ring: travels centre→edge in 400ms, width 0.10
 *  3. Echo 1: same travel speed, offset -0.12 behind primary, gain 0.45
 *  4. Echo 2: same travel speed, offset -0.24 behind primary, gain 0.25
 *  5. Envelope: exp(-ageMs / 320ms)
 *  6. intensity = max(primary, echo1, echo2) * envelope
 *  7. Colour: palette by distance, brightness 0.05 + intensity * 0.95
 *  8. White mix: primary_hit * 0.35
 *  9. No trail state.
 *
 * Effect ID: 116
 */

#pragma once

#include "../CoreEffects.h"
#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos::effects::ieffect {

class BeatPulseShockwaveCascadeEffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_BEAT_PULSE_SHOCKWAVE_CASCADE;
    BeatPulseShockwaveCascadeEffect() = default;
    ~BeatPulseShockwaveCascadeEffect() override = default;

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
