// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
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
    // ========================================================================
    // Legacy 8-band initialization
    // ========================================================================
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

    // Initialize accumulation buffer to zero
    std::memset(m_accumBuffer, 0, sizeof(m_accumBuffer));

    // ========================================================================
    // 64-bin initialization (Sensory Bridge parity)
    // ========================================================================
    initHannLUT();
    initBins();

    // Initialize circular history buffer
    std::memset(m_sampleHistory, 0, sizeof(m_sampleHistory));
    m_historyWriteIndex = 0;
    m_sampleCount = 0;

    // Initialize 64-bin magnitudes to zero
    std::memset(m_magnitudes64, 0, sizeof(m_magnitudes64));
}

void GoertzelAnalyzer::initHannLUT() {
    // Generate 4096-entry Hann window in Q15 fixed-point
    // Hann(n) = 0.5 * (1 - cos(2π * n / (N-1)))
    // Q15 range: 0 to 32767
    for (size_t i = 0; i < HANN_LUT_SIZE; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(HANN_LUT_SIZE - 1);
        float hann = 0.5f * (1.0f - std::cos(2.0f * M_PI * t));
        m_hannLUT[i] = static_cast<int16_t>(hann * 32767.0f);
    }
}

void GoertzelAnalyzer::initBins() {
    // Initialize 64 semitone bins from A1 (55 Hz) upward
    // Frequency formula: f = 55 * 2^(bin/12)
    // This matches Sensory Bridge's frequency range starting at A1

    // First pass: compute all target frequencies (needed for adaptive block sizing)
    float notes[NUM_BINS];
    for (size_t bin = 0; bin < NUM_BINS; ++bin) {
        notes[bin] = 55.0f * std::pow(2.0f, static_cast<float>(bin) / 12.0f);
        m_bins[bin].target_freq = notes[bin];
    }

    // Second pass: compute adaptive block sizes and coefficients
    for (size_t bin = 0; bin < NUM_BINS; ++bin) {
        float freq = notes[bin];

        // ====================================================================
        // Adaptive Block Sizing (Sensory Bridge v4.1.0 algorithm)
        // Block size based on neighbor frequency distance for optimal resolution
        // ====================================================================
        float left_dist = (bin > 0) ? (freq - notes[bin - 1]) : (freq * 0.0595f);  // semitone ~5.95%
        float right_dist = (bin < NUM_BINS - 1) ? (notes[bin + 1] - freq) : (freq * 0.0595f);
        float max_neighbor_dist = std::fmax(left_dist, right_dist);

        // Block size = sample_rate / (max_neighbor_distance * 2)
        // This ensures the frequency bin width is smaller than the neighbor distance
        float block_size_f = static_cast<float>(SAMPLE_RATE_HZ) / (max_neighbor_dist * 2.0f);

        // Clamp to valid range
        if (block_size_f > static_cast<float>(MAX_BLOCK_SIZE)) {
            block_size_f = static_cast<float>(MAX_BLOCK_SIZE);
        }
        if (block_size_f < static_cast<float>(MIN_BLOCK_SIZE)) {
            block_size_f = static_cast<float>(MIN_BLOCK_SIZE);
        }

        m_bins[bin].block_size = static_cast<uint16_t>(block_size_f);
        m_bins[bin].block_size_recip = 1.0f / static_cast<float>(m_bins[bin].block_size);

        // ====================================================================
        // Discrete k Coefficient (Sensory Bridge v4.1.0 formula)
        // k = round(block_size * freq / sample_rate) ensures DFT bin alignment
        // coeff = 2 * cos(2π * k / block_size)
        // ====================================================================
        float k = std::round((static_cast<float>(m_bins[bin].block_size) * freq) /
                             static_cast<float>(SAMPLE_RATE_HZ));

        // ====================================================================
        // Stability Guard: Prevent coefficient from reaching ±2.0 exactly
        // When k = 0 (DC) or k = N/2 (Nyquist), coeff = ±2.0 causes instability.
        // Nudge k by 0.5 to move away from these boundaries.
        // ====================================================================
        float half_block = static_cast<float>(m_bins[bin].block_size) * 0.5f;
        if (k < 1.0f) {
            k = 1.0f;  // Avoid DC (k=0 gives coeff=+2.0)
        } else if (k >= half_block - 0.5f) {
            k = half_block - 1.0f;  // Avoid Nyquist (k=N/2 gives coeff=-2.0)
        }

        float omega = (2.0f * M_PI * k) / static_cast<float>(m_bins[bin].block_size);
        float coeff = 2.0f * std::cos(omega);
        m_bins[bin].coeff_q14 = static_cast<int32_t>(coeff * 16384.0f);  // Q14

        // Window multiplier for Hann LUT indexing
        m_bins[bin].window_mult = static_cast<float>(HANN_LUT_SIZE) /
                                   static_cast<float>(m_bins[bin].block_size);

        // Zone assignment (4 zones, 16 bins each)
        m_bins[bin].zone = static_cast<uint8_t>(bin >> 4);  // bin / 16
    }
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
    // DEFENSIVE CHECK: Validate count parameter to prevent excessive iteration
    // Typical count is HOP_SIZE (256), but clamp to reasonable maximum
    constexpr size_t MAX_ACCUMULATE_COUNT = 512;  // 2x HOP_SIZE as safety limit
    if (count > MAX_ACCUMULATE_COUNT) {
        count = MAX_ACCUMULATE_COUNT;  // Clamp to safe maximum
    }
    
    for (size_t i = 0; i < count; ++i) {
        // Legacy 8-band accumulation buffer
        // DEFENSIVE CHECK: Validate m_accumIndex before array access
        if (m_accumIndex >= WINDOW_SIZE) {
            m_accumIndex = 0;  // Reset if corrupted
        }
        m_accumBuffer[m_accumIndex] = samples[i];
        m_accumIndex++;

        if (m_accumIndex >= WINDOW_SIZE) {
            m_accumIndex = 0;
            m_windowFull = true;
        }

        // 64-bin circular history buffer
        // DEFENSIVE CHECK: Validate m_historyWriteIndex before array access (already done in getHistorySample, but also here for safety)
        if (m_historyWriteIndex >= SAMPLE_HISTORY_LENGTH) {
            m_historyWriteIndex = 0;  // Reset if corrupted
        }
        m_sampleHistory[m_historyWriteIndex] = samples[i];
        m_historyWriteIndex = (m_historyWriteIndex + 1) % SAMPLE_HISTORY_LENGTH;
        if (m_sampleCount < SAMPLE_HISTORY_LENGTH) {
            m_sampleCount++;
        }
    }
}

int16_t GoertzelAnalyzer::getHistorySample(size_t samplesAgo) const {
    // Get sample from circular buffer
    // samplesAgo=0 means most recent sample
    if (samplesAgo >= m_sampleCount) {
        return 0;  // Not enough samples accumulated yet
    }

    // DEFENSIVE CHECK: Validate m_historyWriteIndex to prevent out-of-bounds access
    // If m_historyWriteIndex is corrupted (>= SAMPLE_HISTORY_LENGTH), the modulo
    // calculation might not protect against all cases. This validation ensures
    // we always use a valid index for circular buffer access.
    size_t safe_write_index = m_historyWriteIndex;
    if (safe_write_index >= SAMPLE_HISTORY_LENGTH) {
        safe_write_index = 0;  // Reset if corrupted
    }

    // Calculate read index (wrap around for circular buffer)
    size_t readIndex = (safe_write_index + SAMPLE_HISTORY_LENGTH - 1 - samplesAgo) % SAMPLE_HISTORY_LENGTH;
    
    // DEFENSIVE CHECK: Ensure readIndex is valid (should always be after modulo, but protects against edge cases)
    if (readIndex >= SAMPLE_HISTORY_LENGTH) {
        return 0;  // Safety fallback
    }
    
    return m_sampleHistory[readIndex];
}

float GoertzelAnalyzer::computeGoertzelBin(size_t binIndex) const {
    if (binIndex >= NUM_BINS) return 0.0f;

    const GoertzelBin& bin = m_bins[binIndex];

    // Check if we have enough samples for this bin's window size
    if (m_sampleCount < bin.block_size) {
        return 0.0f;
    }

    // Goertzel algorithm with Hann windowing
    // Using Q14 fixed-point coefficient for efficiency
    float s0 = 0.0f;
    float s1 = 0.0f;
    float s2 = 0.0f;

    float coeff = static_cast<float>(bin.coeff_q14) / 16384.0f;  // Q14 to float

    for (size_t n = 0; n < bin.block_size; ++n) {
        // Get sample from history (oldest first for this window)
        int16_t raw_sample = getHistorySample(bin.block_size - 1 - n);

        // Apply Hann window from LUT
        size_t lut_index = static_cast<size_t>(static_cast<float>(n) * bin.window_mult);
        if (lut_index >= HANN_LUT_SIZE) lut_index = HANN_LUT_SIZE - 1;
        int16_t window_val = m_hannLUT[lut_index];

        // Apply window (Q15 multiply, result in Q15)
        int32_t windowed = (static_cast<int32_t>(raw_sample) * window_val) >> 15;

        // Convert to float normalized [-1, 1]
        float sample = static_cast<float>(windowed) / 32768.0f;

        // Goertzel recursion
        s0 = sample + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    // Compute magnitude
    float magnitude = std::sqrt(s1 * s1 + s2 * s2 - coeff * s1 * s2);

    // Normalize by 2/block_size (Sensory Bridge v4.1.0 formula)
    // This produces consistent magnitude scaling regardless of block size
    magnitude *= (2.0f * bin.block_size_recip);

    return magnitude;
}

bool GoertzelAnalyzer::analyze64(float* binsOut) {
    // DEFENSIVE: Validate output pointer is not null
    if (binsOut == nullptr) {
        return false;
    }
    
    // Check if we have enough samples for the largest window
    if (m_sampleCount < MAX_BLOCK_SIZE) {
        return false;
    }

    if (m_interlacedEnabled) {
        // ====================================================================
        // Interlaced Processing (SensoryBridge pattern)
        // Only compute odd or even bins each frame, halving CPU load
        // Previously computed bins retain their values (2-frame latency for full spectrum)
        // ====================================================================
        size_t startBin = m_processOddBins ? 1 : 0;

        for (size_t bin = startBin; bin < NUM_BINS; bin += 2) {
            float magnitude = computeGoertzelBin(bin);

            // Store in internal buffer (clamped to [0,1])
            // With v4.1.0's 2/N normalization, magnitudes are already properly scaled
            m_magnitudes64[bin] = std::min(1.0f, magnitude);
        }

        // Toggle parity for next frame
        m_processOddBins = !m_processOddBins;
    } else {
        // ====================================================================
        // Full Processing (original behavior)
        // Compute all 64 bins every frame
        // ====================================================================
        for (size_t bin = 0; bin < NUM_BINS; ++bin) {
            float magnitude = computeGoertzelBin(bin);

            // Store in internal buffer (clamped to [0,1])
            m_magnitudes64[bin] = std::min(1.0f, magnitude);
        }
    }

    // DEFENSIVE: Copy all 64 bins to output (including previously computed ones for interlaced mode)
    // Additional null check before array access (redundant but safe)
    if (binsOut != nullptr) {
        for (size_t bin = 0; bin < NUM_BINS; ++bin) {
            binsOut[bin] = m_magnitudes64[bin];
        }
    }

    return true;
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

bool GoertzelAnalyzer::analyzeWindow(const int16_t* window, size_t N, float* bandsOut) {
    if (!window || !bandsOut) return false;
    if (N != WINDOW_SIZE) return false;
    for (uint8_t i = 0; i < NUM_BANDS; ++i) {
        float rawMagnitude = computeGoertzel(window, WINDOW_SIZE, m_coefficients[i]);
        bandsOut[i] = std::min(1.0f, rawMagnitude * m_normFactors[i]);
    }
    return true;
}

void GoertzelAnalyzer::reset() {
    // Reset legacy 8-band state
    m_accumIndex = 0;
    m_windowFull = false;
    std::memset(m_accumBuffer, 0, sizeof(m_accumBuffer));

    // Reset 64-bin state
    m_historyWriteIndex = 0;
    m_sampleCount = 0;
    std::memset(m_sampleHistory, 0, sizeof(m_sampleHistory));
    std::memset(m_magnitudes64, 0, sizeof(m_magnitudes64));

    // Reset interlaced processing state
    m_processOddBins = false;
}

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
