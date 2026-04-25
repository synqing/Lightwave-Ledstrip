/**
 * @file BloomParityEffect.h
 * @brief Sensory Bridge "Bloom" parity port for LightwaveOS v2
 *
 * GOAL: Match the visual motion/transport/signature behaviour of:
 *   void light_mode_bloom()  (Sensory Bridge firmware: lightshow_modes.h)
 *
 * Parity spine (must remain in this exact order each frame):
 *  1) clear
 *  2) advect history by subpixel offset (draw_sprite equivalent)
 *  3) compute chroma-summed injection colour
 *  4) overwrite centre pixels with injection
 *  5) copy current -> history (PRE presentation)
 *  6) tail quadratic taper (presentation only)
 *  7) mirror (presentation only)
 *
 * Notes:
 * - This effect is CENTER ORIGIN compliant via the same "mirror after transport" trick
 *   used by Sensory Bridge Bloom: we simulate ONE radial half (right side) then mirror.
 * - ZoneComposer reuses ONE effect instance across zones, so all state is per-zone.
 *
 * Effect ID: 121 (replaces BeatPulseBloomEffect)
 */

#pragma once

#include "../CoreEffects.h"
#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#include "../../config/effect_ids.h"
#endif

namespace lightwaveos::effects::ieffect {

class BloomParityEffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_BLOOM_PARITY;
    BloomParityEffect() = default;
    ~BloomParityEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;

    const plugins::EffectMetadata& getMetadata() const override;

    // ------------------------------------------------------------------------
    // Runtime test-mode selector — Captain hardware A/B framework.
    // Baseline preserves shipped behaviour. A-I exercise distinct hypotheses
    // about the second-motion-layer artefact (observation #28405 + F4 audit).
    // Non-destructive: render() reads s_prismMode and applies LOCAL overrides;
    // s_prismOpacity / s_bulbOpacity / s_alpha statics are never mutated.
    // Cycle via SerialCLI key 'M'.
    // ------------------------------------------------------------------------
    enum class PrismMode : uint8_t {
        Baseline = 0,   // current shipped behaviour (prism 0.20, mirror, edge floor 0.60, bulb 0.40, alpha 0.99)
        A        = 1,   // prism disabled (opacity = 0)
        B        = 2,   // prism halved (opacity = 0.10)
        C        = 3,   // prism standard but skip mirrorImageDownwards
        D        = 4,   // prism additive cap (proportional to existing buf brightness)
        E        = 5,   // prism multiplicative blend (highlights existing content only)
        F        = 6,   // prism at SB 4.1 parity opacity (0.25)
        G        = 7,   // edge fade to zero (matches SB 4.1 hard-kill on edges)
        H        = 8,   // bulb cover disabled (matches SB 4.1 default BULB_OPACITY = 0)
        I        = 9,   // transport alpha sweep (0.97 instead of 0.99)
    };
    static constexpr uint8_t kPrismModeCount = 10;

    static PrismMode   getPrismMode()              { return s_prismMode; }
    static void        setPrismMode(PrismMode m)   { s_prismMode = (static_cast<uint8_t>(m) < kPrismModeCount) ? m : PrismMode::Baseline; }
    static const char* getPrismModeName(PrismMode m);

    // Runtime-tunable parameters (static: one instance shared across zones)
    static float   getPrismOpacity()       { return s_prismOpacity; }
    static float   getBulbOpacity()        { return s_bulbOpacity; }
    static float   getAlpha()              { return s_alpha; }
    static uint8_t getSquareIter()         { return s_squareIter; }
    static uint8_t getPrismIterations()    { return s_prismIterations; }
    static float   getGHueSpeed()          { return s_gHueSpeed; }
    static float   getSpatialSpread()      { return s_spatialSpread; }
    static float   getIntensityCoupling()  { return s_intensityCoupling; }

    static void setPrismOpacity(float v)       { s_prismOpacity = (v < 0.0f) ? 0.0f : (v > 5.0f ? 5.0f : v); }
    static void setBulbOpacity(float v)        { s_bulbOpacity  = (v < 0.0f) ? 0.0f : (v > 1.0f ? 1.0f : v); }
    static void setAlpha(float v)              { s_alpha = (v < 0.0f) ? 0.0f : (v > 1.0f ? 1.0f : v); }
    static void setSquareIter(uint8_t v)       { s_squareIter = (v > 4) ? 4 : v; }
    static void setPrismIterations(uint8_t v)  { s_prismIterations = (v > 5) ? 5 : v; }
    static void setGHueSpeed(float v)          { s_gHueSpeed = (v < -10.0f) ? -10.0f : (v > 10.0f ? 10.0f : v); }
    static void setSpatialSpread(float v)      { s_spatialSpread = (v < 0.0f) ? 0.0f : (v > 255.0f ? 255.0f : v); }
    static void setIntensityCoupling(float v)  { s_intensityCoupling = (v < 0.0f) ? 0.0f : (v > 1.0f ? 1.0f : v); }

private:
    // ------------------------------------------------------------------------
    // Fixed limits (match LightwaveOS zone model)
    // ------------------------------------------------------------------------
    static constexpr uint8_t  kMaxZones = 4;
    static constexpr uint16_t kMaxLeds  = 160;   // ZoneComposer uses 160 LEDs per zone for LGP
    static constexpr uint16_t kChromaBins = 12;

    // ------------------------------------------------------------------------
    // Internal float RGB (0..1)
    // ------------------------------------------------------------------------
    struct RGBf {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
    };

    // ⚠️ PSRAM-ALLOCATED — large buffers MUST NOT live in DRAM (see MEMORY_ALLOCATION.md)
    struct PsramData {
        RGBf prev[kMaxZones][kMaxLeds];   // Per-zone transport history
        RGBf curr[kMaxZones][kMaxLeds];   // Per-zone current working
        RGBf fx[kMaxLeds];                // Prism working buffer (shared, one zone at a time)
        RGBf tmp[kMaxLeds];               // Scratch for scale/shift/mirror
    };
    PsramData* m_ps = nullptr;

    // Per-zone chroma peak follower (matches make_smooth_chromagram() behaviour)
    float m_chromaMaxPeak[kMaxZones];

    // Per-zone auto hue shift state (parity-ish with process_color_shift())
    float m_huePosition[kMaxZones];
    float m_hueShiftSpeed[kMaxZones];
    float m_huePushDirection[kMaxZones];
    float m_hueDestination[kMaxZones];
    float m_hueShiftingMix[kMaxZones];
    float m_hueShiftingMixTarget[kMaxZones];

    // Per-zone chroma mode state (default parity = chromatic_mode true)
    bool  m_chromaticMode[kMaxZones];
    float m_chromaVal[kMaxZones];

    // ------------------------------------------------------------------------
    // Helpers (no allocation, hot-path friendly)
    // ------------------------------------------------------------------------
    static inline float clamp01(float v) {
        return (v < 0.0f) ? 0.0f : (v > 1.0f ? 1.0f : v);
    }
    static inline float wrap01(float v) {
        // Wrap to [0,1)
        while (v < 0.0f) v += 1.0f;
        while (v >= 1.0f) v -= 1.0f;
        return v;
    }

    static inline void clearBuffer(RGBf* buf, uint16_t len) {
        for (uint16_t i = 0; i < len; i++) {
            buf[i].r = buf[i].g = buf[i].b = 0.0f;
        }
    }

    // Sensory Bridge draw_sprite() equivalent: subpixel advection of history buffer.
    static void drawSprite(RGBf* dest, const RGBf* sprite,
                           uint16_t destLen, uint16_t spriteLen,
                           float position, float alpha);

    // Best-effort chroma source: prefers controlBus.chroma[12] if present; else derives from 8-band control bus.
    void getChroma12(const plugins::EffectContext& ctx, float outChroma12[kChromaBins]) const;

    // Parity-ish hue shift driven by audio novelty; keeps Bloom from feeling static.
    // dtVis = visual delta seconds (speed-scaled) for animation timing.
    void processHueShift(uint8_t zone, const plugins::EffectContext& ctx, float dtVis);

    // Compute Bloom's injection colour (matches the summation in light_mode_bloom()).
    // dtRaw = raw delta seconds (unscaled) for audio signal processing.
    RGBf computeInjection(uint8_t zone, const plugins::EffectContext& ctx, float dtRaw);

    // ------------------------------------------------------------------------
    // PostFX: Sensory Bridge "optical cheats" (prism + bulb cover)
    // Applied AFTER mirror, BEFORE LED write. Order: prism → bulb → output.
    // ------------------------------------------------------------------------
    // m_fx and m_tmp now live inside PsramData (m_ps->fx, m_ps->tmp)

    static float    s_prismOpacity;      // Runtime-tunable (default 0.20)
    static float    s_bulbOpacity;       // Runtime-tunable (default 0.40)
    static float    s_alpha;             // Transport persistence (default 0.99)
    static uint8_t  s_squareIter;        // Contrast shaping passes (default 1)
    static uint8_t  s_prismIterations;   // Prism ghost layer count (default 1)
    static float    s_gHueSpeed;         // Palette sweep speed multiplier (default 1.0)
    static float    s_spatialSpread;     // Palette spread centre→edge (default 128.0)
    static float    s_intensityCoupling; // Blend: 0=spatial, 1=intensity (default 0.0)
    static PrismMode s_prismMode;        // Runtime test-mode selector (default Baseline)

    enum class PrismBlendStyle : uint8_t {
        Standard       = 0,   // additive: buf += fx*opacity
        AdditiveCapped = 1,   // additive but capped proportional to buf brightness (mode D)
        Multiplicative = 2,   // buf *= (1 + fx*opacity) — highlight only (mode E)
    };

    static void scaleImageToHalf(RGBf* buf, uint16_t len, RGBf* temp);
    static void shiftLedsUp(RGBf* buf, uint16_t len, uint16_t offset, RGBf* temp);
    static void mirrorImageDownwards(RGBf* buf, uint16_t len, RGBf* temp);
    static void applyPrismEffect(RGBf* buf, uint16_t len, uint8_t iterations,
                                 float opacity, RGBf* fx, RGBf* temp,
                                 bool useMirror = true,
                                 PrismBlendStyle blendStyle = PrismBlendStyle::Standard);
    static void renderBulbCover(RGBf* buf, uint16_t len, float bulbOpacity);
};

} // namespace lightwaveos::effects::ieffect
