/**
 * @file K1Utils.h
 * @brief Utility functions for K1-Lightwave integration
 *
 * Provides conversion functions between K1 beat tracker data formats
 * and Lightwave-Ledstrip v2 audio contracts.
 *
 * Key conversions:
 * - K1 z-score [-6, +6] <-> Lightwave flux [0, 1]
 * - K1 confidence -> effect intensity scaling
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include <algorithm>
#include <cmath>

namespace lightwaveos {
namespace audio {
namespace k1 {

/**
 * @brief K1 integration utility functions
 */
namespace K1Utils {

/**
 * @brief Convert K1 z-score to Lightwave flux
 *
 * K1 novelty uses MAD-normalized z-scores in [-6, +6] range.
 * Lightwave effects expect flux values in [0, 1] range.
 *
 * Mapping: z=-6 -> 0.0, z=0 -> 0.5, z=+6 -> 1.0
 *
 * @param z K1 novelty z-score [-6, +6]
 * @return Normalized flux [0, 1]
 */
inline float zScoreToFlux(float z) {
    return std::clamp((z + 6.0f) / 12.0f, 0.0f, 1.0f);
}

/**
 * @brief Convert Lightwave flux to K1 z-score
 *
 * Inverse of zScoreToFlux() for compatibility with K1 thresholds.
 *
 * @param flux Normalized flux [0, 1]
 * @return K1 z-score [-6, +6]
 */
inline float fluxToZScore(float flux) {
    return flux * 12.0f - 6.0f;
}

/**
 * @brief Map K1 confidence to effect intensity
 *
 * When confidence is low, effects should still be visible but muted.
 * When confidence is high, effects respond fully to beats.
 *
 * @param conf K1 confidence [0, 1]
 * @param minIntensity Minimum intensity when conf=0 (default 0.3)
 * @return Effect intensity [minIntensity, 1.0]
 */
inline float confidenceToIntensity(float conf, float minIntensity = 0.3f) {
    conf = std::clamp(conf, 0.0f, 1.0f);
    return minIntensity + (1.0f - minIntensity) * conf;
}

/**
 * @brief Convert K1 onset strength to beat strength
 *
 * K1 onset z-scores are typically in [0, 6] range for onsets.
 * This maps to [0, 1] with a soft knee for natural feel.
 *
 * @param z Onset z-score (positive values only)
 * @return Beat strength [0, 1]
 */
inline float onsetZToStrength(float z) {
    if (z <= 0.0f) return 0.0f;
    // Soft saturation: tanh-like curve, full strength at z=4
    float normalized = z / 4.0f;
    return std::clamp(normalized * normalized / (1.0f + normalized * normalized) * 2.0f, 0.0f, 1.0f);
}

/**
 * @brief Convert BPM to beat period in milliseconds
 *
 * @param bpm Beats per minute
 * @return Period in milliseconds
 */
inline float bpmToMs(float bpm) {
    if (bpm <= 0.0f) return 500.0f; // Default to 120 BPM
    return 60000.0f / bpm;
}

/**
 * @brief Convert beat period in milliseconds to BPM
 *
 * @param ms Period in milliseconds
 * @return Beats per minute
 */
inline float msToBpm(float ms) {
    if (ms <= 0.0f) return 120.0f; // Default
    return 60000.0f / ms;
}

/**
 * @brief Interpolate phase with wrap-around
 *
 * Smoothly interpolates between two phase values [0, 1) accounting
 * for wrap-around at the boundary.
 *
 * @param from Starting phase [0, 1)
 * @param to Target phase [0, 1)
 * @param t Interpolation factor [0, 1]
 * @return Interpolated phase [0, 1)
 */
inline float lerpPhase(float from, float to, float t) {
    // Handle wrap-around: choose shorter path
    float diff = to - from;
    if (diff > 0.5f) diff -= 1.0f;
    if (diff < -0.5f) diff += 1.0f;

    float result = from + diff * t;
    if (result < 0.0f) result += 1.0f;
    if (result >= 1.0f) result -= 1.0f;
    return result;
}

} // namespace K1Utils

} // namespace k1
} // namespace audio
} // namespace lightwaveos
