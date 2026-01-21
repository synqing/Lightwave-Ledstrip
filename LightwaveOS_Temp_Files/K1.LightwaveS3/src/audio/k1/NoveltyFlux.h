/**
 * @file NoveltyFlux.h
 * @brief Half-wave rectified spectral flux on rhythm bins
 *
 * Normalized by running baseline (scale-invariant).
 * Used for onset detection in tempo tracking.
 *
 * @author LightwaveOS Team
 * @version 1.0.0 - K1 Migration
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include "K1Spec.h"

namespace lightwaveos {
namespace audio {
namespace k1 {

/**
 * @brief Novelty flux calculator
 *
 * Computes half-wave rectified spectral flux on rhythm bins only.
 * Normalizes by running baseline for scale-invariance.
 */
class NoveltyFlux {
public:
    /**
     * @brief Constructor
     */
    NoveltyFlux();

    /**
     * @brief Initialize novelty flux calculator
     */
    void init();

    /**
     * @brief Update and compute novelty flux
     *
     * @param rhythm_bins Current rhythm bin magnitudes (length RHYTHM_BINS)
     * @return Novelty flux value (normalized, scale-invariant)
     */
    float update(const float* rhythm_bins);

    /**
     * @brief Reset state
     */
    void reset();

private:
    float m_prevBins[RHYTHM_BINS];       ///< Previous frame magnitudes
    float m_baseline;                     ///< Running baseline for normalization
    float m_baselineAlpha;                ///< Baseline EMA coefficient (default 0.99)
    bool m_initialized;                   ///< Initialization flag
};

} // namespace k1
} // namespace audio
} // namespace lightwaveos

