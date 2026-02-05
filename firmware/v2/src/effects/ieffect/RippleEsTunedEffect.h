/**
 * @file RippleEsTunedEffect.h
 * @brief Ripple (ES tuned) - beat-locked, ES FFT/flux driven ripples
 *
 * Effect ID: 106
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN | TRAVELING
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../CoreEffects.h"
#include <FastLED.h>

namespace lightwaveos::effects::ieffect {

class RippleEsTunedEffect : public plugins::IEffect {
public:
    RippleEsTunedEffect();
    ~RippleEsTunedEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    struct Ripple {
        float radius = 0.0f;
        float speed = 0.0f;
        uint8_t hue = 0;
        uint8_t intensity = 0;
        bool active = false;
    };

    static constexpr uint8_t MAX_RIPPLES = 6;
    Ripple m_ripples[MAX_RIPPLES];

    uint32_t m_lastHopSeq = 0;
    uint8_t m_spawnCooldown = 0;

    // Radial LED history buffer (centre-out)
    CRGB m_radial[HALF_LENGTH];
    CRGB m_radialAux[HALF_LENGTH];

    // Audio-derived envelopes (updated on hop)
    float m_subBass = 0.0f;        // 0..1 (bins 0-5)
    float m_treble = 0.0f;         // 0..1 (bins 48-63)
    float m_fluxEnv = 0.0f;        // 0..1 transient envelope
    uint8_t m_baseHue = 0;

    void spawnRipple(uint8_t hue, uint8_t intensity, float speed);
};

} // namespace lightwaveos::effects::ieffect

