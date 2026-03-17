/**
 * @file SbSpectralEnvelopeEffect.h
 * @brief SB Spectral Envelope — sparse anchor dots with outward scroll
 *
 * Contract: "When bass hits, the centre glows. When treble hits, the edges glow."
 *
 * Writes ONLY 8 sparse anchor dots (one per octave band) into a persistent
 * trail buffer. Audio-coupled decay + outward scroll create temporal history
 * visible as trails. Bass anchors sit near the centre, treble anchors at
 * the edges (centre-origin). Mirror + copy to both strips.
 *
 * Pipeline per frame:
 *   1. Decay trail buffer (audio-coupled)
 *   2. Scroll trail outward (~60 px/s)
 *   3. Write 8 sparse anchor dots from bands[0..7]
 *   4. Mirror right half → left half
 *   5. Output trail buffer to LEDs + copy strip 2
 */

#pragma once

#include "SbK1BaseEffect.h"
#include "../../CoreEffects.h"
#include "../../../plugins/api/IEffect.h"
#include "../../../plugins/api/EffectContext.h"

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#endif

namespace lightwaveos::effects::ieffect::sensorybridge_reference {

class SbSpectralEnvelopeEffect final : public SbK1BaseEffect {
public:
    static constexpr lightwaveos::EffectId kId = 0x130E;
    SbSpectralEnvelopeEffect() = default;
    ~SbSpectralEnvelopeEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

protected:
    void renderEffect(plugins::EffectContext& ctx) override;

    // Parameter interface
    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    static constexpr uint16_t kHalfLength   = lightwaveos::effects::HALF_LENGTH;    // 80
    static constexpr uint16_t kStripLength  = lightwaveos::effects::STRIP_LENGTH;   // 160
    static constexpr uint16_t kCenterLeft   = lightwaveos::effects::CENTER_LEFT;    // 79
    static constexpr uint16_t kCenterRight  = lightwaveos::effects::CENTER_RIGHT;   // 80

    /// Number of octave band anchor points
    static constexpr uint8_t kBandCount = 8;

    /// Pixel offsets of each band anchor within the right half (centre-origin).
    /// Band 0 (sub-bass) at centre, band 7 (air) at edge.
    static constexpr uint8_t kAnchors[kBandCount] = {
        0, 10, 20, 30, 40, 50, 60, 79
    };

    /// Outward scroll rate in pixels per second
    static constexpr float kScrollRate = 200.0f;

    // Effect parameters
    float m_contrast    = 1.0f;
    float m_chromaHue   = 0.0f;

    static constexpr uint8_t kParamCount = 2;
    static const plugins::EffectParameter s_params[kParamCount];

    // PSRAM-allocated trail buffer for frame-to-frame persistence
    struct SbSpecEnvPsram {
        CRGB trailBuffer[160];   ///< Persistent pixel buffer with decay + scroll
        float scrollAccum;       ///< Sub-pixel scroll accumulator
    };

#ifndef NATIVE_BUILD
    SbSpecEnvPsram* m_ps = nullptr;
#else
    SbSpecEnvPsram* m_ps = nullptr;
#endif
};

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
