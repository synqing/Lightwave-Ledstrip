/**
 * @file Contrast.h
 * @brief K1 contrast curve — hoisted from SbK1BaseEffect to a free function
 *        accessible to all effects without SB inheritance.
 *
 * Rationale (I-1 from EFFECT_FRAMEWORK_STANDARD.md §INCIDENTAL):
 *   Four effects open-coded `bin *= bin` (pure squaring, squareIter = 1.0)
 *   instead of the canonical SB mix-back curve (squareIter = kSbK1SquareIter).
 *   Pure squaring is a different perceptual response and departs from SB parity.
 *   Hoisting the function here makes it reachable without inheritance so all
 *   effects can adopt the canonical curve.
 *
 * ── Canonical K1 usage ───────────────────────────────────────────────────
 *
 *   float contrasted = applyContrast(band, kSbK1SquareIter);
 *
 *   With squareIter = 0.65:
 *     intIter  = 0  → no full squarings applied
 *     fractIter = 0.65 → bin = bin*(1-0.65) + bin²*0.65
 *              = bin*0.35 + bin²*0.65
 *   This is the "(x²·0.65 + x·0.35) mix-back" described in the standard.
 *
 * ── Constraints ──────────────────────────────────────────────────────────
 *   - No heap allocation. Pure arithmetic.
 *   - Identical to SbK1BaseEffect::applyContrast — verified against source.
 *   - British English in comments.
 */

#pragma once

#include <cmath>

namespace lightwaveos {
namespace effects {
namespace math {

/// Canonical squareIter for the K1 contrast curve (SB parity mix-back).
/// Produces: bin*0.35 + bin²*0.65
static constexpr float kSbK1SquareIter = 0.65f;

/**
 * @brief K1 contrast curve with fractional squaring iteration.
 *
 * Applies an integer number of `bin *= bin` passes, then linearly
 * interpolates towards one additional squaring by the fractional remainder.
 *
 * Examples:
 *   squareIter = 0.65 → bin*0.35 + bin²*0.65   (canonical K1 mix-back)
 *   squareIter = 1.0  → bin²                     (pure squaring)
 *   squareIter = 1.5  → lerp(bin², bin⁴, 0.5)
 *
 * @param bin        Input value in [0, 1].
 * @param squareIter Squaring iterations; fractional part is a lerp coefficient.
 * @return           Contrast-adjusted value, monotone in [0, 1].
 */
static inline float applyContrast(float bin, float squareIter) {
    auto intIter  = static_cast<uint8_t>(squareIter);
    float fractIter = squareIter - floorf(squareIter);

    for (uint8_t i = 0; i < intIter; ++i) {
        bin *= bin;
    }

    if (fractIter > 0.01f) {
        float squared = bin * bin;
        bin = bin * (1.0f - fractIter) + squared * fractIter;
    }

    return bin;
}

}  // namespace math
}  // namespace effects
}  // namespace lightwaveos
