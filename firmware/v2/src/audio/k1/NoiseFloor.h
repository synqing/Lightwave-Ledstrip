/**
 * @file NoiseFloor.h
 * @brief Slow leaky-min noise floor estimator
 *
 * Freezes updates when clipping is detected to prevent noise floor
 * from rising during loud transients.
 *
 * @author LightwaveOS Team
 * @version 1.0.0 - K1 Migration
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <algorithm>

namespace lightwaveos {
namespace audio {
namespace k1 {

/**
 * @brief Noise floor estimator with clipping freeze
 *
 * Uses slow leaky-min to track background noise level.
 * Subtracts noise floor from magnitudes with configurable multiplier.
 */
class NoiseFloor {
public:
    /**
     * @brief Constructor
     */
    NoiseFloor();

    /**
     * @brief Initialize noise floor estimator
     *
     * @param num_bins Number of bins to track
     * @param k Multiplier for subtraction (default 1.5)
     * @param leak_rate Leaky-min update rate (default 0.999)
     */
    void init(size_t num_bins, float k = 1.5f, float leak_rate = 0.999f);

    /**
     * @brief Update noise floor from magnitudes
     *
     * @param mags Magnitude array (length num_bins)
     * @param is_clipping True if clipping detected (freezes updates)
     */
    void update(const float* mags, bool is_clipping);

    /**
     * @brief Subtract noise floor from magnitudes
     *
     * @param mags_in Input magnitude array
     * @param mags_out Output magnitude array (can be same as mags_in)
     * @param num_bins Number of bins
     */
    void subtract(const float* mags_in, float* mags_out, size_t num_bins) const;

    /**
     * @brief Get noise floor value for a bin
     *
     * @param bin_idx Bin index
     * @return Noise floor value
     */
    float getNoiseFloor(size_t bin_idx) const;

private:
    float* m_noiseFloor;                 ///< Per-bin noise floor estimates
    size_t m_numBins;                    ///< Number of bins
    float m_k;                           ///< Subtraction multiplier
    float m_leakRate;                    ///< Leaky-min update rate
    bool m_initialized;                   ///< Initialization flag
};

} // namespace k1
} // namespace audio
} // namespace lightwaveos

