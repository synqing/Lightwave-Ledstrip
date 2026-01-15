/**
 * @file AGC.h
 * @brief Automatic Gain Control (separate instances per bank)
 *
 * Rhythm: attenuation-only (never boosts), slow release
 * Harmony: mild boost allowed, capped
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
 * @brief AGC mode
 */
enum class AGCMode {
    Rhythm,     ///< Attenuation-only, slow release
    Harmony     ///< Mild boost allowed, capped
};

/**
 * @brief Automatic Gain Control
 *
 * Separate instances for Rhythm and Harmony banks with different behaviors.
 */
class AGC {
public:
    /**
     * @brief Constructor
     */
    AGC();

    /**
     * @brief Initialize AGC
     *
     * @param num_bins Number of bins
     * @param mode AGC mode (Rhythm or Harmony)
     */
    void init(size_t num_bins, AGCMode mode);

    /**
     * @brief Process magnitudes through AGC
     *
     * @param mags_in Input magnitude array
     * @param mags_out Output magnitude array (can be same as mags_in)
     * @param num_bins Number of bins
     */
    void process(const float* mags_in, float* mags_out, size_t num_bins);

    /**
     * @brief Get current gain
     */
    float getGain() const { return m_gain; }

private:
    float* m_targetLevel;                ///< Per-bin target levels
    size_t m_numBins;                    ///< Number of bins
    AGCMode m_mode;                      ///< AGC mode
    float m_gain;                        ///< Current gain
    float m_attackRate;                   ///< Attack rate
    float m_releaseRate;                  ///< Release rate
    float m_maxGain;                     ///< Maximum gain (1.0 for Rhythm, >1.0 for Harmony)
    bool m_initialized;                   ///< Initialization flag

    /**
     * @brief Update gain based on input levels
     *
     * @param mags Magnitude array
     * @param num_bins Number of bins
     */
    void updateGain(const float* mags, size_t num_bins);
};

} // namespace k1
} // namespace audio
} // namespace lightwaveos

