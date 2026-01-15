/**
 * @file NoiseFloor.h
 * @brief Adaptive noise floor tracker for per-bin thresholding
 *
 * Tracks exponentially-weighted moving average of magnitude per frequency bin
 * to establish adaptive noise floor. Used for onset detection and peak gating
 * in tempo tracking.
 *
 * Design:
 * - Per-bin EMA with configurable time constant (~1.0s default)
 * - Magnitude-based thresholding (X dB above floor)
 * - Prevents onset contamination from ambient noise
 *
 * Used by: NoveltyFlux for spectral flux onset detection (Phase 2C)
 *
 * Memory: numBins * sizeof(float) bytes (heap-allocated)
 * Thread-safety: Single-threaded (caller must synchronize)
 *
 * @author LightwaveOS Team
 * @version 2.0.0 - Tempo Tracking Dual-Bank Architecture
 */

#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace lightwaveos {
namespace audio {

/**
 * @brief Adaptive noise floor estimator for frequency bins
 *
 * Maintains per-bin exponential moving average of magnitude to track
 * ambient noise floor. Provides thresholding for onset detection.
 *
 * Example usage:
 * @code
 * NoiseFloor floor(1.0f, 24);  // 1s time constant, 24 bins
 * for (int i = 0; i < 24; i++) {
 *     floor.update(i, magnitudes[i]);
 *     if (floor.isAboveFloor(i, magnitudes[i], 2.0f)) {
 *         // Magnitude is 2× above noise floor - potential onset
 *     }
 * }
 * @endcode
 */
class NoiseFloor {
public:
    /**
     * @brief Construct noise floor tracker
     *
     * @param timeConstant Time constant in seconds (default 1.0s)
     *                     Controls adaptation speed: higher = slower
     * @param numBins Number of frequency bins to track
     *
     * Alpha decay factor is computed as:
     * alpha = 1.0 - exp(-hopDuration / timeConstant)
     *
     * For 62.5 Hz hop rate (16ms hop) and 1.0s time constant:
     * alpha ≈ 0.016 (1.6% new value, 98.4% history)
     */
    NoiseFloor(float timeConstant = 1.0f, uint8_t numBins = 24);

    /**
     * @brief Destructor - releases heap memory
     */
    ~NoiseFloor();

    /**
     * @brief Update noise floor estimate for a bin
     *
     * Applies exponential moving average:
     * floor[bin] = (1 - alpha) * floor[bin] + alpha * magnitude
     *
     * @param bin Bin index (0 to numBins-1)
     * @param magnitude Current magnitude value (non-negative)
     *
     * Preconditions:
     * - bin < numBins
     * - magnitude >= 0.0
     */
    void update(uint8_t bin, float magnitude);

    /**
     * @brief Get current noise floor estimate for a bin
     *
     * @param bin Bin index (0 to numBins-1)
     * @return Current floor estimate (non-negative)
     */
    float getFloor(uint8_t bin) const;

    /**
     * @brief Get threshold value for onset detection
     *
     * Returns floor * multiplier for onset detection comparison.
     * Legacy compatibility wrapper for NoveltyFlux.
     *
     * @param bin Bin index (0 to numBins-1)
     * @param multiplier Threshold multiplier (default 2.0)
     * @return Threshold value (floor * multiplier)
     */
    float getThreshold(uint8_t bin, float multiplier = 2.0f) const;

    /**
     * @brief Check if magnitude exceeds noise floor by threshold
     *
     * Compares magnitude against floor with multiplicative threshold:
     * magnitude > floor * threshold
     *
     * @param bin Bin index (0 to numBins-1)
     * @param magnitude Current magnitude
     * @param threshold Multiplier (default 2.0 = 6 dB above floor)
     * @return True if magnitude exceeds floor * threshold
     *
     * Example:
     * - threshold = 2.0: Magnitude must be 2× floor (6 dB)
     * - threshold = 1.5: Magnitude must be 1.5× floor (3.5 dB)
     */
    bool isAboveFloor(uint8_t bin, float magnitude, float threshold = 2.0f) const;

    /**
     * @brief Reset all bins to initial state
     *
     * Sets all floor values to a small epsilon to prevent divide-by-zero.
     */
    void reset();

    /**
     * @brief Get number of bins
     * @return Number of bins tracked
     */
    uint8_t getNumBins() const {
        return m_numBins;
    }

    /**
     * @brief Get alpha decay factor
     * @return EMA alpha coefficient
     */
    float getAlpha() const {
        return m_alpha;
    }

private:
    float* m_floor;           ///< Per-bin noise floor estimates (heap-allocated)
    uint8_t m_numBins;        ///< Number of frequency bins
    float m_alpha;            ///< EMA decay factor (0 to 1)
    float m_timeConstant;     ///< Time constant in seconds

    static constexpr float MIN_FLOOR = 1e-6f;  ///< Minimum floor to prevent zero
};

} // namespace audio
} // namespace lightwaveos
