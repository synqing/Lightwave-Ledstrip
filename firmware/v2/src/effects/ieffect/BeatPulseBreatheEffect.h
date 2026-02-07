/**
 * @file BeatPulseBreatheEffect.h
 * @brief Beat Pulse (Breathe) - Organic whole-strip breathing pulse
 *
 * The simplest, most primal beat-reactive effect - like a heartbeat or subwoofer cone.
 * NO ring shape. The entire strip pulses with strong centre-weighting.
 * Warm, organic, living.
 *
 * Visual identity:
 *  - Whole-strip amplitude modulation (no travelling ring)
 *  - Strong centre weighting: centre = 100%, edge = 40% (60% falloff)
 *  - Slower attack and decay than other effects (organic breathing)
 *  - Colour shifts warmer on beat, cooler at rest
 *  - Higher resting brightness (warm ambient glow)
 *
 * Core maths:
 *  1. On beat: m_targetIntensity = 1.0 (soft attack target)
 *  2. Soft attack: m_beatIntensity += (target - current) * ATTACK_SMOOTHING
 *  3. Slow decay: *= pow(DECAY_FACTOR, dt * 60) — organic exhale
 *  4. Centre weighting: centreWeight = 1.0 - dist01 * 0.6
 *  5. Colour warmth shift on beat (palette index modulation)
 *  6. Resting brightness = 0.20 (warm glow baseline)
 *
 * Effect ID: 119
 */

#pragma once

#include "../CoreEffects.h"
#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos::effects::ieffect {

class BeatPulseBreatheEffect final : public plugins::IEffect {
public:
    BeatPulseBreatheEffect() = default;
    ~BeatPulseBreatheEffect() override = default;

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
    float m_targetIntensity = 0.0f;  // Soft attack target for organic feel
    uint32_t m_lastBeatTimeMs = 0;
    float m_fallbackBpm = 128.0f;
};

} // namespace lightwaveos::effects::ieffect
