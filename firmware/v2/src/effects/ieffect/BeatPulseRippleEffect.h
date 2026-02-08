/**
 * @file BeatPulseRippleEffect.h
 * @brief Beat Pulse (Ripple) - 3-slot ring buffer of cascading implosion rings
 *
 * Each beat spawns a new ring that contracts inward from edge to centre.
 * Up to 3 rings can be alive simultaneously (a 3-slot ring buffer), so
 * rapid beats produce cascading concentric ripples converging on the centre.
 * Each ring has its own birth timestamp and decays independently.
 *
 * Core maths:
 *  1. On beat: push new ring into 3-slot circular buffer
 *  2. Each ring: pos = 1.0 - age/travelMs, envelope = exp(-age/decayMs)
 *  3. Ring shape: tent, width 0.10
 *  4. Final pixel = max across all 3 rings
 *  5. Colour: palette by distance, brightness = 0.04 + intensity * 0.96
 *  6. White mix: intensity * 0.30
 *  7. No trail state (the ring buffer IS the memory).
 *
 * Effect ID: 115
 */

#pragma once

#include "../CoreEffects.h"
#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos::effects::ieffect {

class BeatPulseRippleEffect final : public plugins::IEffect {
public:
    BeatPulseRippleEffect() = default;
    ~BeatPulseRippleEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    static constexpr uint8_t MAX_RINGS = 3;

    struct RingSlot {
        uint32_t birthMs = 0;
        bool active = false;
    };

    RingSlot m_rings[MAX_RINGS] = {};
    uint8_t m_nextSlot = 0;
    float m_fallbackBpm = 128.0f;
    uint32_t m_lastFallbackBeatMs = 0;
};

} // namespace lightwaveos::effects::ieffect
