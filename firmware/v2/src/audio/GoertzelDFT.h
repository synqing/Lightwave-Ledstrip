/**
 * @file GoertzelDFT.h
 * @brief Canonical Goertzel-based DFT implementation from Sensory Bridge 4.1.1
 * 
 * FROM SENSORY BRIDGE 4.1.1 - DO NOT MODIFY WITHOUT EVIDENCE
 * 
 * This module implements frequency analysis using the Goertzel algorithm with:
 * - 64 semitone-spaced frequency bins (A1 55Hz to C7 2093Hz)
 * - Variable block sizes per frequency for optimal resolution
 * - Q14 fixed-point coefficients for integer-only DSP
 * - Hamming window for spectral leakage reduction
 * 
 * CANONICAL SOURCE:
 * - Sensory Bridge 4.1.1: https://github.com/connornishijima/SensoryBridge
 * - Reference: GDFT.h, system.h, constants.h
 * 
 * SPECIFICATION DOCUMENTS:
 * - planning/audio-pipeline-redesign/HARDENED_SPEC.md §2-§4
 * - planning/audio-pipeline-redesign/MAGIC_NUMBERS.md §1-§3
 * - planning/audio-pipeline-redesign/prd.md §5.1, §5.2
 * 
 * CRITICAL CONSTRAINTS:
 * - Processing budget: ≤8ms per hop (part of 8ms total audio budget)
 * - Memory budget: ~20KB for all audio structures
 * - Sample rate: 16000 Hz (adapted from Sensory Bridge 12800 Hz)
 * - Chunk size: 128 samples per hop
 * 
 * @version 1.0.0
 * @date 2026-01-13
 */

#pragma once

#include <cstdint>
#include <cmath>
#include "AudioCanonicalConfig.h"

#ifdef ESP_PLATFORM
#include <esp_attr.h>
#else
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif
#endif

namespace LightwaveOS {
namespace Audio {

// ============================================================================
// CANONICAL CONSTANTS (FROM SENSORY BRIDGE 4.1.1)
// ============================================================================

// Core audio constants now defined in AudioCanonicalConfig.h:
// - LWOS_SAMPLE_RATE = 16000 Hz
// - LWOS_CHUNK_SIZE = 128 samples
// - LWOS_NUM_FREQS = 64 bins

/**
 * @brief Hop period = LWOS_CHUNK_SIZE / LWOS_SAMPLE_RATE
 * FROM: MAGIC_NUMBERS.md §1.3
 * CRITICAL: All processing MUST complete within this time
 */
constexpr float LWOS_HOP_PERIOD_MS = 8.0f;  // milliseconds

/**
 * @brief Window lookup table size
 * FROM: Sensory Bridge system.h
 * WHY: 4096 provides sufficient resolution for window function
 */
constexpr uint16_t WINDOW_LUT_SIZE = 4096;

// ============================================================================
// CANONICAL NOTE FREQUENCIES (FROM SENSORY BRIDGE 4.1.1 - DO NOT MODIFY)
// ============================================================================

/**
 * @brief Exact semitone frequencies from A1 (55 Hz) to C7 (2093 Hz)
 * 
 * FROM: Sensory Bridge constants.h notes[] array
 * FORMULA: f(n) = 440 * 2^((n-49)/12) where n=49 is A4 (440Hz)
 * 
 * These frequencies are MATHEMATICALLY PRECISE - do not round or approximate.
 */
constexpr float CANONICAL_NOTES[64] = {
    // Octave 1 (A1-G#2) - indices 0-11
    55.00000f, 58.27047f, 61.73541f, 65.40639f, 69.29566f, 73.41619f,
    77.78175f, 82.40689f, 87.30706f, 92.49861f, 97.99886f, 103.8262f,

    // Octave 2 (A2-G#3) - indices 12-23
    110.0000f, 116.5409f, 123.4708f, 130.8128f, 138.5913f, 146.8324f,
    155.5635f, 164.8138f, 174.6141f, 184.9972f, 195.9977f, 207.6523f,

    // Octave 3 (A3-G#4) - indices 24-35
    220.0000f, 233.0819f, 246.9417f, 261.6256f, 277.1826f, 293.6648f,
    311.1270f, 329.6276f, 349.2282f, 369.9944f, 391.9954f, 415.3047f,

    // Octave 4 (A4-G#5) - indices 36-47
    440.0000f, 466.1638f, 493.8833f, 523.2511f, 554.3653f, 587.3295f,
    622.2540f, 659.2551f, 698.4565f, 739.9888f, 783.9909f, 830.6094f,

    // Octave 5 (A5-G#6) - indices 48-59
    880.0000f, 932.3275f, 987.7666f, 1046.502f, 1108.731f, 1174.659f,
    1244.508f, 1318.510f, 1396.913f, 1479.978f, 1567.982f, 1661.219f,

    // Octave 6 (A6-C7) - indices 60-63 (partial octave)
    1760.000f, 1864.655f, 1975.533f, 2093.005f
};

// ============================================================================
// DATA STRUCTURES (FROM SENSORY BRIDGE 4.1.1)
// ============================================================================

/**
 * @brief Frequency bin metadata for Goertzel analysis
 * 
 * FROM: Sensory Bridge globals.h struct freq
 * 
 * Each bin has:
 * - Target frequency (Hz) from CANONICAL_NOTES
 * - Goertzel coefficient in Q14 fixed-point for integer-only DSP
 * - Variable block size optimized for frequency resolution
 * - Precomputed reciprocals for fast division
 * - Zone assignment (0=bass, 1=treble)
 * - A-weighting for perceptual balance
 */
struct FrequencyBin {
    float    targetFreq;       ///< Target frequency in Hz (from CANONICAL_NOTES)
    int32_t  coeffQ14;         ///< Goertzel coefficient in Q14 fixed-point: (1<<14) * 2*cos(w)
    uint16_t blockSize;        ///< Samples per Goertzel window (varies by frequency)
    float    blockSizeRecip;   ///< Precomputed 1.0f / blockSize for fast division
    uint8_t  zone;             ///< 0=bass (bins 0-31), 1=treble (bins 32-63)
    float    aWeightRatio;     ///< A-weighting perceptual correction factor
    float    windowMult;       ///< Window lookup multiplier: 4096.0f / blockSize
};

// ============================================================================
// GOERTZEL DFT CLASS
// ============================================================================

/**
 * @brief Goertzel-based Discrete Fourier Transform analyzer
 * 
 * Implements canonical Sensory Bridge 4.1.1 Goertzel algorithm with:
 * - Semitone-spaced frequency bins (NOT arbitrary FFT bins)
 * - Variable block sizes per frequency
 * - Q14 fixed-point coefficients for ESP32 performance
 * - Hamming window for spectral leakage reduction
 * 
 * TIMING: Target <6ms for 64-bin analysis @ 240MHz
 * MEMORY: ~20KB for coefficients, window LUT, and output bins
 */
class GoertzelDFT {
public:
    GoertzelDFT();
    ~GoertzelDFT() = default;

    /**
     * @brief Initialize Goertzel coefficients and window lookup table
     * 
     * FROM: Sensory Bridge system.h precompute_goertzel_constants()
     *       + generate_window_lookup()
     * 
     * WHEN: Called once during AudioNode::onStart()
     * WHERE: Populates g_freqBins and g_windowLookup
     * 
     * TIMING: ~1ms (not in hot path)
     * 
     * @return true if initialization successful
     */
    bool init();

    /**
     * @brief Perform Goertzel analysis on windowed audio samples
     * 
     * FROM: Sensory Bridge GDFT.h process_GDFT()
     * 
     * ALGORITHM:
     * 1. For each frequency bin:
     *    - Run Goertzel recurrence on block_size samples
     *    - Calculate magnitude from final q1/q2 state
     *    - Normalize by block size
     * 2. Store results in g_magnitudes[]
     * 
     * CRITICAL: Function MUST be in IRAM for ESP32 performance
     * 
     * TIMING: Target <6ms for 64 bins @ 240MHz
     * 
     * @param samples Windowed audio samples (newest first)
     * @param sampleCount Number of samples available
     */
    void IRAM_ATTR analyze(const int16_t* samples, uint16_t sampleCount);

    /**
     * @brief Get normalized magnitude for a specific frequency bin
     * 
     * @param binIndex Bin index [0, 63]
     * @return Normalized magnitude [0.0, inf) where 1.0 = reference level
     */
    float getMagnitude(uint16_t binIndex) const;

    /**
     * @brief Get all bin magnitudes
     * 
     * @return Pointer to magnitude array (64 floats)
     */
    const float* getMagnitudes() const { return m_magnitudes; }

    /**
     * @brief Get target frequency for a specific bin
     * 
     * @param binIndex Bin index [0, 63]
     * @return Frequency in Hz from CANONICAL_NOTES
     */
    float getBinFrequency(uint16_t binIndex) const;

    /**
     * @brief Get frequency bin metadata
     * 
     * @param binIndex Bin index [0, 63]
     * @return Const reference to FrequencyBin struct
     */
    const FrequencyBin& getBinInfo(uint16_t binIndex) const { return m_freqBins[binIndex]; }

private:
    /**
     * @brief Precompute Goertzel coefficients for all frequency bins
     * 
     * FROM: Sensory Bridge system.h precompute_goertzel_constants()
     * 
     * FORMULA:
     * - block_size = sample_rate / (2 * semitone_bandwidth_hz)
     * - k = round(block_size * target_freq / sample_rate)
     * - w = 2π * k / block_size
     * - coeff = 2 * cos(w)
     * - coeff_q14 = coeff * 16384
     */
    void precomputeCoefficients();

    /**
     * @brief Generate Hamming window lookup table
     * 
     * FROM: Sensory Bridge system.h generate_window_lookup()
     * 
     * FORMULA: w(n) = 0.54 * (1 - cos(2π * n / N))
     * SCALE: 32767 (Q15 fixed-point)
     * SIZE: 4096 entries (symmetrical, only compute half)
     */
    void initWindowLookup();

    // Static data (persistent across analysis calls)
    FrequencyBin m_freqBins[LWOS_NUM_FREQS];   ///< Frequency bin metadata
    int16_t      m_windowLookup[WINDOW_LUT_SIZE]; ///< Hamming window LUT (Q15)
    float        m_magnitudes[LWOS_NUM_FREQS];    ///< Output bin magnitudes
    bool         m_initialized;                   ///< Initialization guard
};

} // namespace Audio
} // namespace LightwaveOS
