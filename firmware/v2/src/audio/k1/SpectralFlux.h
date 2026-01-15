/**
 * @file SpectralFlux.h
 * @brief Spectral Flux Calculator for K1 Audio Front-End (FFT-based)
 *
 * Computes half-wave rectified magnitude differences between consecutive frames.
 * Spectral flux detects energy onsets by summing positive changes across frequency spectrum.
 *
 * Features:
 * - Magnitude history (2 frames for delta calculation)
 * - Half-wave rectified flux: Σ max(0, current[k] - previous[k])
 * - Flux history buffer for adaptive threshold (40 frames, ~1.25s @ 31.25ms)
 * - Normalized flux output (0.0-1.0+)
 *
 * Algorithm:
 * For each frequency bin k:
 *   flux[frame] = Σ max(0, magnitude[k] - prevMagnitude[k])
 *
 * This captures energy onsets (sudden increases) while ignoring decreases,
 * making it ideal for beat detection.
 *
 * @author LightwaveOS Team
 * @version 1.0.0
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cmath>
#include "K1FFTConfig.h"

namespace lightwaveos {
namespace audio {
namespace k1 {

/**
 * @brief Spectral Flux Calculator
 *
 * Detects energy onsets by computing half-wave rectified magnitude differences.
 * Maintains flux history for adaptive onset threshold calculation.
 *
 * Thread Safety: Not thread-safe. Should be called from single thread only.
 */
class SpectralFlux {
public:
    /// Flux history size for adaptive threshold: 40 frames ≈ 1.25s @ 31.25ms/frame
    static constexpr uint16_t FLUX_HISTORY_SIZE = 40;

    /**
     * @brief Constructor
     */
    SpectralFlux();

    /**
     * @brief Process magnitude frame and calculate spectral flux
     *
     * Compares current magnitude spectrum against previous frame,
     * computing flux as sum of positive magnitude changes.
     *
     * @param magnitude Current magnitude spectrum (256 bins)
     * @return Scalar flux value (0.0 at minimum, can exceed 1.0 at onsets)
     */
    float process(const float magnitude[K1FFTConfig::MAGNITUDE_BINS]);

    /**
     * @brief Get most recent flux value
     * @return Latest computed flux (0.0-1.0+)
     */
    float getFlux() const {
        return m_currentFlux;
    }

    /**
     * @brief Get previous flux value
     * @return Flux from previous frame
     */
    float getPreviousFlux() const {
        return m_previousFlux;
    }

    /**
     * @brief Access flux history for statistics
     * @return Pointer to flux history array (40 values, newest at [0])
     */
    const float* getFluxHistory() const {
        return m_fluxHistory;
    }

    /**
     * @brief Get flux history size
     * @return Number of frames in history buffer
     */
    static constexpr uint16_t getFluxHistorySize() {
        return FLUX_HISTORY_SIZE;
    }

    /**
     * @brief Calculate flux statistics for adaptive threshold
     *
     * Computes median and standard deviation of flux history
     * for adaptive onset threshold: median + sensitivity * stddev
     *
     * @param[out] median Median flux value from history
     * @param[out] stddev Standard deviation of flux history
     */
    void getFluxStatistics(float& median, float& stddev) const;

    /**
     * @brief Reset calculator state
     *
     * Clears magnitude and flux history buffers.
     */
    void reset();

private:
    // Magnitude buffers
    float m_currentMagnitude[K1FFTConfig::MAGNITUDE_BINS];   ///< Current frame magnitude
    float m_previousMagnitude[K1FFTConfig::MAGNITUDE_BINS];  ///< Previous frame magnitude

    // Flux state
    float m_currentFlux;    ///< Latest computed flux value
    float m_previousFlux;   ///< Flux from previous frame

    // Flux history for adaptive threshold (circular buffer, newest at [0])
    float m_fluxHistory[FLUX_HISTORY_SIZE];  ///< History of flux values (40 frames)
    uint16_t m_historyIndex;                 ///< Write position in circular buffer
    bool m_historyFull;                      ///< Has history been filled at least once?

    /**
     * @brief Calculate magnitude difference between frames
     *
     * Half-wave rectified: flux = Σ max(0, current[k] - previous[k])
     *
     * @param current Current magnitude spectrum
     * @param previous Previous magnitude spectrum
     * @return Scalar flux value
     */
    float calculateFlux(const float current[K1FFTConfig::MAGNITUDE_BINS],
                        const float previous[K1FFTConfig::MAGNITUDE_BINS]) const;

    /**
     * @brief Add flux to history (circular buffer)
     *
     * Maintains last 40 flux values for statistics calculation.
     * Newest value is at index 0; oldest shifts out.
     *
     * @param flux New flux value to add
     */
    void addToHistory(float flux);
};

}  // namespace k1
}  // namespace audio
}  // namespace lightwaveos
