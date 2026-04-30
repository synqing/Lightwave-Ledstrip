/**
 * @file PersistenceHelpers.h
 * @brief Centralised dt-correct persistence and blending primitives.
 *
 * Phase 1 Move 1.1 substrate per Topology_Reconciliation §5. Hoists the
 * existing dtDecay() pattern from ChromaUtils.h into a project-wide library
 * and adds the minimum subset future Phase 6 PDE work depends on:
 *
 *   - dtDecay(scalar)        — frame-rate-independent exponential decay
 *   - dtDecay3(CRGB)         — same, applied per channel in-place
 *   - emaArrayDt(float[])    — dt-correct exponential moving average
 *   - crossBlendArray()      — elementwise linear interpolation
 *
 * Heavier helpers (heatStep1D, velocityAniso1D) intentionally deferred to
 * Phase 6 — their behaviour is still under research.
 *
 * ── Constraints ──────────────────────────────────────────────────────────
 *
 *  - NO heap allocation. Every helper is `static inline` operating on caller-
 *    owned storage; safe to call transitively from render().
 *  - dt-correct semantics. EMA alpha is computed via `1 - exp(-dt / tau)` so
 *    behaviour stays identical at 60 Hz, 120 Hz, or with variable frame
 *    pacing.
 *  - Cheap. Per-call cost is dominated by one or two `expf`/`powf` plus an
 *    n-element loop; well under 1 µs typical for n ≤ 160.
 *  - Portable. Pure C++ + <cmath>; no platform intrinsics. Builds against
 *    the FastLED `<FastLED.h>` CRGB on ESP32-S3 and the `fastled_mock.h`
 *    CRGB shim under native unit tests.
 *
 * ── British English ──────────────────────────────────────────────────────
 * Comments use British spelling (centre, colour, behaviour). The exposed
 * names mirror existing ChromaUtils.h conventions (camelCase) so call sites
 * port across with a single `using` statement.
 */

#pragma once

#include <cmath>
#include <cstddef>

#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace persistence {

/**
 * @brief Frame-rate-independent exponential decay (scalar).
 *
 * Identical contract to the original `lightwaveos::effects::chroma::dtDecay`:
 * `rate60fps` is the per-frame multiplier the effect would have applied at a
 * 60 fps reference, and `dt` is the actual frame interval in seconds.
 *
 * Returns `value * rate60fps^(dt * 60)`. Special cases: rate = 1 is the
 * identity, rate = 0 collapses to zero.
 */
static inline float dtDecay(float value, float rate60fps, float dt) {
    return value * powf(rate60fps, dt * 60.0f);
}

/**
 * @brief Per-channel CRGB decay, in place.
 *
 * Applies `dtDecay` to each of r, g, b independently, preserving the CRGB
 * structure. Channel arithmetic is done in float and cast back to uint8_t at
 * the end; no clamping is required because the multiplier is in [0, 1] for
 * any well-behaved decay rate.
 */
static inline void dtDecay3(CRGB& rgb, float rate60fps, float dt) {
    const float k = powf(rate60fps, dt * 60.0f);
    rgb.r = static_cast<uint8_t>(static_cast<float>(rgb.r) * k);
    rgb.g = static_cast<uint8_t>(static_cast<float>(rgb.g) * k);
    rgb.b = static_cast<uint8_t>(static_cast<float>(rgb.b) * k);
}

/**
 * @brief Dt-correct exponential moving average over a float array.
 *
 * Computes `alpha = 1 - exp(-dt / tau_s)` once, then applies
 * `arr[i] = arr[i] * (1 - alpha) + src * alpha` to every element. The same
 * scalar `src` is used for all elements (typical for "track this signal into
 * each band"). Pass per-element targets via `crossBlendArray` instead.
 *
 * Edge cases:
 *   - `tau_s` very large vs `dt`: alpha → 0, array unchanged (infinite memory)
 *   - `tau_s` very small vs `dt`: alpha → 1, array replaced with src
 *   - `n == 0` or `arr == nullptr`: no-op
 */
static inline void emaArrayDt(float* arr,
                              size_t n,
                              float src,
                              float tau_s,
                              float dt) {
    if (arr == nullptr || n == 0) {
        return;
    }
    const float alpha = 1.0f - expf(-dt / tau_s);
    const float oneMinusAlpha = 1.0f - alpha;
    for (size_t i = 0; i < n; ++i) {
        arr[i] = arr[i] * oneMinusAlpha + src * alpha;
    }
}

/**
 * @brief Dt-correct fade-to-black over an LED strip segment.
 *
 * Drop-in replacement for FastLED's frame-coupled `fadeToBlackBy(leds, n, X)`.
 * `fadeBy` is on the same 0–255 scale (FastLED convention).
 *
 * The K values across the codebase were calibrated at K1's native 120 FPS
 * with frame-coupled `fadeToBlackBy`, where `fadeBy=32` produced a per-frame
 * survival of `(256-32)/256 = 0.875`. To preserve that visual behaviour
 * under dt-correction (which uses a 60 FPS reference internally), the
 * per-frame survival is squared: `rate60fps = rate120fps²`. This ensures
 * `powf(rate60fps, dt*60)` evaluates to `rate120fps` when dt = 1/120 s.
 *
 * @param leds    Pointer to the first CRGB element.
 * @param n       Number of LEDs to process.
 * @param fadeBy  Fade amount 0–255 (FastLED convention). 0 = no fade, 255 = instant black.
 * @param dt      Actual frame interval in seconds (use ctx.getSafeDeltaSeconds()).
 */
static inline void fadeToBlackByDt(CRGB* leds, size_t n,
                                    uint8_t fadeBy, float dt) {
    // K values (fadeBy 0–255) were calibrated at K1's native 120 FPS with
    // frame-coupled fadeToBlackBy. dtDecay3 uses a 60 FPS reference
    // (powf(rate, dt * 60)). To preserve the visual time-constant that
    // effects were tuned at, convert the 120 FPS per-frame survival to a
    // 60 FPS equivalent: rate60 = rate120² (one 60 FPS frame = two 120 FPS
    // frames). Verified: at 120 FPS, powf(rate120², dt*60) = rate120.
    const float rate120 = (256.0f - static_cast<float>(fadeBy)) / 256.0f;
    const float rate60fps = rate120 * rate120;
    for (size_t i = 0; i < n; ++i) {
        dtDecay3(leds[i], rate60fps, dt);
    }
}

/**
 * @brief Dt-correct fade-to-black with explicit float survival rate.
 *
 * Use when you need a specific rate rather than a FastLED-integer fadeBy.
 * Named distinctly from the uint8_t overload to avoid int-literal ambiguity.
 *
 * @param rate60fps  Per-channel survival fraction at 60 fps reference [0, 1].
 */
static inline void fadeToBlackByDtRate(CRGB* leds, size_t n,
                                        float rate60fps, float dt) {
    for (size_t i = 0; i < n; ++i) {
        dtDecay3(leds[i], rate60fps, dt);
    }
}

/**
 * @brief Elementwise linear interpolation between two float arrays.
 *
 * Writes `dest[i] = a[i] * (1 - alpha) + b[i] * alpha`. `dest` may alias `a`
 * or `b` safely because each element is read before it is written.
 *
 * Edge cases:
 *   - alpha = 0: dest <- a
 *   - alpha = 1: dest <- b
 *   - n == 0 or any pointer null: no-op
 */
static inline void crossBlendArray(float* dest,
                                   const float* a,
                                   const float* b,
                                   size_t n,
                                   float alpha) {
    if (dest == nullptr || a == nullptr || b == nullptr || n == 0) {
        return;
    }
    const float oneMinusAlpha = 1.0f - alpha;
    for (size_t i = 0; i < n; ++i) {
        dest[i] = a[i] * oneMinusAlpha + b[i] * alpha;
    }
}

}  // namespace persistence
}  // namespace effects
}  // namespace lightwaveos
