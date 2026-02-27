/**
 * @file AudioBloomEffect.h
 * @brief Sensory Bridge-style scrolling bloom effect
 *
 * Computes colour from chromagram, shifts radial buffer outward, and applies
 * logarithmic distortion, fade, and saturation boost. Centre-origin push-outwards.
 *
 * Effect ID: 73
 * Family: PARTY
 * Tags: CENTER_ORIGIN | AUDIO_SYNC
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

// HALF_LENGTH constant (80 LEDs per half strip, matches CoreEffects.h)
constexpr uint16_t HALF_LENGTH = 80;

class AudioBloomEffect : public plugins::IEffect {
public:
    AudioBloomEffect() = default;
    ~AudioBloomEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Radial buffer (index 0 = centre, grows outward to HALF_LENGTH-1)
    CRGB m_radial[HALF_LENGTH];
    CRGB m_radialAux[HALF_LENGTH];  // Aux buffer for alternate frames
    CRGB m_radialTemp[HALF_LENGTH];  // Temp buffer for post-processing
    
    uint32_t m_iter = 0;  // Frame counter for alternate frame logic
    uint32_t m_lastHopSeq = 0;  // Track hop sequence for updates
    float m_scrollPhase = 0.0f;  // Fractional scroll accumulator
    float m_subBassPulse = 0.0f;  // 64-bin sub-bass energy for center pulse

    // Helper functions matching Sensory Bridge post-processing
    void distortLogarithmic(CRGB* src, CRGB* dst, uint16_t len);
    void fadeTopHalf(CRGB* buffer, uint16_t len);
    void increaseSaturation(CRGB* buffer, uint16_t len, uint8_t amount);
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

