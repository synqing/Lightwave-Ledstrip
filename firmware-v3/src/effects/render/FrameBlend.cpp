/**
 * @file FrameBlend.cpp
 * @brief Implementation of `applyFrameBlending` — the Layer 5 frame post-process.
 *
 * See `FrameBlend.h` for the contract and integration point. Per-pixel cost is
 * one float multiply-add (1 - dtCorrected scalar precomputed) plus a CRGB
 * lerp; ~150 ns/pixel at 240 MHz, ~50 µs for a 320-LED frame. Well under the
 * 2.0 ms per-frame ceiling.
 */

#include "FrameBlend.h"

#include <cmath>

namespace lightwaveos {
namespace effects {
namespace render {

// ─── Local helpers ────────────────────────────────────────────────────────────

/**
 * 8-bit per-channel lerp: `out = a × (1 - mix) + b × mix` with `mix` in [0, 1].
 * Self-contained — does not depend on FastLED's `blend()`, which the native
 * test mock does not provide.
 */
static inline CRGB lerpCRGB(const CRGB& a, const CRGB& b, float mix) {
    if (mix <= 0.0f) return a;
    if (mix >= 1.0f) return b;
    const float inv = 1.0f - mix;
    int r = static_cast<int>(static_cast<float>(a.r) * inv +
                             static_cast<float>(b.r) * mix + 0.5f);
    int g = static_cast<int>(static_cast<float>(a.g) * inv +
                             static_cast<float>(b.g) * mix + 0.5f);
    int b8 = static_cast<int>(static_cast<float>(a.b) * inv +
                              static_cast<float>(b.b) * mix + 0.5f);
    if (r < 0) r = 0; else if (r > 255) r = 255;
    if (g < 0) g = 0; else if (g > 255) g = 255;
    if (b8 < 0) b8 = 0; else if (b8 > 255) b8 = 255;
    return CRGB(static_cast<uint8_t>(r),
                static_cast<uint8_t>(g),
                static_cast<uint8_t>(b8));
}

// ─── applyFrameBlending ───────────────────────────────────────────────────────

void applyFrameBlending(CRGB* leds,
                        CRGB* prevFrame,
                        uint16_t ledCount,
                        uint8_t mood,
                        float dt) {
    if (leds == nullptr || prevFrame == nullptr || ledCount == 0) {
        return;
    }

    // mood=0 — no blend. Pass current frame through unchanged but synchronise
    // prevFrame so the next call (with mood>0) starts from a consistent state.
    if (mood == 0) {
        for (uint16_t i = 0; i < ledCount; ++i) {
            prevFrame[i] = leds[i];
        }
        return;
    }

    // Persistence coefficient: 0.92 maximum at mood=255; linear in mood.
    const float blendCoeff =
        (static_cast<float>(mood) / 255.0f) * 0.92f;

    // dt-correct against a 120 FPS reference. At dt = 1/120 s the exponent is
    // 1.0 (no change); at dt = 1/60 s the exponent is 2.0 (one 60 FPS frame
    // behaves like two 120 FPS frames). Identical convention to
    // `lightwaveos::effects::persistence::dtDecay`.
    float dtCorrected;
    if (dt <= 0.0f) {
        dtCorrected = blendCoeff;
    } else if (blendCoeff < 1.0e-6f) {
        // Underflow guard — powf(0, x) is implementation-defined for x > 0.
        dtCorrected = 0.0f;
    } else {
        dtCorrected = ::powf(blendCoeff, dt * 120.0f);
        if (dtCorrected < 0.0f) dtCorrected = 0.0f;
        if (dtCorrected > 1.0f) dtCorrected = 1.0f;
    }

    // mixToCurrent in [0, 1]: 0 = output is purely prevFrame (impossible here
    // because dtCorrected < 1 once mood < 255), 1 = output is purely current.
    const float mixToCurrent = 1.0f - dtCorrected;

    for (uint16_t i = 0; i < ledCount; ++i) {
        const CRGB blended = lerpCRGB(prevFrame[i], leds[i], mixToCurrent);
        leds[i] = blended;
        prevFrame[i] = blended;
    }
}

}  // namespace render
}  // namespace effects
}  // namespace lightwaveos
