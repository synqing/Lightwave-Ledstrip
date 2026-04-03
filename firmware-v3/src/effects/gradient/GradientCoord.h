/**
 * @file GradientCoord.h
 * @brief K1-specific coordinate helpers for gradient sampling
 *
 * These functions convert a global LED index (0-319) into the coordinate
 * spaces used by the gradient system. They are physically faithful to the
 * K1 dual-edge Light Guide Plate topology:
 *
 *   Physical layout:
 *   Strip 1: LED 0 ←←←← LED 79 | LED 80 →→→→ LED 159
 *   Strip 2: LED 160 ←← LED 239 | LED 240 →→→ LED 319
 *                           ↑ CENTRE PAIR (79/80)
 *
 * The true geometric centre falls BETWEEN LEDs 79 and 80. All coordinate
 * functions treat this midpoint (79.5) as the origin, producing symmetric
 * results for the left and right halves.
 *
 * All functions are inline and allocation-free. Safe for render().
 */

#pragma once

#include <cstdint>
#include <cmath>
#include "../CoreEffects.h"

namespace lightwaveos {
namespace effects {
namespace gradient {

// ============================================================================
// Constants (mirror CoreEffects.h for self-contained use)
// ============================================================================

static constexpr float kCentreMidpoint = 79.5f;    ///< True geometric centre
static constexpr float kHalfSpan = 80.0f;           ///< LEDs per half-strip
static constexpr uint16_t kStripLength = 160;        ///< LEDs per physical strip
static constexpr uint16_t kTotalLeds = 320;          ///< Total LED count

static constexpr uint8_t EDGE_A = 0;  ///< Strip 1
static constexpr uint8_t EDGE_B = 1;  ///< Strip 2

// ============================================================================
// Strip-local index
// ============================================================================

/**
 * @brief Convert global LED index (0-319) to strip-local index (0-159)
 * @param index Global LED index
 * @return Strip-local index [0-159]
 */
inline uint16_t stripLocal(uint16_t index) {
    return (index < kStripLength) ? index : (index - kStripLength);
}

// ============================================================================
// Edge identification
// ============================================================================

/**
 * @brief Get which physical strip an LED belongs to
 * @param index Global LED index (0-319)
 * @return EDGE_A (0) for strip 1, EDGE_B (1) for strip 2
 */
inline uint8_t edgeId(uint16_t index) {
    return (index < kStripLength) ? EDGE_A : EDGE_B;
}

// ============================================================================
// Centre-distance coordinate: u_center [0.0 - 1.0]
// ============================================================================

/**
 * @brief Normalised centre distance
 * @param index Global LED index (0-319)
 * @return 0.0 at centre pair (79/80), 1.0 at edge (0/159)
 *
 * Uses the true geometric midpoint (79.5) for symmetric results:
 *   LED 79 → 0.00625  (≈0, just off centre)
 *   LED 80 → 0.00625  (symmetric with 79)
 *   LED  0 → 0.99375  (≈1, far edge)
 *   LED 159→ 0.99375  (symmetric with 0)
 */
inline float uCenter(uint16_t index) {
    float local = (float)stripLocal(index);
    return fabsf(local - kCentreMidpoint) / kHalfSpan;
}

// ============================================================================
// Signed position: u_signed [-1.0 to +1.0]
// ============================================================================

/**
 * @brief Signed position relative to centre pair
 * @param index Global LED index (0-319)
 * @return -1.0 at LED 0 (left edge), 0.0 at centre, +1.0 at LED 159 (right edge)
 *
 * This fixes the asymmetry in EffectContext::getSignedPosition() which
 * uses centerPoint=80 as origin instead of the true midpoint 79.5.
 */
inline float uSigned(uint16_t index) {
    float local = (float)stripLocal(index);
    return (local - kCentreMidpoint) / kHalfSpan;
}

// ============================================================================
// Half-strip index: u_half [0 - 79]
// ============================================================================

/**
 * @brief Distance from centre as a discrete half-strip index
 * @param index Global LED index (0-319)
 * @return 0 at centre pair, 79 at edge
 *
 * Equivalent to CoreEffects::centerPairDistance() but works on global indices.
 */
inline uint8_t halfIndex(uint16_t index) {
    uint16_t local = stripLocal(index);
    return (local <= CENTER_LEFT) ? (uint8_t)(CENTER_LEFT - local)
                                  : (uint8_t)(local - CENTER_RIGHT);
}

// ============================================================================
// Local position: u_local [0.0 - 1.0]
// ============================================================================

/**
 * @brief Normalised position along a single strip
 * @param index Global LED index (0-319)
 * @return 0.0 at LED 0/160, 1.0 at LED 159/319
 *
 * For linear sweeps that run edge-to-edge rather than centre-out.
 */
inline float uLocal(uint16_t index) {
    return (float)stripLocal(index) / (float)(kStripLength - 1);
}

// ============================================================================
// Dual-strip helpers
// ============================================================================

/**
 * @brief Get the corresponding LED on the other strip
 * @param index Global LED index (0-319)
 * @return Corresponding index on the other strip
 *
 * strip1[i] → strip2[i], and vice versa.
 */
inline uint16_t dualStripIndex(uint16_t index) {
    return (index < kStripLength) ? (index + kStripLength) : (index - kStripLength);
}

/**
 * @brief Write a colour to both strips at the same strip-local position
 * @param leds LED buffer (must be at least 320 entries)
 * @param localIdx Strip-local index [0-159]
 * @param colourA Colour for strip 1
 * @param colourB Colour for strip 2
 * @param ledCount Total LED count (for bounds checking)
 */
inline void writeDualEdge(CRGB* leds, uint16_t localIdx,
                          CRGB colourA, CRGB colourB, uint16_t ledCount) {
    if (localIdx < kStripLength && localIdx < ledCount) {
        leds[localIdx] = colourA;
    }
    uint16_t strip2Idx = localIdx + kStripLength;
    if (strip2Idx < ledCount) {
        leds[strip2Idx] = colourB;
    }
}

/**
 * @brief Write a colour symmetrically from centre on both strips
 * @param leds LED buffer
 * @param dist Distance from centre [0-79]
 * @param colourA Colour for strip 1
 * @param colourB Colour for strip 2
 * @param ledCount Total LED count
 *
 * Writes to 4 LEDs: strip1 left, strip1 right, strip2 left, strip2 right.
 */
inline void writeCentrePairDual(CRGB* leds, uint8_t dist,
                                CRGB colourA, CRGB colourB, uint16_t ledCount) {
    uint16_t left  = CENTER_LEFT - dist;
    uint16_t right = CENTER_RIGHT + dist;
    if (left < kStripLength && left < ledCount)  leds[left]  = colourA;
    if (right < kStripLength && right < ledCount) leds[right] = colourA;

    uint16_t left2  = kStripLength + CENTER_LEFT - dist;
    uint16_t right2 = kStripLength + CENTER_RIGHT + dist;
    if (left2 < ledCount)  leds[left2]  = colourB;
    if (right2 < ledCount) leds[right2] = colourB;
}

} // namespace gradient
} // namespace effects
} // namespace lightwaveos
