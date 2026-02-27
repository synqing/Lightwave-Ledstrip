/**
 * @file EsV11RefUtil.h
 * @brief Utilities for ES v1.1 reference show ports.
 *
 * These effects are intended for parity comparisons against canonical
 * Emotiscope hardware. They prioritise algorithmic similarity over LWLS
 * aesthetic constraints (e.g. HSV hue gradients).
 */

#pragma once

#include "../../CoreEffects.h"
#include "../../../plugins/api/EffectContext.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

namespace lightwaveos::effects::ieffect::esv11ref {

static inline float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static inline float interp12(const float chroma[12], float progress01) {
    float x = clamp01(progress01) * 11.0f;
    uint8_t idx = static_cast<uint8_t>(x);
    float frac = x - static_cast<float>(idx);
    if (idx >= 11) return chroma[11];
    return lerp(chroma[idx], chroma[idx + 1], frac);
}

static inline float interp64(const float bins64[64], float progress01) {
    float x = clamp01(progress01) * 63.0f;
    uint8_t idx = static_cast<uint8_t>(x);
    float frac = x - static_cast<float>(idx);
    if (idx >= 63) return bins64[63];
    return lerp(bins64[idx], bins64[idx + 1], frac);
}

static inline CRGB hsvProgress(const plugins::EffectContext& ctx, float progress01, float value01) {
#ifdef NATIVE_BUILD
    (void)ctx;
    (void)progress01;
    (void)value01;
    return CRGB::Black;
#else
    uint8_t hue = static_cast<uint8_t>(clamp01(progress01) * 255.0f) + ctx.gHue;
    uint8_t sat = ctx.saturation;
    uint8_t val = static_cast<uint8_t>(clamp01(value01) * 255.0f);
    // Apply global brightness in the same way as other effects.
    val = scale8(val, ctx.brightness);
    return CHSV(hue, sat, val);
#endif
}

static inline void clearAll(plugins::EffectContext& ctx) {
#ifdef NATIVE_BUILD
    (void)ctx;
#else
    fill_solid(ctx.leds, ctx.ledCount, CRGB::Black);
#endif
}

} // namespace lightwaveos::effects::ieffect::esv11ref

