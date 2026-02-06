/**
 * @file BeatPulseStackEffect.h
 * @brief Beat Pulse (Stack) - UI preview parity pulse (static palette gradient + white push)
 *
 * This effect implements the core Beat Pulse maths used in:
 * `docs/ui-mockups/components/v2/led-preview-stack.html`.
 *
 * Key traits:
 * - Centre-origin, symmetric output (uses SET_CENTER_PAIR)
 * - Static palette gradient (distance → palette index)
 * - Beat-driven brightness boost + “white push” (specular punch)
 * - Exponential-ish decay with dt-correct behaviour (frame-rate independent)
 *
 * Notes:
 * - The HTML demo simulates beats at a fixed BPM; this effect uses real beat
 *   ticks when audio is available, and falls back to a metronome otherwise.
 *
 * Effect ID: 110
 */

#pragma once

#include "../CoreEffects.h"
#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

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
    float m_beatIntensity = 0.0f;      // 0..1
    uint32_t m_lastBeatTimeMs = 0;     // For fallback metronome
    float m_fallbackBpm = 128.0f;

    // “Glowing stack” aesthetic control (0..1). Higher = brighter base + more white push.
    float m_stackGlow = 0.75f;

    // Trails state in centre-distance space (HALF_LENGTH entries).
    float m_trail[HALF_LENGTH] = {0.0f};
};

} // namespace lightwaveos::effects::ieffect
