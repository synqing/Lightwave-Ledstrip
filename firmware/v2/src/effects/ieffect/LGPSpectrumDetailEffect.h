/**
 * @file LGPSpectrumDetailEffect.h
 * @brief Detailed 64-bin FFT spectrum visualization with logarithmic mapping
 *
 * Uses the full 64-bin Goertzel spectrum (110-4186 Hz) for detailed frequency visualization.
 * Logarithmic mapping places low frequencies at center, high frequencies at edges.
 * Center-origin pattern: bass at center (LEDs 79/80), treble at edges.
 *
 * Effect ID: 93
 * Family: AUDIO_REACTIVE
 * Tags: CENTER_ORIGIN | AUDIO_SYNC | SPECTRUM
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../enhancement/SmoothingEngine.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#include <esp_heap_caps.h>
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPSpectrumDetailEffect : public plugins::IEffect {
public:
    LGPSpectrumDetailEffect() = default;
    ~LGPSpectrumDetailEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
#ifndef NATIVE_BUILD
    struct SpectrumDetailPsram {
        enhancement::AsymmetricFollower binFollowers[64];
        float binSmoothing[64];
        float targetBins[64];
    };
    SpectrumDetailPsram* m_ps = nullptr;
#else
    void* m_ps = nullptr;
#endif

    // Hop sequence tracking
    uint32_t m_lastHopSeq = 0;
    
    // Frequency-to-color mapping helper (uses palette system)
    CRGB frequencyToColor(uint8_t bin, const plugins::EffectContext& ctx) const;
    
    // Logarithmic mapping: map bin index to LED distance from center
    uint16_t binToLedDistance(uint8_t bin) const;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
