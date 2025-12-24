#pragma once
#include "config/features.h"
#if FEATURE_AUDIO_SYNC

#include <cstdint>
#include <cstddef>

namespace lightwaveos {
namespace audio {

/**
 * @brief Goertzel frequency analyzer for 8 frequency bands
 *
 * Accumulates 512 samples (2 hops at 256 samples each) for bass coherence,
 * then computes magnitude for each target frequency using the Goertzel algorithm.
 *
 * Target frequencies at 16kHz sample rate:
 * - Band 0: 60 Hz (sub-bass)
 * - Band 1: 120 Hz (bass)
 * - Band 2: 250 Hz (low-mid)
 * - Band 3: 500 Hz (mid)
 * - Band 4: 1000 Hz (high-mid)
 * - Band 5: 2000 Hz (presence)
 * - Band 6: 4000 Hz (brilliance)
 * - Band 7: 8000 Hz (air)
 */
class GoertzelAnalyzer {
public:
    static constexpr size_t WINDOW_SIZE = 512;
    static constexpr uint8_t NUM_BANDS = 8;
    static constexpr uint32_t SAMPLE_RATE_HZ = 16000;

    /**
     * @brief Constructor - precomputes Goertzel coefficients
     */
    GoertzelAnalyzer();

    /**
     * @brief Accumulate audio samples into the analysis window
     * @param samples Pointer to int16_t PCM samples
     * @param count Number of samples to add
     *
     * Samples are added to the rolling window. When 512 samples are accumulated,
     * the next call to analyze() will compute fresh results.
     */
    void accumulate(const int16_t* samples, size_t count);

    /**
     * @brief Compute frequency band magnitudes
     * @param bandsOut Output array of 8 floats for band magnitudes [0,1]
     * @return true if new results are ready (window is full), false otherwise
     *
     * When returning true, bandsOut is filled with normalized magnitudes.
     * When returning false, bandsOut is unchanged and caller should reuse previous values.
     */
    bool analyze(float* bandsOut);

    /**
     * @brief Reset the accumulator to start fresh
     */
    void reset();

private:
    // Target frequencies for 8 bands
    // Note: Band 7 set to 7800Hz to avoid Goertzel instability at Nyquist (8000Hz)
    static constexpr float TARGET_FREQS[NUM_BANDS] = {
        60.0f, 120.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 7800.0f
    };

    // Accumulation buffer (rolling window)
    int16_t m_accumBuffer[WINDOW_SIZE];
    size_t m_accumIndex = 0;
    bool m_windowFull = false;

    // Precomputed Goertzel coefficients (2 * cos(2π * f/Fs))
    float m_coefficients[NUM_BANDS];

    // Normalization factors (to convert magnitudes to [0,1])
    float m_normFactors[NUM_BANDS];

    /**
     * @brief Compute Goertzel magnitude for a single frequency
     * @param buffer Input samples
     * @param N Number of samples
     * @param coeff Precomputed Goertzel coefficient
     * @return Raw magnitude (before normalization)
     */
    float computeGoertzel(const int16_t* buffer, size_t N, float coeff) const;

    /**
     * @brief Precompute Goertzel coefficient for a target frequency
     * @param targetFreq Frequency in Hz
     * @param sampleRate Sample rate in Hz
     * @return Coefficient value (2 * cos(2π * k/N))
     */
    static float computeCoefficient(float targetFreq, uint32_t sampleRate, size_t windowSize);
};

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
