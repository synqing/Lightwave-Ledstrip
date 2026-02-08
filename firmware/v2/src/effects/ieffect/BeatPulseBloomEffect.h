/**
 * @file BeatPulseBloomEffect.h
 * @brief Beat Pulse (Bloom) - Bloom-style transport (subpixel advection + persistence) with centre injection
 *
 * This is the *stateful* counterpart to the stateless HTML-parity BeatPulse effects:
 * the "liquid" look comes from moving the entire previous frame forward by a fractional amount
 * each tick (subpixel advection), then injecting new energy at the centre.
 *
 * Effect ID: 121 (requires MAX_EFFECTS bump + PatternRegistry metadata entry)
 */

#pragma once

#include "../CoreEffects.h"
#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos::effects::ieffect {

class BeatPulseBloomEffect final : public plugins::IEffect {
public:
    BeatPulseBloomEffect();
    ~BeatPulseBloomEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    plugins::EffectMetadata m_meta;

    // Per-zone envelope (ZoneComposer shares a single effect instance)
    float    m_beatEnv[4] = {0,0,0,0};     // 0..1 "beat slam" envelope
    uint32_t m_lastBeatMs[4] = {0,0,0,0};  // for fallback metronome per-zone

    // Lazy init guard (ZoneComposer might never call init on non-selected effects)
    bool m_hasEverRendered = false;
};

} // namespace lightwaveos::effects::ieffect
