/**
 * @file RippleEnhancedEffect.h
 * @brief Beat-synchronized ripple effect with musical intelligence
 *
 * Enhanced version of RippleEffect with:
 * - Beat-sync ripple spawning (ripples spawn ON the beat)
 * - Downbeat emphasis (stronger ripples on beat 1)
 * - Style-adaptive response (EDM=fast, ambient=slow)
 * - Harmonic saliency for color shifts
 *
 * Effect ID: 97
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN | TRAVELING | AUDIO_SYNC
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../CoreEffects.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class RippleEnhancedEffect : public plugins::IEffect {
public:
    RippleEnhancedEffect();
    ~RippleEnhancedEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    struct Ripple {
        float radius;
        float speed;
        uint8_t hue;
        uint8_t intensity;
        bool active;
        bool isDownbeat;  // True if spawned on downbeat (stronger)
    };

    static constexpr uint8_t MAX_RIPPLES = 6;
    Ripple m_ripples[MAX_RIPPLES];

    // Beat tracking
    bool m_lastBeatState = false;
    bool m_lastDownbeatState = false;
    float m_beatPhaseAccum = 0.0f;
    uint32_t m_lastHopSeq = 0;  // Track audio hop sequence for spawn gating

    // Style-adaptive parameters (updated each frame)
    float m_styleSpeedMult = 1.0f;
    float m_styleIntensityMult = 1.0f;

    // Radial LED buffer (center-origin)
    CRGB m_radial[HALF_LENGTH];
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
