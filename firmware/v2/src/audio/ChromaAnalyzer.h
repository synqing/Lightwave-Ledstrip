/**
 * @file ChromaAnalyzer.h
 * @brief 12-bin chromagram analyzer using Goertzel algorithm
 *
 * Computes pitch-class energy by analyzing note frequencies across octaves
 * and folding them into 12 pitch classes (C, C#, D, D#, E, F, F#, G, G#, A, A#, B).
 *
 * Uses the same 512-sample window as GoertzelAnalyzer for consistency.
 */

#pragma once

#include "config/features.h"

#if FEATURE_AUDIO_SYNC

#include <cstdint>
#include <cstddef>

namespace lightwaveos {
namespace audio {

/**
 * @brief Chromagram analyzer for 12 pitch classes
 *
 * Accumulates 512 samples (2 hops at 256 samples each) and computes
 * chromagram by analyzing note frequencies and folding into 12 pitch classes.
 */
class ChromaAnalyzer {
public:
    static constexpr size_t WINDOW_SIZE = 512;
    static constexpr uint8_t NUM_CHROMA = 12;
    static constexpr uint8_t NUM_OCTAVES = 4;  // Analyze 4 octaves (48 notes total)
    static constexpr uint32_t SAMPLE_RATE_HZ = 12800;  // FIX: Match I2S sample rate (was 16000!)

    /**
     * @brief Constructor - precomputes Goertzel coefficients for note frequencies
     */
    ChromaAnalyzer();

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
     * @brief Compute chromagram (12 pitch classes)
     * @param chromaOut Output array of 12 floats for pitch class magnitudes [0,1]
     * @return true if new results are ready (window is full), false otherwise
     *
     * When returning true, chromaOut is filled with normalized magnitudes.
     * When returning false, chromaOut is unchanged and caller should reuse previous values.
     */
    bool analyze(float* chromaOut);

    /**
     * @brief Compute chromagram on an explicit window buffer
     * @param window Pointer to WINDOW_SIZE samples (contiguous)
     * @param N Number of samples (must be WINDOW_SIZE)
     * @param chromaOut Output array of 12 floats [0,1]
     * @return true if computation succeeded, false otherwise
     */
    bool analyzeWindow(const int16_t* window, size_t N, float* chromaOut);

    /**
     * @brief Reset the accumulator to start fresh
     */
    void reset();

private:
    // Note frequencies (equal temperament, A4=440Hz)
    // 4 octaves × 12 notes = 48 frequencies
    // Octave 2: 55-104 Hz, Octave 3: 110-208 Hz, Octave 4: 220-415 Hz, Octave 5: 440-830 Hz
    static constexpr float NOTE_FREQS[NUM_OCTAVES * NUM_CHROMA] = {
        // Octave 2 (A2=110Hz, but we start from C2=65.4Hz)
        65.41f, 69.30f, 73.42f, 77.78f, 82.41f, 87.31f, 92.50f, 98.00f, 103.83f, 110.00f, 116.54f, 123.47f,
        // Octave 3
        130.81f, 138.59f, 146.83f, 155.56f, 164.81f, 174.61f, 185.00f, 196.00f, 207.65f, 220.00f, 233.08f, 246.94f,
        // Octave 4
        261.63f, 277.18f, 293.66f, 311.13f, 329.63f, 349.23f, 369.99f, 392.00f, 415.30f, 440.00f, 466.16f, 493.88f,
        // Octave 5
        523.25f, 554.37f, 587.33f, 622.25f, 659.25f, 698.46f, 739.99f, 783.99f, 830.61f, 880.00f, 932.33f, 987.77f
    };

    // Accumulation buffer (rolling window)
    int16_t m_accumBuffer[WINDOW_SIZE];
    size_t m_accumIndex = 0;
    bool m_windowFull = false;

    // Precomputed Goertzel coefficients for all note frequencies
    float m_coefficients[NUM_OCTAVES * NUM_CHROMA];

    // Normalization factors (to convert magnitudes to [0,1])
    float m_normFactors[NUM_OCTAVES * NUM_CHROMA];

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
     * @param windowSize Window size in samples
     * @return Coefficient value (2 * cos(2π * k/N))
     */
    static float computeCoefficient(float targetFreq, uint32_t sampleRate, size_t windowSize);
};

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
