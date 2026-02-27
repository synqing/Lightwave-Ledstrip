// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once
#include "config/features.h"
#include "config/audio_config.h"
#if FEATURE_AUDIO_SYNC

#include <cstdint>
#include <cstddef>

namespace lightwaveos {
namespace audio {

/**
 * @brief Goertzel 64-bin frequency analyzer with Sensory Bridge parity
 *
 * This analyzer implements a 64-bin Goertzel-based Discrete Fourier Transform (GDFT)
 * matching the Sensory Bridge audio analysis algorithm. Key features:
 *
 * 1. **64 Semitone Bins**: Musical note frequencies from A1 (55 Hz) to C7 (2093 Hz)
 *    Each bin is one semitone apart: freq = 55 * 2^(bin/12)
 *    Coverage: 5.25 octaves (64 semitones from bin 0 to bin 63)
 *
 * 2. **Adaptive Window Sizing** (v4.1.0 algorithm): Block size calculated based on
 *    neighbor frequency distance to maximize frequency resolution per bin.
 *    - 55 Hz (bin 0): 2000 samples (capped) = 125ms @ 16kHz
 *    - 2093 Hz (bin 63): ~64 samples (min) = 4ms @ 16kHz
 *
 * 3. **Hann Windowing**: 4096-entry lookup table for smooth spectral leakage reduction
 *    (retained from v3.1.0 for cleaner LED visualization)
 *
 * 4. **Discrete k Coefficient** (v4.1.0 formula): k = round(block_size * freq / sample_rate)
 *    ensures Goertzel targets exact DFT bin boundaries. Stability guard prevents
 *    k from reaching DC (k=0) or Nyquist (k=N/2) boundaries.
 *
 * 5. **Backward Compatible**: Still provides 8-band output for ControlBus integration
 *
 * Sample rate: 16 kHz (Nyquist 8 kHz, well above max bin frequency of 2093 Hz)
 * Max sample history: 2000 samples for lowest frequency bins
 */
class GoertzelAnalyzer {
public:
    // ========================================================================
    // Constants
    // ========================================================================

    // Legacy 8-band mode (backward compatible with ControlBus)
    static constexpr size_t WINDOW_SIZE = 512;      // Legacy fixed window
    static constexpr uint8_t NUM_BANDS = 8;         // Legacy band count

    // Sensory Bridge parity: 64 semitone bins
    static constexpr size_t NUM_BINS = 64;          // A1 (55 Hz) to C7 (2093 Hz) = 5.25 octaves
    static constexpr uint32_t SAMPLE_RATE_HZ = SAMPLE_RATE;

    // Adaptive window sizing (Sensory Bridge v4.1.0 algorithm)
    static constexpr size_t MAX_BLOCK_SIZE = 2000;  // Max samples for lowest freq (capped)
    static constexpr size_t MIN_BLOCK_SIZE = 64;    // Min samples for highest freq

    // Hann window lookup table size (Q15 fixed-point)
    static constexpr size_t HANN_LUT_SIZE = 4096;

    // Sample history buffer (circular, holds MAX_BLOCK_SIZE samples)
    static constexpr size_t SAMPLE_HISTORY_LENGTH = MAX_BLOCK_SIZE;

    // ========================================================================
    // Per-Bin State Structure
    // ========================================================================

    /**
     * @brief Configuration and state for each frequency bin
     */
    struct GoertzelBin {
        float target_freq;          // Target frequency in Hz
        uint16_t block_size;        // Window size for this bin (samples)
        float block_size_recip;     // 1.0 / block_size for normalization
        int32_t coeff_q14;          // Goertzel coeff in Q14 fixed-point: 2*cos(w)*(1<<14)
        float window_mult;          // HANN_LUT_SIZE / block_size for window indexing
        uint8_t zone;               // Zone assignment (0-3) for per-zone max tracking
    };

    // ========================================================================
    // Public API
    // ========================================================================

    /**
     * @brief Constructor - precomputes all coefficients, window LUT, and bin configs
     */
    GoertzelAnalyzer();

    /**
     * @brief Accumulate audio samples into the circular history buffer
     * @param samples Pointer to int16_t PCM samples
     * @param count Number of samples to add (typically HOP_SIZE = 256)
     *
     * Samples are added to the rolling history. After each call, analyze64() can
     * compute fresh results using the most recent MAX_BLOCK_SIZE samples.
     */
    void accumulate(const int16_t* samples, size_t count);

    /**
     * @brief Compute all 64 bin magnitudes using variable windows and Hann windowing
     * @param binsOut Output array of NUM_BINS (64) floats for bin magnitudes [0,1]
     * @return true if analysis completed, false if not enough samples accumulated
     *
     * Each bin uses its own window size and applies Hann windowing for reduced
     * spectral leakage. Results are normalized by block size and frequency-compensated.
     */
    bool analyze64(float* binsOut);

    /**
     * @brief Compute legacy 8-band magnitudes (backward compatible with ControlBus)
     * @param bandsOut Output array of NUM_BANDS (8) floats for band magnitudes [0,1]
     * @return true if new results are ready (enough samples), false otherwise
     *
     * This folds the 64 bins into 8 bands by averaging semitone groups:
     * - Band 0: Bins 0-7   (60-100 Hz, sub-bass)
     * - Band 1: Bins 8-15  (120-200 Hz, bass)
     * - Band 2: Bins 16-23 (250-400 Hz, low-mid)
     * - Band 3: Bins 24-31 (500-800 Hz, mid)
     * - Band 4: Bins 32-39 (1000-1600 Hz, high-mid)
     * - Band 5: Bins 40-47 (2000-3200 Hz, presence)
     * - Band 6: Bins 48-55 (4000+ Hz, brilliance)
     * - Band 7: Bins 56-63 (highest frequencies, air)
     */
    bool analyze(float* bandsOut);

    /**
     * @brief Compute magnitudes on an explicit window buffer (legacy API)
     * @param window Pointer to WINDOW_SIZE samples (contiguous)
     * @param N Number of samples (must be WINDOW_SIZE = 512)
     * @param bandsOut Output array of 8 floats [0,1]
     * @return true if computation succeeded, false otherwise
     *
     * NOTE: This uses the legacy 8-band fixed-window mode without Hann windowing.
     * For new code, prefer using analyze64() with the internal history buffer.
     */
    bool analyzeWindow(const int16_t* window, size_t N, float* bandsOut);

    /**
     * @brief Reset the accumulator and history buffer
     */
    void reset();

    // ========================================================================
    // Accessors for Bin Configuration
    // ========================================================================

    /**
     * @brief Get the configuration for a specific bin
     */
    const GoertzelBin& getBin(size_t index) const { return m_bins[index]; }

    /**
     * @brief Get the raw 64-bin magnitudes from the last analyze64() call
     */
    const float* getMagnitudes64() const { return m_magnitudes64; }

    /**
     * @brief Get the target frequency for a specific bin
     */
    float getBinFrequency(size_t index) const {
        return (index < NUM_BINS) ? m_bins[index].target_freq : 0.0f;
    }

    /**
     * @brief Check if enough samples have been accumulated for analysis
     */
    bool hasEnoughSamples() const { return m_sampleCount >= MAX_BLOCK_SIZE; }

    // ========================================================================
    // Interlaced Processing Control (SensoryBridge pattern)
    // ========================================================================

    /**
     * @brief Enable/disable interlaced 64-bin processing
     * @param enabled When true, only odd or even bins are processed each frame
     *
     * Trade-off:
     * - Enabled: ~50% CPU reduction, 2-frame latency for full spectrum
     * - Disabled: Full spectrum every frame, higher CPU load
     */
    void setInterlacedProcessing(bool enabled) { m_interlacedEnabled = enabled; }
    bool getInterlacedProcessing() const { return m_interlacedEnabled; }

    /**
     * @brief Get which bin parity was processed last (for debugging)
     * @return true if odd bins were processed, false if even bins
     */
    bool getLastProcessedParity() const { return m_processOddBins; }

private:
    // ========================================================================
    // Legacy 8-Band Configuration (backward compatibility)
    // ========================================================================

    // Target frequencies for legacy 8 bands
    // Band 7 set to 7800Hz to avoid Goertzel instability at Nyquist (8000Hz)
    static constexpr float TARGET_FREQS[NUM_BANDS] = {
        60.0f, 120.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 7800.0f
    };

    // ========================================================================
    // Hann Window Lookup Table (Q15 fixed-point)
    // ========================================================================

    int16_t m_hannLUT[HANN_LUT_SIZE];

    // ========================================================================
    // 64-Bin Configuration
    // ========================================================================

    GoertzelBin m_bins[NUM_BINS];
    float m_magnitudes64[NUM_BINS];  // Last computed 64-bin magnitudes

    // ========================================================================
    // Sample History Buffer (Circular)
    // ========================================================================

    int16_t m_sampleHistory[SAMPLE_HISTORY_LENGTH];
    size_t m_historyWriteIndex = 0;  // Next write position
    size_t m_sampleCount = 0;        // Total samples accumulated (saturates at SAMPLE_HISTORY_LENGTH)

    // ========================================================================
    // Legacy Mode State
    // ========================================================================

    int16_t m_accumBuffer[WINDOW_SIZE];  // Legacy accumulation buffer
    size_t m_accumIndex = 0;
    bool m_windowFull = false;

    // ========================================================================
    // Interlaced Processing State (SensoryBridge pattern)
    // ========================================================================
    bool m_interlacedEnabled = true;   // Enable by default for CPU efficiency
    bool m_processOddBins = false;     // Alternates each frame: false=even(0,2,4...), true=odd(1,3,5...)

    // Precomputed Goertzel coefficients for legacy 8 bands
    float m_coefficients[NUM_BANDS];

    // Normalization factors for legacy bands
    float m_normFactors[NUM_BANDS];

    // ========================================================================
    // Private Methods
    // ========================================================================

    /**
     * @brief Initialize the Hann window lookup table
     */
    void initHannLUT();

    /**
     * @brief Initialize the 64 frequency bins with Sensory Bridge formula
     */
    void initBins();

    /**
     * @brief Compute Goertzel magnitude for a single bin with Hann windowing
     * @param binIndex Index of the bin (0-63)
     * @return Raw magnitude before normalization
     *
     * Uses fixed-point arithmetic for the Goertzel iteration (Q14 coefficient)
     * and applies Hann window from the LUT during iteration.
     */
    float computeGoertzelBin(size_t binIndex) const;

    /**
     * @brief Compute legacy Goertzel magnitude (no windowing, float arithmetic)
     * @param buffer Input samples
     * @param N Number of samples
     * @param coeff Precomputed Goertzel coefficient (2*cos(w))
     * @return Raw magnitude before normalization
     */
    float computeGoertzel(const int16_t* buffer, size_t N, float coeff) const;

    /**
     * @brief Precompute Goertzel coefficient for a target frequency (legacy mode)
     */
    static float computeCoefficient(float targetFreq, uint32_t sampleRate, size_t windowSize);

    /**
     * @brief Get sample from circular history buffer
     * @param samplesAgo How many samples back from the most recent (0 = newest)
     * @return Sample value
     */
    int16_t getHistorySample(size_t samplesAgo) const;
};

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
