/**
 * @file SbSpectralEnvelopeEffect.h
 * @brief SB Spectral Envelope — maps 8 octave bands to pixel positions
 *
 * Maps the 8 ControlBus octave energy bands to spatial positions along the
 * LED strip with smooth Hermite interpolation. Bass at centre, treble at
 * edges (centre-origin). Colour is derived from the chromagram palette via
 * the SbK1BaseEffect base class.
 *
 * Data flow:
 *   controlBus.bands[0..7] → spatial interpolation (80 px) → smoothstep
 *     → palette colour at interpolated brightness → mirror + copy
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

    /// Number of octave band anchor points for interpolation
    static constexpr uint8_t kBandCount = 8;

    /// Pixel positions of each band anchor (right half, centre-origin)
    static constexpr float kBandPositions[kBandCount] = {
        0.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 79.0f
    };

    // Effect parameters
    float m_contrast    = 1.0f;
    float m_chromaHue   = 0.0f;

    static constexpr uint8_t kParamCount = 2;
    static const plugins::EffectParameter s_params[kParamCount];

    /// Smooth Hermite interpolation between band anchor points.
    /// Returns interpolated energy at a given pixel position [0, 79].
    static float interpolateBands(const float* bands, float pixelPos);
};

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
