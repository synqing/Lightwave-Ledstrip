/**
 * @file AudioMath.h
 * @brief Frame-rate-independent EMA alpha computation utilities.
 *
 * Converts between time constants (seconds) and per-frame EMA alphas,
 * enabling a single set of perceptual tuning values to work across
 * any hop rate (50 Hz, 100 Hz, 125 Hz, etc.).
 *
 * Usage:
 *   float alpha = computeEmaAlpha(0.2f, 125.0f);  // 200ms tau @ 125 Hz
 *   smoothed = smoothed + alpha * (raw - smoothed); // standard EMA
 */

#pragma once

#include <cmath>

namespace lightwaveos::audio {

/**
 * @brief Compute EMA alpha from a time constant and frame rate.
 *
 * alpha = 1 - exp(-1 / (tau_seconds * hop_rate_hz))
 *
 * @param tau_seconds  Time constant in seconds (63% settling time)
 * @param hop_rate_hz  Frame/hop rate in Hz
 * @return Per-frame alpha in [0, 1]
 */
inline float computeEmaAlpha(float tau_seconds, float hop_rate_hz) {
    if (tau_seconds <= 0.0f || hop_rate_hz <= 0.0f) return 1.0f;
    return 1.0f - expf(-1.0f / (tau_seconds * hop_rate_hz));
}

/**
 * @brief Reverse-engineer the time constant from an existing alpha and frame rate.
 *
 * tau = -1 / (hop_rate_hz * ln(1 - alpha))
 *
 * Useful for documenting what time constant a hardcoded alpha corresponds to.
 *
 * @param alpha        Per-frame EMA alpha in (0, 1)
 * @param hop_rate_hz  Frame/hop rate in Hz
 * @return Time constant in seconds
 */
inline float tauFromAlpha(float alpha, float hop_rate_hz) {
    if (alpha <= 0.0f || alpha >= 1.0f || hop_rate_hz <= 0.0f) return 0.0f;
    return -1.0f / (hop_rate_hz * logf(1.0f - alpha));
}

/**
 * @brief Compute EMA alpha from half-life in frames.
 *
 * alpha = 1 - exp(-ln(2) / half_life_frames)
 *
 * @param half_life_frames  Number of frames for signal to decay to 50%
 * @return Per-frame alpha in [0, 1]
 */
inline float alphaFromHalfLife(float half_life_frames) {
    if (half_life_frames <= 0.0f) return 1.0f;
    return 1.0f - expf(-0.693147f / half_life_frames);
}

/**
 * @brief Re-scale an alpha tuned at one frame rate to another.
 *
 * Preserves the perceptual time constant across frame rates.
 *
 * @param alpha_at_ref  Original alpha value
 * @param ref_hz        Frame rate the original alpha was tuned for
 * @param target_hz     Frame rate to retune to
 * @return Retuned alpha for target_hz
 */
inline float retunedAlpha(float alpha_at_ref, float ref_hz, float target_hz) {
    float tau = tauFromAlpha(alpha_at_ref, ref_hz);
    return computeEmaAlpha(tau, target_hz);
}

} // namespace lightwaveos::audio
