/**
 * @file ChromaAnalyzer.cpp
 * @brief 12-bin chromagram analyzer implementation
 */

#include "ChromaAnalyzer.h"

#if FEATURE_AUDIO_SYNC

#include <cmath>
#include <cstring>
#include <algorithm>

namespace lightwaveos {
namespace audio {

// Define static constexpr arrays (C++17 requires definition)
constexpr float ChromaAnalyzer::NOTE_FREQS[NUM_OCTAVES * NUM_CHROMA];

ChromaAnalyzer::ChromaAnalyzer() {
    // Precompute Goertzel coefficients for all note frequencies
    for (uint8_t i = 0; i < NUM_OCTAVES * NUM_CHROMA; ++i) {
        m_coefficients[i] = computeCoefficient(NOTE_FREQS[i], SAMPLE_RATE_HZ, WINDOW_SIZE);
    }

    // Precompute normalization factors
    // Use uniform normalization similar to GoertzelAnalyzer
    float uniformFactor = 1.0f / 250.0f;
    for (uint8_t i = 0; i < NUM_OCTAVES * NUM_CHROMA; ++i) {
        m_normFactors[i] = uniformFactor;
    }

    // Initialize accumulation buffer to zero
    std::memset(m_accumBuffer, 0, sizeof(m_accumBuffer));
}

float ChromaAnalyzer::computeCoefficient(float targetFreq, uint32_t sampleRate, size_t windowSize) {
    // Compute normalized frequency k (bin index)
    // k = (targetFreq / sampleRate) * windowSize
    float k = (targetFreq / static_cast<float>(sampleRate)) * static_cast<float>(windowSize);

    // Goertzel coefficient: 2 * cos(2π * k / N)
    float omega = (2.0f * M_PI * k) / static_cast<float>(windowSize);
    return 2.0f * std::cos(omega);
}

void ChromaAnalyzer::accumulate(const int16_t* samples, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        m_accumBuffer[m_accumIndex] = samples[i];
        m_accumIndex++;

        if (m_accumIndex >= WINDOW_SIZE) {
            m_accumIndex = 0;
            m_windowFull = true;
        }
    }
}

float ChromaAnalyzer::computeGoertzel(const int16_t* buffer, size_t N, float coeff) const {
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

bool ChromaAnalyzer::analyze(float* chromaOut) {
    // Only compute if we have a full window
    if (!m_windowFull) {
        return false;
    }

    // Initialize chromagram to zero
    float chromaRaw[NUM_CHROMA] = {0.0f};

    // Compute magnitude for each note frequency and fold into pitch classes
    for (uint8_t octave = 0; octave < NUM_OCTAVES; ++octave) {
        for (uint8_t note = 0; note < NUM_CHROMA; ++note) {
            uint8_t noteIndex = octave * NUM_CHROMA + note;
            float rawMagnitude = computeGoertzel(m_accumBuffer, WINDOW_SIZE, m_coefficients[noteIndex]);

            // Normalize to [0,1] and clamp
            float normalized = std::min(1.0f, rawMagnitude * m_normFactors[noteIndex]);

            // Fold into pitch class (note % 12)
            // Weight by 0.5 per octave (matching Sensory Bridge aggregation)
            chromaRaw[note] += normalized * 0.5f;
        }
    }

    // Normalize chromagram output (clamp to [0,1])
    for (uint8_t i = 0; i < NUM_CHROMA; ++i) {
        chromaOut[i] = std::min(1.0f, chromaRaw[i]);
    }

    // Reset window flag (continue accumulating for next window)
    m_windowFull = false;

    return true;
}

void ChromaAnalyzer::reset() {
    m_accumIndex = 0;
    m_windowFull = false;
    std::memset(m_accumBuffer, 0, sizeof(m_accumBuffer));
}

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC

