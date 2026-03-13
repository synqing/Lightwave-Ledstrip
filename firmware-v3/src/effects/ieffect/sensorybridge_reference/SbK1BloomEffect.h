/**
 * @file SbK1BloomEffect.h
 * @brief K1.Lightwave Bloom mode ported to Ledstrip firmware-v3 IEffect
 *
 * This is a parity port of K1's `light_mode_bloom()` (lightshow_modes.h:502-647).
 * It produces a centre-origin scrolling trail driven by chromagram colour synthesis
 * with sub-pixel interpolation sprite blitting.
 *
 * Key algorithm steps:
 * 1. Clear working buffer
 * 2. Blit previous frame shifted outward (sub-pixel interpolation) with decay
 * 3. Synthesize colour from 12-bin chromagram (cyan-offset hue, Bloom-specific)
 * 4. Force saturation, apply photons brightness
 * 5. Insert colour at centre pair (LEDs 79/80)
 * 6. Save frame for next iteration
 * 7. Edge fade (quadratic, outer quarter)
 * 8. Mirror right half to left half
 * 9. Output to ctx.leds (both strips)
 *
 * Derives from SbK1BaseEffect for shared chromagram/colour-shift pipeline.
 */

#pragma once

#include "SbK1BaseEffect.h"
#include "../../../plugins/api/IEffect.h"
#include "../../../plugins/api/EffectContext.h"

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#include "../../../config/effect_ids.h"
#endif

namespace lightwaveos::effects::ieffect::sensorybridge_reference {

class SbK1BloomEffect final : public SbK1BaseEffect {
public:
    static constexpr lightwaveos::EffectId kId = 0x1301; // EID_SB_K1_BLOOM
    SbK1BloomEffect() = default;
    ~SbK1BloomEffect() override = default;

    const plugins::EffectMetadata& getMetadata() const override;

    // Parameter interface
    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

protected:
    void renderEffect(plugins::EffectContext& ctx) override;

    /// Called by base init/cleanup via lifecycle
    bool init(plugins::EffectContext& ctx) override;
    void cleanup() override;

private:
    // ---------------------------------------------------------------
    // Constants
    // ---------------------------------------------------------------
    static constexpr uint16_t kStripLen    = 160;
    static constexpr uint16_t kHalf        = 80;
    static constexpr uint16_t kCenterLeft  = 79;
    static constexpr uint16_t kCenterRight = 80;
    static constexpr uint16_t kEdgeFade    = 40;  // Outer quarter each end

    // Incandescent warm-white filter coefficients
    static constexpr float kIncanR = 1.0000f;
    static constexpr float kIncanG = 0.4453f;
    static constexpr float kIncanB = 0.1562f;

    // ---------------------------------------------------------------
    // Parameters
    // ---------------------------------------------------------------
    static constexpr uint8_t kParamCount = 7;

    float m_mood        = 0.16f;   // Scroll speed (MOOD knob)
    float m_contrast    = 1.0f;    // Chromagram contrast (SQUARE_ITER)
    float m_saturation  = 1.0f;    // Colour saturation
    float m_chromaHue   = 0.0f;    // Manual hue offset (CHROMA knob)
    float m_incandescent = 0.0f;   // Warm-white filter blend
    float m_prismCount  = 1.42f;   // Prism layers (scale→mirror→additive blend)
    float m_bulbOpacity = 0.0f;    // Discrete bulb cover pattern opacity
    float m_scrollAccum = 0.0f;  // Sub-pixel scroll accumulator

    // Parameter descriptors (defined in .cpp)
    static const plugins::EffectParameter s_params[kParamCount];

    // ---------------------------------------------------------------
    // PSRAM allocation (large buffers MUST NOT live in DRAM)
    // ---------------------------------------------------------------
    struct SbK1BloomPsram {
        CRGB_F workBuffer[kStripLen];  // Current frame working buffer
        CRGB_F prevBuffer[kStripLen];  // Previous frame for sprite scroll
        CRGB_F prismFxBuf[kStripLen];  // Prism effect scratch buffer
        CRGB_F prismTmpBuf[kStripLen]; // Prism transform temp buffer
    };

#ifndef NATIVE_BUILD
    SbK1BloomPsram* m_bloom = nullptr;
#else
    void* m_bloom = nullptr;
#endif

    // ---------------------------------------------------------------
    // Private methods
    // ---------------------------------------------------------------

    /// Sub-pixel additive sprite blit (K1 draw_sprite parity)
    static void drawSprite(CRGB_F* dest, const CRGB_F* sprite,
                           int destLen, int spriteLen,
                           float position, float alpha);

    /// Prism transform: scale to half → shift up → mirror downwards
    static void prismTransform(CRGB_F* buf, CRGB_F* tmp);
};

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
