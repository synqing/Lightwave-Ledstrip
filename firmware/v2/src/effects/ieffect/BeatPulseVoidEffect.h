/**
 * @file BeatPulseVoidEffect.h
 * @brief Beat Pulse (Void) - Hard detonation in darkness, palette-coloured ring against black
 *
 * Variant of the Parity pulse (BeatPulseStackEffect) with the key difference
 * that the 0.5 base brightness "background" is completely removed. The strip
 * is black between beats; the ring itself IS the palette colours, detonating
 * out of the void and collapsing inward while fading to black.
 *
 * Core maths (differences from Parity marked):
 *  1. On beat tick: m_beatIntensity = 1.0 (same as Parity)
 *  2. Decay: *= pow(0.94, dt * 60) (same as Parity)
 *  3. Ring: centre = 0.6 * beatIntensity, sharpness = 3.0 (same)
 *  4. Brightness: 0.0 + intensity * 1.0 (NOT 0.5 + intensity * 0.5)
 *  5. White mix: intensity * 0.4 (NOT 0.3 — more punch against black)
 *  6. No trail state.
 *
 * Effect ID: 113
 */

#pragma once

#include "../CoreEffects.h"
#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos::effects::ieffect {

class BeatPulseVoidEffect final : public plugins::IEffect {
public:
    BeatPulseVoidEffect() = default;
    ~BeatPulseVoidEffect() override = default;

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
