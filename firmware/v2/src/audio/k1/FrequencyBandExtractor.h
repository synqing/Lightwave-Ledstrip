/**
 * @file FrequencyBandExtractor.h
 * @brief Frequency Band Energy Extraction from FFT Magnitude Spectrum
 *
 * Extracts bass, mid, and high frequency band energy from 256-bin magnitude spectrum.
 * Supports both simple energy summation and frequency mapping to legacy arrays
 * (mag_rhythm[24] and mag_harmony[64]) for backward compatibility.
 *
 * Frequency Band Definitions (at 16kHz, 31.25 Hz/bin):
 * - Bass: 20-200 Hz (bins 1-6)
 * - Rhythm: 60-600 Hz (bins 2-19, for mag_rhythm compatibility)
 * - Mid/Harmony: 200-2000 Hz (bins 6-64, for mag_harmony compatibility)
 * - High: 6000-20000 Hz (bins 192-256)
 *
 * Usage:
 * ```cpp
 * FrequencyBandExtractor extractor;
 * float bassEnergy = extractor.getBassEnergy(magnitude);
 * float midEnergy = extractor.getMidEnergy(magnitude);
 * float highEnergy = extractor.getHighEnergy(magnitude);
 * ```
 *
 * @author LightwaveOS Team
 * @version 1.0.0
 */

#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>
#include "K1FFTConfig.h"

namespace lightwaveos {
namespace audio {
namespace k1 {

/**
 * @brief Frequency Band Energy Extractor
 *
 * Static utility class for extracting and mapping frequency band energy
 * from FFT magnitude spectrum. No state, no instantiation required.
 *
 * Thread Safe: Yes, all methods are pure functions with no side effects.
 */
class FrequencyBandExtractor {
public:
    /**
     * @brief Get bass band energy (20-200 Hz)
     *
     * Sums magnitude values in bass frequency range.
     *
     * @param magnitude 256-bin magnitude spectrum from FFT
     * @return Sum of bass band magnitudes
     */
    static inline float getBassEnergy(const float magnitude[K1FFTConfig::MAGNITUDE_BINS]) {
        return getMagnitudeRange(magnitude,
                                K1FFTConfig::BASS_BIN_START,
                                K1FFTConfig::BASS_BIN_END);
    }

    /**
     * @brief Get rhythm band energy (60-600 Hz)
     *
     * Sums magnitude values in rhythm range (used for beat detection).
     * Subset: ~18 frequency bins stretched to mag_rhythm[24] in legacy format.
     *
     * @param magnitude 256-bin magnitude spectrum from FFT
     * @return Sum of rhythm band magnitudes
     */
    static inline float getRhythmEnergy(const float magnitude[K1FFTConfig::MAGNITUDE_BINS]) {
        return getMagnitudeRange(magnitude,
                                K1FFTConfig::RHYTHM_BIN_START,
                                K1FFTConfig::RHYTHM_BIN_END);
    }

    /**
     * @brief Get mid/harmony band energy (200-2000 Hz)
     *
     * Sums magnitude values in mid-frequency harmonic range.
     * Subset: ~59 frequency bins stretched to mag_harmony[64] in legacy format.
     *
     * @param magnitude 256-bin magnitude spectrum from FFT
     * @return Sum of mid/harmony band magnitudes
     */
    static inline float getMidEnergy(const float magnitude[K1FFTConfig::MAGNITUDE_BINS]) {
        return getMagnitudeRange(magnitude,
                                K1FFTConfig::MID_BIN_START,
                                K1FFTConfig::MID_BIN_END);
    }

    /**
     * @brief Get high band energy (6000-20000 Hz)
     *
     * Sums magnitude values in high-frequency range.
     *
     * @param magnitude 256-bin magnitude spectrum from FFT
     * @return Sum of high band magnitudes
     */
    static inline float getHighEnergy(const float magnitude[K1FFTConfig::MAGNITUDE_BINS]) {
        return getMagnitudeRange(magnitude,
                                K1FFTConfig::HIGH_BIN_START,
                                K1FFTConfig::HIGH_BIN_END);
    }

    /**
     * @brief Get total spectral energy across all bands
     *
     * Sums all magnitude values (useful for normalization).
     *
     * @param magnitude 256-bin magnitude spectrum from FFT
     * @return Sum of all magnitudes
     */
    static inline float getTotalEnergy(const float magnitude[K1FFTConfig::MAGNITUDE_BINS]) {
        float total = 0.0f;
        for (uint16_t k = 0; k < K1FFTConfig::MAGNITUDE_BINS; k++) {
            total += magnitude[k];
        }
        return total;
    }

    /**
     * @brief Get magnitude sum in frequency range
     *
     * Helper function for band energy extraction.
     *
     * @param magnitude 256-bin magnitude spectrum
     * @param startBin Start bin (inclusive)
     * @param endBin End bin (inclusive)
     * @return Sum of magnitudes in range
     */
    static inline float getMagnitudeRange(const float magnitude[K1FFTConfig::MAGNITUDE_BINS],
                                          uint16_t startBin,
                                          uint16_t endBin) {
        float sum = 0.0f;
        if (startBin >= K1FFTConfig::MAGNITUDE_BINS) {
            return 0.0f;
        }
        endBin = std::min(endBin, (uint16_t)(K1FFTConfig::MAGNITUDE_BINS - 1));
        for (uint16_t i = startBin; i <= endBin; i++) {
            sum += magnitude[i];
        }
        return sum;
    }

    /**
     * @brief Map FFT magnitude to legacy rhythm array
     *
     * Extracts 18 rhythm bins (RHYTHM_BIN_START to RHYTHM_BIN_END) and
     * stretches/maps them to mag_rhythm[24] for backward compatibility.
     *
     * @param magnitude 256-bin magnitude spectrum from FFT
     * @param[out] rhythmArray Output array (must be 24 elements)
     */
    static inline void mapToRhythmArray(const float magnitude[K1FFTConfig::MAGNITUDE_BINS],
                                        float rhythmArray[24]) {
        const uint16_t rhythmBins = K1FFTConfig::RHYTHM_BIN_END - K1FFTConfig::RHYTHM_BIN_START + 1;
        const float mapRatio = static_cast<float>(rhythmBins) / 24.0f;

        for (uint16_t i = 0; i < 24; i++) {
            uint16_t binStart = K1FFTConfig::RHYTHM_BIN_START + static_cast<uint16_t>(i * mapRatio);
            uint16_t binEnd = K1FFTConfig::RHYTHM_BIN_START + static_cast<uint16_t>((i + 1) * mapRatio);
            binEnd = std::min(binEnd, (uint16_t)(K1FFTConfig::RHYTHM_BIN_END + 1));

            float sum = 0.0f;
            for (uint16_t k = binStart; k < binEnd; k++) {
                sum += magnitude[k];
            }
            rhythmArray[i] = sum;
        }
    }

    /**
     * @brief Map FFT magnitude to legacy harmony array
     *
     * Extracts ~59 mid bins (MID_BIN_START to MID_BIN_END) and
     * stretches/maps them to mag_harmony[64] for backward compatibility.
     *
     * @param magnitude 256-bin magnitude spectrum from FFT
     * @param[out] harmonyArray Output array (must be 64 elements)
     */
    static inline void mapToHarmonyArray(const float magnitude[K1FFTConfig::MAGNITUDE_BINS],
                                         float harmonyArray[64]) {
        const uint16_t midBins = K1FFTConfig::MID_BIN_END - K1FFTConfig::MID_BIN_START + 1;
        const float mapRatio = static_cast<float>(midBins) / 64.0f;

        for (uint16_t i = 0; i < 64; i++) {
            uint16_t binStart = K1FFTConfig::MID_BIN_START + static_cast<uint16_t>(i * mapRatio);
            uint16_t binEnd = K1FFTConfig::MID_BIN_START + static_cast<uint16_t>((i + 1) * mapRatio);
            binEnd = std::min(binEnd, (uint16_t)(K1FFTConfig::MID_BIN_END + 1));

            float sum = 0.0f;
            for (uint16_t k = binStart; k < binEnd; k++) {
                sum += magnitude[k];
            }
            harmonyArray[i] = sum;
        }
    }

private:
    FrequencyBandExtractor() = delete;  // Static utility class, cannot instantiate
};

}  // namespace k1
}  // namespace audio
}  // namespace lightwaveos
