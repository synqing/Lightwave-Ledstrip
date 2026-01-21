/**
 * @file NoveltyFlux.h
 * @brief Spectral flux computation for onset detection
 *
 * Computes spectral flux (magnitude change) between frames with half-wave rectification.
 * Used as primary onset detection feature for tempo tracking.
 *
 * Part of Phase 2C: Feature Extraction Pipeline
 */

#pragma once

#include <cstdint>

// Forward declaration (replaced with full include for getThreshold() access)
namespace lightwaveos {
namespace audio {
class NoiseFloor;
}
}

// Include full header for method access
#include "NoiseFloor.h"

// Bring NoiseFloor into global namespace for backward compatibility
using lightwaveos::audio::NoiseFloor;

/**
 * @class NoveltyFlux
 * @brief Computes spectral flux from magnitude spectrum
 *
 * Spectral flux measures the rate of change in the magnitude spectrum.
 * Half-wave rectification ensures only increasing energy contributes to onsets.
 *
 * Algorithm:
 * 1. For each bin: delta = currentMag - prevMag
 * 2. If delta > noiseThreshold: accumulate delta (half-wave rectification)
 * 3. Normalize by number of bins
 *
 * Typical usage:
 * - Harmonic bins (200-2000 Hz): Beat/onset detection
 * - Percussive bins (2000-8000 Hz): Transient detection
 */
class NoveltyFlux {
public:
    /**
     * @brief Construct novelty flux calculator
     * @param numBins Number of frequency bins to track
     */
    explicit NoveltyFlux(uint8_t numBins);

    /**
     * @brief Destructor - frees magnitude history
     */
    ~NoveltyFlux();

    /**
     * @brief Compute spectral flux for current frame
     * @param currentMags Magnitude spectrum (numBins floats)
     * @param noiseFloor Noise floor estimator for thresholding
     * @return Normalized flux value [0, inf) - typical range [0, 2]
     *
     * Returns 0.0 on first call (no previous frame).
     */
    float compute(const float* currentMags, const NoiseFloor& noiseFloor);

    /**
     * @brief Reset state (clears magnitude history)
     */
    void reset();

private:
    float* m_prevMags;      ///< Previous frame magnitudes for delta computation
    uint8_t m_numBins;      ///< Number of frequency bins
    bool m_initialized;     ///< False until first frame processed
};
