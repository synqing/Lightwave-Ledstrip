/**
 * @file GoertzelAnalyzer.cpp
 * @brief Goertzel frequency analyzer with sliding window implementation
 *
 * Sliding window design:
 * - Ring buffer maintains 512 samples
 * - After startup, always has full window ready
 * - Can analyze after every 128-sample hop (75% overlap)
 * - Provides fast updates (125 Hz) with long integration (32ms)
 */

#include "GoertzelAnalyzer.h"

#if FEATURE_AUDIO_SYNC

#include <cmath>
#include <cstring>
#include <algorithm>

namespace lightwaveos {
namespace audio {

// Define static constexpr arrays (C++17 requires definition)
constexpr float GoertzelAnalyzer::TARGET_FREQS[NUM_BANDS];

GoertzelAnalyzer::GoertzelAnalyzer() {
    // Precompute Goertzel coefficients for all target frequencies
    for (uint8_t i = 0; i < NUM_BANDS; ++i) {
        m_coefficients[i] = computeCoefficient(TARGET_FREQS[i], SAMPLE_RATE_HZ, WINDOW_SIZE);
    }

    // Precompute normalization factors
    // Adjusted to map half-scale sine waves (amp=16000) to > 0.3
    // Theoretical max raw magnitude for full scale is N/2 = 256
    // We use uniform normalization for now to ensure flat frequency response
    float uniformFactor = 1.0f / 250.0f;
    for (uint8_t i = 0; i < NUM_BANDS; ++i) {
        m_normFactors[i] = uniformFactor;
    }

    // Initialize ring buffer to zero
    std::memset(m_ringBuffer, 0, sizeof(m_ringBuffer));
    std::memset(m_contiguousBuffer, 0, sizeof(m_contiguousBuffer));
}

float GoertzelAnalyzer::computeCoefficient(float targetFreq, uint32_t sampleRate, size_t windowSize) {
    // Compute normalized frequency k (bin index)
    // k = (targetFreq / sampleRate) * windowSize
    float k = (targetFreq / static_cast<float>(sampleRate)) * static_cast<float>(windowSize);

    // Goertzel coefficient: 2 * cos(2π * k / N)
    float omega = (2.0f * M_PI * k) / static_cast<float>(windowSize);
    return 2.0f * std::cos(omega);
}

void GoertzelAnalyzer::accumulate(const int16_t* samples, size_t count) {
    // Write samples into ring buffer
    for (size_t i = 0; i < count; ++i) {
        m_ringBuffer[m_writeIndex] = samples[i];
        m_writeIndex = (m_writeIndex + 1) % WINDOW_SIZE;
        m_totalSamples++;
    }

    // After first WINDOW_SIZE samples, window is always ready
    if (!m_windowReady && m_totalSamples >= WINDOW_SIZE) {
        m_windowReady = true;
    }
}

void GoertzelAnalyzer::copyToContiguous() {
    // Ring buffer order: oldest samples are at writeIndex, newest at writeIndex-1
    // We need to copy in time order: oldest first
    //
    // Example with WINDOW_SIZE=8, writeIndex=3:
    //   Ring: [5, 6, 7, ?, ?, 2, 3, 4]  (indices 0-7, writeIndex=3 is next write)
    //                   ^ writeIndex
    //   Time order: 2, 3, 4, 5, 6, 7 (if we only have 6 samples)
    //
    // After full: writeIndex points to oldest sample
    //   Ring: [5, 6, 7, 0, 1, 2, 3, 4]
    //                   ^ writeIndex=3
    //   Time order: 0, 1, 2, 3, 4, 5, 6, 7

    // Copy from writeIndex to end (oldest samples)
    size_t firstPartLen = WINDOW_SIZE - m_writeIndex;
    std::memcpy(m_contiguousBuffer, &m_ringBuffer[m_writeIndex],
                firstPartLen * sizeof(int16_t));

    // Copy from start to writeIndex (newest samples)
    if (m_writeIndex > 0) {
        std::memcpy(&m_contiguousBuffer[firstPartLen], m_ringBuffer,
                    m_writeIndex * sizeof(int16_t));
    }
}

float GoertzelAnalyzer::computeGoertzel(const int16_t* buffer, size_t N, float coeff) const {
    // Goertzel algorithm state variables
    float s0 = 0.0f; // Current state
    float s1 = 0.0f; // Previous state 1
    float s2 = 0.0f; // Previous state 2

    // Process all samples in the window
    for (size_t n = 0; n < N; ++n) {
        // Convert int16 to float and normalize to [-1, 1]
        float sample = static_cast<float>(buffer[n]) / 32768.0f;

        // Goertzel recursion: s[n] = sample[n] + coeff * s[n-1] - s[n-2]
        s0 = sample + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    // Compute magnitude from final state
    // Magnitude = sqrt(s1^2 + s2^2 - coeff * s1 * s2)
    float magnitude = std::sqrt(s1 * s1 + s2 * s2 - coeff * s1 * s2);

    return magnitude;
}

bool GoertzelAnalyzer::analyze(float* bandsOut) {
    // Can't analyze until we have a full window
    if (!m_windowReady) {
        return false;
    }

    // Copy ring buffer to contiguous buffer for Goertzel
    copyToContiguous();

    // Compute magnitude for each frequency band
    for (uint8_t i = 0; i < NUM_BANDS; ++i) {
        float rawMagnitude = computeGoertzel(m_contiguousBuffer, WINDOW_SIZE, m_coefficients[i]);

        // Normalize to [0,1] and clamp
        bandsOut[i] = std::min(1.0f, rawMagnitude * m_normFactors[i]);
    }

    // Note: Unlike the old design, we don't reset any flag here.
    // The window is always ready after startup, and we can analyze
    // every hop for sliding window updates.

    return true;
}

void GoertzelAnalyzer::reset() {
    m_writeIndex = 0;
    m_totalSamples = 0;
    m_windowReady = false;
    std::memset(m_ringBuffer, 0, sizeof(m_ringBuffer));
    std::memset(m_contiguousBuffer, 0, sizeof(m_contiguousBuffer));
}

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
