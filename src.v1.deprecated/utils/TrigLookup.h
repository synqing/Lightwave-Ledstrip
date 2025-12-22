#ifndef TRIG_LOOKUP_H
#define TRIG_LOOKUP_H

#include <stdint.h>

/**
 * Fast Trigonometry Lookup Tables
 *
 * Provides 256-entry lookup tables for sine/cosine calculations.
 * Uses fixed-point math for speed on ESP32-S3.
 *
 * Memory usage: ~768 bytes total
 *   - SIN8_TABLE:       256 bytes (uint8_t, 0-255 output)
 *   - SIN8_SIGNED:      256 bytes (int8_t, -127 to 127 output)
 *   - SIN_TABLE_FLOAT:  256 * sizeof(float) = ~1024 bytes (optional)
 *
 * Usage:
 *   uint8_t brightness = TrigLookup::sin8_fast(phase);      // 0-255
 *   int8_t  offset     = TrigLookup::sin8_signed(phase);    // -127 to 127
 *   float   wave       = TrigLookup::sinf_fast(phase);      // -1.0 to 1.0
 *
 * ============================================================================
 * CRITICAL: PRECISION AND ERROR COMPOUNDING CONSIDERATIONS
 * ============================================================================
 *
 * PRECISION:
 *   - Angular resolution: 256 steps = 1.406° per step (no interpolation)
 *   - Float table max error: ±0.0123 (~1.23% of full scale)
 *   - Integer tables: ±1-2 LSB (~1.5-2% worst case)
 *
 * WHEN ERRORS DO NOT COMPOUND (SAFE):
 *   - Per-LED brightness calculations (each LED independent per frame)
 *   - Phase accumulators where phase is stored in float, lookup is output only
 *   - Example (SAFE):
 *       float phase = 0;
 *       phase += speed;                              // Float accumulator (exact)
 *       uint8_t val = sin8_fast(rad_to_theta(phase)); // Error in output only
 *
 * WHEN ERRORS DO COMPOUND (DANGEROUS):
 *   - Feedback loops where lookup output feeds back into next iteration's input
 *   - Numerical integration / physics simulations using lookup for derivatives
 *   - Coupled oscillator systems
 *   - Example (DANGEROUS):
 *       float state = 0;
 *       state = sinf_fast(rad_to_theta(state * 10));  // ERROR FEEDS BACK!
 *       // After N iterations, drift ≈ N × 0.0123 × feedback_gain
 *
 * RECOMMENDATIONS:
 *   1. For display/brightness: Use lookup tables freely (errors invisible)
 *   2. For physics state evolution: Use native sin()/cos() for accuracy
 *   3. For feedback systems: Either use native trig OR add periodic re-sync
 *   4. If precision critical: Consider 1024-entry table (0.3% error) or
 *      implement linear interpolation between samples
 *
 * Most LED effects in this codebase use pattern 1 (independent per-LED) or
 * pattern 2 (float phase accumulator), so errors remain bounded and do NOT
 * compound over time.
 * ============================================================================
 */

namespace TrigLookup {

// ==========================================================================
// Lookup Tables (defined in TrigLookup.cpp)
// ==========================================================================

// 256-entry sine table, output 0-255 (half-wave shifted to positive)
// sin8_fast(0) = 128, sin8_fast(64) = 255, sin8_fast(128) = 128, sin8_fast(192) = 0
extern const uint8_t SIN8_TABLE[256];

// 256-entry signed sine table, output -127 to 127
// Useful for oscillation around zero
extern const int8_t SIN8_SIGNED[256];

// 256-entry float sine table, output -1.0 to 1.0
// For effects requiring floating-point precision
extern const float SIN_TABLE_FLOAT[256];

// ==========================================================================
// Fast Inline Functions
// ==========================================================================

/**
 * Fast 8-bit sine lookup
 * @param theta Phase angle (0-255 = 0 to 2*PI)
 * @return Sine value (0-255, with 128 = zero crossing)
 */
inline uint8_t sin8_fast(uint8_t theta) {
    return SIN8_TABLE[theta];
}

/**
 * Fast 8-bit cosine lookup
 * @param theta Phase angle (0-255 = 0 to 2*PI)
 * @return Cosine value (0-255, with 128 = zero crossing)
 */
inline uint8_t cos8_fast(uint8_t theta) {
    return SIN8_TABLE[(uint8_t)(theta + 64)];  // cos = sin + 90 degrees
}

/**
 * Fast signed 8-bit sine lookup
 * @param theta Phase angle (0-255 = 0 to 2*PI)
 * @return Sine value (-127 to 127)
 */
inline int8_t sin8_signed(uint8_t theta) {
    return SIN8_SIGNED[theta];
}

/**
 * Fast signed 8-bit cosine lookup
 * @param theta Phase angle (0-255 = 0 to 2*PI)
 * @return Cosine value (-127 to 127)
 */
inline int8_t cos8_signed(uint8_t theta) {
    return SIN8_SIGNED[(uint8_t)(theta + 64)];
}

/**
 * Fast floating-point sine lookup
 * @param theta Phase angle (0-255 = 0 to 2*PI)
 * @return Sine value (-1.0 to 1.0)
 */
inline float sinf_fast(uint8_t theta) {
    return SIN_TABLE_FLOAT[theta];
}

/**
 * Fast floating-point cosine lookup
 * @param theta Phase angle (0-255 = 0 to 2*PI)
 * @return Cosine value (-1.0 to 1.0)
 */
inline float cosf_fast(uint8_t theta) {
    return SIN_TABLE_FLOAT[(uint8_t)(theta + 64)];
}

// ==========================================================================
// Conversion Utilities
// ==========================================================================

/**
 * Convert radians to 8-bit theta (0-255)
 * @param radians Angle in radians
 * @return Theta value (0-255)
 */
inline uint8_t rad_to_theta(float radians) {
    // 256 / (2 * PI) = 40.7436...
    return (uint8_t)(radians * 40.7436654315f);
}

/**
 * Convert degrees to 8-bit theta (0-255)
 * @param degrees Angle in degrees (0-360)
 * @return Theta value (0-255)
 */
inline uint8_t deg_to_theta(float degrees) {
    // 256 / 360 = 0.7111...
    return (uint8_t)(degrees * 0.71111111f);
}

/**
 * Convert 8-bit theta to radians
 * @param theta Phase angle (0-255)
 * @return Angle in radians (0 to ~2*PI)
 */
inline float theta_to_rad(uint8_t theta) {
    // (2 * PI) / 256 = 0.0245436926...
    return theta * 0.0245436926f;
}

/**
 * Convert integer position to theta with scaling
 * Useful for LED position to phase conversion
 * @param position Position value
 * @param scale Scale factor (e.g., wavelength in LEDs)
 * @return Theta value (0-255)
 */
inline uint8_t pos_to_theta(int32_t position, uint8_t scale) {
    return (uint8_t)((position * 256) / scale);
}

/**
 * Fast sine for float angle (radians), using lookup
 * Quantizes angle to 256 steps
 * @param radians Angle in radians
 * @return Sine value (-1.0 to 1.0)
 */
inline float sinf_lookup(float radians) {
    return SIN_TABLE_FLOAT[rad_to_theta(radians)];
}

/**
 * Fast cosine for float angle (radians), using lookup
 * @param radians Angle in radians
 * @return Cosine value (-1.0 to 1.0)
 */
inline float cosf_lookup(float radians) {
    return SIN_TABLE_FLOAT[(uint8_t)(rad_to_theta(radians) + 64)];
}

} // namespace TrigLookup

#endif // TRIG_LOOKUP_H
