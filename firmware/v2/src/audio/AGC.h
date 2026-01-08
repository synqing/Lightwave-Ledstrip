/**
 * @file AGC.h
 * @brief Automatic Gain Control for spectral normalization
 *
 * Provides adaptive gain control with attack/release time constants.
 * Normalizes input magnitudes to target level range (0.5-1.0).
 *
 * Design:
 * - Peak detection with exponential decay
 * - Attack/release time constants for smooth gain transitions
 * - Target level normalization to stabilize dynamic range
 *
 * Two usage modes:
 * 1. RhythmBank: Fast attack, slow release, no boost (attenuation-only)
 * 2. HarmonyBank: Moderate attack/release, mild boost allowed
 *
 * Memory: 3 floats (12 bytes) - very lightweight
 * Thread-safety: Single-threaded (caller must synchronize)
 *
 * Used by: RhythmBank and HarmonyBank for magnitude normalization
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
 * @brief Automatic Gain Control with attack/release dynamics
 *
 * Tracks peak magnitude and computes adaptive gain to normalize
 * output to target level. Prevents over-amplification of noise
 * while maintaining dynamic range for transients.
 *
 * Example usage:
 * @code
 * AGC agc(0.01f, 0.5f, 0.7f);  // 10ms attack, 500ms release, 0.7 target
 *
 * // Per-hop processing (delta_sec = 0.016 for 62.5 Hz)
 * agc.update(peakMagnitude, 0.016f);
 * float gain = agc.getGain();
 *
 * // Apply gain to magnitude array
 * for (int i = 0; i < numBins; i++) {
 *     magnitudes[i] *= gain;
 * }
 * @endcode
 */
class AGC {
public:
    /**
     * @brief Construct AGC with time constants
     *
     * @param attackTime Attack time in seconds (default 0.01s = 10ms)
     *                   Fast attack prevents clipping on transients
     * @param releaseTime Release time in seconds (default 0.5s = 500ms)
     *                    Slow release prevents pumping artifacts
     * @param targetLevel Target output level (default 0.7)
     *                    Gain computed to normalize peak to this level
     *
     * Time constants control gain smoothing via exponential decay:
     * alpha_attack = 1.0 - exp(-delta_sec / attackTime)
     * alpha_release = 1.0 - exp(-delta_sec / releaseTime)
     *
     * Recommended presets:
     * - RhythmBank: attackTime=0.01s, releaseTime=0.5s (fast/slow)
     * - HarmonyBank: attackTime=0.05s, releaseTime=0.3s (moderate)
     */
    AGC(float attackTime = 0.01f, float releaseTime = 0.5f, float targetLevel = 0.7f);

    /**
     * @brief Update gain based on peak magnitude
     *
     * Computes target gain to normalize peak to target level.
     * Applies attack/release smoothing to prevent abrupt changes.
     *
     * Algorithm:
     * 1. Update peak tracker with exponential decay
     * 2. Compute desired gain: targetLevel / peak
     * 3. Smooth gain with attack (decreasing) or release (increasing)
     *
     * @param peakMagnitude Current peak magnitude (typically max across bins)
     * @param dt Time since last update in seconds (e.g., 0.016s for 62.5 Hz)
     *
     * Preconditions:
     * - peakMagnitude >= 0.0
     * - dt > 0.0 and dt < 1.0 (sanity check)
     */
    void update(float peakMagnitude, float dt);

    /**
     * @brief Get current gain value
     *
     * @return Gain coefficient (multiply magnitudes by this)
     *         Range: 0.01 to maxGain (typically 1.0-2.0)
     */
    float getGain() const {
        return m_gain;
    }

    /**
     * @brief Reset AGC state
     *
     * Resets gain to 1.0 and peak to target level.
     * Use when switching audio sources or on silence detection.
     */
    void reset();

    /**
     * @brief Set maximum gain limit
     *
     * @param maxGain Maximum allowed gain (default 1.0 = no boost)
     *                Use 1.0 for attenuation-only (RhythmBank)
     *                Use 2.0 for mild boost (HarmonyBank)
     */
    void setMaxGain(float maxGain) {
        m_maxGain = maxGain;
    }

    /**
     * @brief Get maximum gain limit
     * @return Maximum gain coefficient
     */
    float getMaxGain() const {
        return m_maxGain;
    }

    /**
     * @brief Get current peak estimate
     * @return Peak magnitude tracker value
     */
    float getPeak() const {
        return m_peak;
    }

private:
    float m_gain;          ///< Current gain coefficient
    float m_peak;          ///< Peak magnitude tracker (exponential decay)
    float m_targetLevel;   ///< Target normalized level (0.5-1.0 range)
    float m_attackTime;    ///< Attack time constant (seconds)
    float m_releaseTime;   ///< Release time constant (seconds)
    float m_maxGain;       ///< Maximum gain limit (1.0 = attenuation-only)

    static constexpr float MIN_GAIN = 0.01f;   ///< Minimum gain to prevent zero
    static constexpr float MIN_PEAK = 1e-6f;   ///< Minimum peak to prevent div-by-zero
};

} // namespace audio
} // namespace lightwaveos
