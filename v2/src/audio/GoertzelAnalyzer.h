#pragma once
#include "config/features.h"
#if FEATURE_AUDIO_SYNC

#include <cstdint>
#include <cstddef>

namespace lightwaveos {
namespace audio {

/**
 * @brief Goertzel frequency analyzer for 8 frequency bands with sliding window
 *
 * SLIDING WINDOW DESIGN (for two-rate pipeline):
 * - Window size: 512 samples (32ms for bass coherence)
 * - Hop size: 128 samples (8ms updates for fast texture)
 * - Overlap: 75% (512 - 128 = 384 samples overlap)
 *
 * This gives us:
 * - Fast updates (125 Hz) for responsive visual texture
 * - Long integration (32ms) for stable bass detection
 *
 * The ring buffer always maintains a full 512-sample window.
 * After the first 512 samples are accumulated, analyze() can be called
 * every 128 samples (after each accumulate() of a hop).
 *
 * Target frequencies at 16kHz sample rate:
 * - Band 0: 60 Hz (sub-bass)
 * - Band 1: 120 Hz (bass)
 * - Band 2: 250 Hz (low-mid)
 * - Band 3: 500 Hz (mid)
 * - Band 4: 1000 Hz (high-mid)
 * - Band 5: 2000 Hz (presence)
 * - Band 6: 4000 Hz (brilliance)
 * - Band 7: 7800 Hz (air - slightly below Nyquist)
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
     * @brief Accumulate audio samples into the sliding window (ring buffer)
     * @param samples Pointer to int16_t PCM samples
     * @param count Number of samples to add (typically HOP_FAST = 128)
     *
     * Samples are added to the ring buffer. After the first WINDOW_SIZE samples,
     * the window is always "full" and analyze() can be called after each hop.
     */
    void accumulate(const int16_t* samples, size_t count);

    /**
     * @brief Compute frequency band magnitudes from current window
     * @param bandsOut Output array of 8 floats for band magnitudes [0,1]
     * @return true if window is ready (has >= WINDOW_SIZE samples), false otherwise
     *
     * After the first 512 samples, this always returns true.
     * Can be called every hop (128 samples) for fast updates with 75% overlap.
     */
    bool analyze(float* bandsOut);

    /**
     * @brief Check if window has enough samples for analysis
     */
    bool isReady() const { return m_windowReady; }

    /**
     * @brief Reset the ring buffer to start fresh
     */
    void reset();

    /**
     * @brief Get total samples accumulated (for debugging)
     */
    size_t getTotalSamples() const { return m_totalSamples; }

private:
    // Target frequencies for 8 bands
    // Note: Band 7 set to 7800Hz to avoid Goertzel instability at Nyquist (8000Hz)
    static constexpr float TARGET_FREQS[NUM_BANDS] = {
        60.0f, 120.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 7800.0f
    };

    // Ring buffer for sliding window
    int16_t m_ringBuffer[WINDOW_SIZE];
    size_t m_writeIndex = 0;       // Next write position in ring buffer
    size_t m_totalSamples = 0;     // Total samples written (for startup tracking)
    bool m_windowReady = false;    // True after first WINDOW_SIZE samples

    // Temporary contiguous buffer for Goertzel computation
    // (Goertzel needs contiguous samples, ring buffer may wrap)
    int16_t m_contiguousBuffer[WINDOW_SIZE];

    // Precomputed Goertzel coefficients (2 * cos(2π * f/Fs))
    float m_coefficients[NUM_BANDS];

    // Normalization factors (to convert magnitudes to [0,1])
    float m_normFactors[NUM_BANDS];

    /**
     * @brief Copy ring buffer to contiguous buffer for Goertzel
     *
     * Handles wrap-around by copying in correct order:
     * oldest samples first (from writeIndex to end, then start to writeIndex)
     */
    void copyToContiguous();

    /**
     * @brief Compute Goertzel magnitude for a single frequency
     * @param buffer Input samples (contiguous)
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
