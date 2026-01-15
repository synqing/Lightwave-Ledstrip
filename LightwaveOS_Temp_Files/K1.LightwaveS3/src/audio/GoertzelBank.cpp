/**
 * @file GoertzelBank.cpp
 * @brief Multi-bin Goertzel DFT implementation
 *
 * @author LightwaveOS Team
 * @version 2.0.0 - Tempo Tracking Dual-Bank Architecture
 */

#include "GoertzelBank.h"
#include "AudioRingBuffer.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace lightwaveos {
namespace audio {

GoertzelBank::GoertzelBank(uint8_t numBins, const GoertzelConfig* configs)
    : m_configs(configs)
    , m_bins(nullptr)
    , m_numBins(numBins)
    , m_windowBuffer(nullptr)
    , m_maxWindowSize(0)
{
    // Allocate per-bin state
    m_bins = new BinState[numBins];

    // Initialize bins and find maximum window size
    for (uint8_t i = 0; i < numBins; i++) {
        m_bins[i].q1_q14 = 0;
        m_bins[i].q2_q14 = 0;
        m_bins[i].coeff_q14 = configs[i].coeff_q14;
        m_bins[i].windowSize = configs[i].windowSize;

        if (configs[i].windowSize > m_maxWindowSize) {
            m_maxWindowSize = configs[i].windowSize;
        }
    }

    // Allocate window buffer (for largest window size)
    m_windowBuffer = new float[m_maxWindowSize];
}

GoertzelBank::~GoertzelBank() {
    if (m_bins != nullptr) {
        delete[] m_bins;
    }
    if (m_windowBuffer != nullptr) {
        delete[] m_windowBuffer;
    }
}

void GoertzelBank::applyHannWindow(float* samples, uint16_t n) {
    // Phase 2B: Placeholder implementation (uniform window)
    // Phase 3: Will integrate Hann LUT from K1_GoertzelTables_16k.h

    // For now, apply simplified Hann window using float math
    // w[n] = 0.5 * (1 - cos(2*pi*n / (N-1)))
    const float pi = 3.14159265358979323846f;
    const float scale = 2.0f * pi / static_cast<float>(n - 1);

    for (uint16_t i = 0; i < n; i++) {
        float w = 0.5f * (1.0f - cosf(scale * static_cast<float>(i)));
        samples[i] *= w;
    }
}

float GoertzelBank::processBin(uint8_t binIndex, const AudioRingBuffer<float, 2048>& ringBuffer) {
    if (binIndex >= m_numBins) {
        return 0.0f;
    }

    const BinState& bin = m_bins[binIndex];
    const uint16_t N = bin.windowSize;

    // Check if enough samples available
    if (ringBuffer.size() < N) {
        return 0.0f;  // Not enough samples yet
    }

    // Extract last N samples from ring buffer
    ringBuffer.copyLast(m_windowBuffer, N);

    // Apply Hann window
    applyHannWindow(m_windowBuffer, N);

    // Goertzel algorithm with Q14 fixed-point coefficient
    // State variables: q1, q2
    // Recurrence: q[n] = sample[n] + (coeff * q[n-1]) - q[n-2]
    //
    // Q14 format: 16384 = 1.0
    // coeff_q14 = round(2 * cos(2*pi*f/Fs) * 16384)

    int32_t q1 = 0;  // q[n-1]
    int32_t q2 = 0;  // q[n-2]
    const int32_t coeff = bin.coeff_q14;

    // Process samples
    for (uint16_t n = 0; n < N; n++) {
        // Convert float sample to Q14
        int32_t sample_q14 = static_cast<int32_t>(m_windowBuffer[n] * 16384.0f);

        // Clamp to prevent overflow
        if (sample_q14 > 32767) sample_q14 = 32767;
        if (sample_q14 < -32768) sample_q14 = -32768;

        // Goertzel recurrence: q0 = sample + (coeff * q1 >> 14) - q2
        // Shift by 14 to account for Q14 * Q14 = Q28 multiplication
        int32_t q0 = sample_q14 + ((coeff * q1) >> 14) - q2;

        // Update state
        q2 = q1;
        q1 = q0;
    }

    // Compute magnitude from final state
    // magnitude^2 = q1^2 + q2^2 - q1*q2*coeff
    // Convert back to float and normalize

    float q1_f = static_cast<float>(q1) / 16384.0f;
    float q2_f = static_cast<float>(q2) / 16384.0f;
    float coeff_f = static_cast<float>(coeff) / 16384.0f;

    float mag_squared = q1_f * q1_f + q2_f * q2_f - q1_f * q2_f * coeff_f;

    // Prevent negative values due to numerical errors
    if (mag_squared < 0.0f) {
        mag_squared = 0.0f;
    }

    // Normalize by window size (energy normalization)
    // Divide by N to account for window length
    float magnitude = sqrtf(mag_squared) / static_cast<float>(N);

    return magnitude;
}

void GoertzelBank::compute(const AudioRingBuffer<float, 2048>& ringBuffer, float* magnitudes) {
    if (magnitudes == nullptr) {
        return;
    }

    // Process each bin independently
    for (uint8_t i = 0; i < m_numBins; i++) {
        magnitudes[i] = processBin(i, ringBuffer);
    }
}

} // namespace audio
} // namespace lightwaveos
