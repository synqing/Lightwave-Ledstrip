/**
 * @file SbK1BloomEffect.h
 * @brief Canonical SB 4.1.1 light_mode_bloom port — Phase 5B PoC.
 *
 * Verbatim port of canonical Sensory Bridge 4.1.1 `light_mode_bloom()`
 * (lightshow_modes.h:398-499). Operates entirely in CRGB_F for sub-byte
 * trail precision (uint8 CRGB truncates sub-1.0 propagation values to
 * zero, killing trails after ~4 LEDs — proven by failed Phase 5 PoC).
 *
 * Per-frame algorithm (canonical SB):
 *   1. Clear working buffer (CRGB_F, float precision)
 *   2. Sub-pixel scroll prevBuf → workBuf rightward via drawSprite()
 *      (V1's existing CRGB_F port of SB led_utilities.h:1247-1290)
 *   3a. Chroma input peak normalisation (PoC modification — input scale
 *       repair for K1 ESV11 raw chroma vs SB's max_peak-normalised input)
 *   3b. Synthesize colour: Σ palette[i/12 + 0.5] × bin² × 1/6 (canonical SB)
 *   4. Clip per channel at 1.0 (canonical SB; NOT totalMag normalisation)
 *   5. SQUARE_ITER post-sum squarings (canonical SB iterative gain)
 *   6. force_saturation via HSV roundtrip
 *   7. force_hue if non-chromatic mode
 *   8. Direct injection at centre pair LEDs 79+80 (canonical SB; NO EMA)
 *   9. Snapshot full workBuf → prevBuf BEFORE edge fade (canonical SB)
 *   10. Quadratic edge fade outer 40 LEDs of right half (canonical SB,
 *       50% of right half scaled to K1 geometry)
 *   11. Mirror right half (80..159) → left half (79..0)
 *   12. K1 post-processing: prism, bulb cover, incandescent filter
 *   13. Output to ctx.leds + mirror to strip 2
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
