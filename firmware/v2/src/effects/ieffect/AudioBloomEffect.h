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
#include <esp_heap_caps.h>
#include "../../config/effect_ids.h"
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

// HALF_LENGTH constant (80 LEDs per half strip, matches CoreEffects.h)
constexpr uint16_t HALF_LENGTH = 80;

class AudioBloomEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_AUDIO_BLOOM;

    AudioBloomEffect() = default;
    ~AudioBloomEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;


    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;
private:
#ifndef NATIVE_BUILD
    struct AudioBloomPsram {
        CRGB radial[HALF_LENGTH];
        CRGB radialAux[HALF_LENGTH];
        CRGB radialTemp[HALF_LENGTH];
    };
    AudioBloomPsram* m_ps = nullptr;
#else
    void* m_ps = nullptr;
#endif

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

