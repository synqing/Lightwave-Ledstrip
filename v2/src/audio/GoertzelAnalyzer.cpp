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
    // These are empirically tuned to map expected magnitudes to [0,1]
    // Lower frequencies typically have higher energy, so they need stronger normalization
    m_normFactors[0] = 1.0f / 8000.0f;   // 60 Hz (sub-bass) - strongest normalization
    m_normFactors[1] = 1.0f / 6000.0f;   // 120 Hz (bass)
    m_normFactors[2] = 1.0f / 4000.0f;   // 250 Hz (low-mid)
    m_normFactors[3] = 1.0f / 3000.0f;   // 500 Hz (mid)
    m_normFactors[4] = 1.0f / 2000.0f;   // 1000 Hz (high-mid)
    m_normFactors[5] = 1.0f / 1500.0f;   // 2000 Hz (presence)
    m_normFactors[6] = 1.0f / 1000.0f;   // 4000 Hz (brilliance)
    m_normFactors[7] = 1.0f / 800.0f;    // 8000 Hz (air) - weakest normalization

    // Initialize accumulation buffer to zero
    std::memset(m_accumBuffer, 0, sizeof(m_accumBuffer));
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
    for (size_t i = 0; i < count; ++i) {
        m_accumBuffer[m_accumIndex] = samples[i];
        m_accumIndex++;

        if (m_accumIndex >= WINDOW_SIZE) {
            m_accumIndex = 0;
            m_windowFull = true;
        }
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
    // Only compute if we have a full window
    if (!m_windowFull) {
        return false;
    }

    // Compute magnitude for each frequency band
    for (uint8_t i = 0; i < NUM_BANDS; ++i) {
        float rawMagnitude = computeGoertzel(m_accumBuffer, WINDOW_SIZE, m_coefficients[i]);

        // Normalize to [0,1] and clamp
        bandsOut[i] = std::min(1.0f, rawMagnitude * m_normFactors[i]);
    }

    // Reset window flag (continue accumulating for next window)
    m_windowFull = false;

    return true;
}

void GoertzelAnalyzer::reset() {
    m_accumIndex = 0;
    m_windowFull = false;
    std::memset(m_accumBuffer, 0, sizeof(m_accumBuffer));
}

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
