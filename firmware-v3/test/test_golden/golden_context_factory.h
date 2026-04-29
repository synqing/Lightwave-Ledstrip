/**
 * @file golden_context_factory.h
 * @brief Default-initialise an EffectContext for native golden-frame capture.
 *
 * Addresses C2: ensure EVERY field of EffectContext (and its embedded
 * AudioContext::ControlBusFrame) is in a defined state before any effect
 * touches it. EffectContext's default constructor already zeroes scalar
 * fields and value-initialises the embedded AudioContext (whose
 * ControlBusFrame default-constructor zeros all bands[], chroma[], bins64[],
 * bins256[], waveform[], onset*, kick/snare/hihat triggers, etc.). What it
 * does NOT do is wire a real palette or strip pointers — this header does.
 *
 * Usage:
 *     CRGB leds[320]; CRGB s1[160]; CRGB s2[160];
 *     auto ctx = golden::makeContext(leds, s1, s2);
 *     // ctx is fully defaulted; populate audio per-scenario each frame.
 */

#pragma once

#include <cstdint>
#include <cstring>

#include "../../src/plugins/api/EffectContext.h"

namespace golden {

// Total LEDs in a unified K1 buffer (2 strips * 160).
static constexpr uint16_t kLedCount    = 320;
static constexpr uint16_t kCenterPoint = 80;
static constexpr uint16_t kStripLength = 160;
static constexpr uint8_t  kStripCount  = 2;
static constexpr uint16_t kStripCenter = 79;

// ---------------------------------------------------------------------------
// Deterministic palette — a 16-stop "Incandescent" warm palette.
// Values mirror a Sensory-Bridge-style amber gradient. Built once and
// referenced via PaletteRef so palette.getColor() is well-defined.
// ---------------------------------------------------------------------------
inline const CRGBPalette16& goldenPalette() {
    static const CRGBPalette16 kPalette = []() {
        CRGBPalette16 p;
        p[0]  = CRGB(  0,   0,   0);
        p[1]  = CRGB( 16,   0,   0);
        p[2]  = CRGB( 48,   4,   0);
        p[3]  = CRGB( 96,  12,   0);
        p[4]  = CRGB(144,  28,   0);
        p[5]  = CRGB(192,  48,   0);
        p[6]  = CRGB(224,  80,   8);
        p[7]  = CRGB(240, 112,  16);
        p[8]  = CRGB(248, 144,  32);
        p[9]  = CRGB(252, 176,  48);
        p[10] = CRGB(255, 200,  72);
        p[11] = CRGB(255, 220, 112);
        p[12] = CRGB(255, 232, 152);
        p[13] = CRGB(255, 240, 192);
        p[14] = CRGB(255, 248, 224);
        p[15] = CRGB(255, 255, 255);
        return p;
    }();
    return kPalette;
}

// ---------------------------------------------------------------------------
// Make a fully-defaulted EffectContext suitable for native capture.
// All non-pointer scalar fields rely on EffectContext's default constructor.
// ---------------------------------------------------------------------------
inline lightwaveos::plugins::EffectContext makeContext(
    CRGB* leds,
    CRGB* strip1,
    CRGB* strip2)
{
    lightwaveos::plugins::EffectContext ctx;  // default ctor zeroes everything

    // ---- LED buffers ----
    ctx.leds         = leds;
    ctx.ledCount     = kLedCount;
    ctx.centerPoint  = kCenterPoint;

    // ---- Palette ----
    ctx.palette      = lightwaveos::plugins::PaletteRef(&goldenPalette());

    // ---- Animation parameters ----
    ctx.brightness   = 255;
    ctx.speed        = 15;
    ctx.gHue         = 0;
    ctx.mood         = 128;
    ctx.intensity    = 128;
    ctx.saturation   = 255;
    ctx.complexity   = 128;
    ctx.variation    = 64;
    ctx.fadeAmount   = 20;

    // ---- Timing — 120 FPS canonical ----
    ctx.deltaTimeMs        = 8;
    ctx.deltaTimeSeconds   = 0.008f;
    ctx.rawDeltaTimeMs     = 8;
    ctx.rawDeltaTimeSeconds= 0.008f;
    ctx.frameNumber        = 0;
    ctx.totalTimeMs        = 0;
    ctx.rawTotalTimeMs     = 0;

    // ---- Zone — global render ----
    ctx.zoneId      = 0xFF;
    ctx.zoneStart   = 0;
    ctx.zoneLength  = kLedCount;

    // ---- Dual-strip channel API ----
    ctx.stripLeds[0]    = strip1;
    ctx.stripLeds[1]    = strip2;
    ctx.stripLength     = kStripLength;
    ctx.stripCount      = kStripCount;
    ctx.stripCenter     = kStripCenter;
    ctx.dualChannelMode = false;

    return ctx;
}

// ---------------------------------------------------------------------------
// Reset a context's per-frame mutable state at the head of each render.
// ---------------------------------------------------------------------------
inline void beginFrame(lightwaveos::plugins::EffectContext& ctx, uint32_t frameIndex) {
    if (ctx.leds) {
        std::memset(ctx.leds, 0, sizeof(CRGB) * ctx.ledCount);
    }
    if (ctx.stripLeds[0]) {
        std::memset(ctx.stripLeds[0], 0, sizeof(CRGB) * ctx.stripLength);
    }
    if (ctx.stripLeds[1]) {
        std::memset(ctx.stripLeds[1], 0, sizeof(CRGB) * ctx.stripLength);
    }
    ctx.frameNumber    = frameIndex;
    ctx.totalTimeMs    = frameIndex * ctx.deltaTimeMs;
    ctx.rawTotalTimeMs = frameIndex * ctx.rawDeltaTimeMs;

    // Zero the audio surface every frame — scenarios opt-in per frame.
    ctx.audio = lightwaveos::plugins::AudioContext{};
}

// ---------------------------------------------------------------------------
// Mirror RendererActor's unified -> strip split when an effect did NOT
// opt into dualChannelMode. Mirrors the production memcpy semantics so the
// captured GFRM v2 frame reflects what would actually reach the LEDs.
// ---------------------------------------------------------------------------
inline void mirrorUnifiedToStrips(lightwaveos::plugins::EffectContext& ctx) {
    if (!ctx.leds || !ctx.stripLeds[0] || !ctx.stripLeds[1]) return;
    std::memcpy(ctx.stripLeds[0], ctx.leds,                     sizeof(CRGB) * ctx.stripLength);
    std::memcpy(ctx.stripLeds[1], ctx.leds + ctx.stripLength,   sizeof(CRGB) * ctx.stripLength);
}

} // namespace golden
