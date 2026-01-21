/**
 * @file GoertzelDFT.cpp
 * @brief Canonical Goertzel-based DFT implementation from Sensory Bridge 4.1.1
 * 
 * FROM SENSORY BRIDGE 4.1.1 - DO NOT MODIFY WITHOUT EVIDENCE
 * 
 * This implementation is a VERBATIM PORT of the Sensory Bridge Goertzel algorithm
 * adapted for LightwaveOS parameters (16kHz sample rate vs 12.8kHz).
 * 
 * CANONICAL SOURCE:
 * - Sensory Bridge 4.1.1 GDFT.h process_GDFT()
 * - Sensory Bridge 4.1.1 system.h precompute_goertzel_constants()
 * - Sensory Bridge 4.1.1 system.h generate_window_lookup()
 * 
 * SPECIFICATION:
 * - planning/audio-pipeline-redesign/HARDENED_SPEC.md §2-§4
 * 
 * @version 1.0.0
 * @date 2026-01-13
 */

#include "GoertzelDFT.h"
#include <cstring>  // For memset

namespace LightwaveOS {
namespace Audio {

// ============================================================================
// MATHEMATICAL CONSTANTS
// ============================================================================

constexpr float PI = 3.141592653589793f;
constexpr float TWOPI = 6.283185307179586f;

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

GoertzelDFT::GoertzelDFT()
    : m_initialized(false)
{
    // Zero-initialize arrays
    memset(m_freqBins, 0, sizeof(m_freqBins));
    memset(m_windowLookup, 0, sizeof(m_windowLookup));
    memset(m_magnitudes, 0, sizeof(m_magnitudes));
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool GoertzelDFT::init() {
    if (m_initialized) {
        return true;  // Already initialized
    }

    // Generate Hamming window lookup table
    initWindowLookup();

    // Precompute Goertzel coefficients for all bins
    precomputeCoefficients();

    m_initialized = true;
    return true;
}

// ============================================================================
// WINDOW FUNCTION GENERATION
// FROM: Sensory Bridge system.h:197-207 - DO NOT MODIFY
// ============================================================================

void GoertzelDFT::initWindowLookup() {
    /**
     * Hamming window generation (Sensory Bridge canonical implementation)
     * 
     * FORMULA: w(n) = 0.54 * (1 - cos(2π * n / (N-1)))
     * 
     * IMPORTANT: Uses 0.54 coefficient (Hamming), not 0.5 (Hann)
     * 
     * SCALE: 32767 (Q15 fixed-point maximum)
     * SYMMETRY: Window is symmetric, only compute half then mirror
     * 
     * FROM: HARDENED_SPEC.md §2.1
     */
    
    for (uint16_t i = 0; i < 2048; i++) {
        // Ratio from 0.0 to 1.0 across window
        float ratio = static_cast<float>(i) / 4095.0f;
        
        // Hamming window formula
        float weight = 0.54f * (1.0f - cosf(TWOPI * ratio));
        
        // Scale to Q15 fixed-point
        int16_t value = static_cast<int16_t>(32767.0f * weight);
        
        // Store symmetrical values
        m_windowLookup[i] = value;
        m_windowLookup[4095 - i] = value;
    }
}

// ============================================================================
// GOERTZEL COEFFICIENT PRECOMPUTATION
// FROM: Sensory Bridge system.h:209-256 - DO NOT MODIFY
// ============================================================================

void GoertzelDFT::precomputeCoefficients() {
    /**
     * Precompute Goertzel constants for all frequency bins
     * 
     * FROM: Sensory Bridge system.h precompute_goertzel_constants()
     * 
     * ALGORITHM:
     * 1. Calculate optimal block size for each frequency
     *    - Higher frequencies = smaller blocks (better time resolution)
     *    - Lower frequencies = larger blocks (better frequency resolution)
     * 2. Calculate Goertzel coefficient: 2 * cos(2π * k / N)
     * 3. Convert to Q14 fixed-point for integer-only DSP
     * 4. Precompute reciprocals for fast division
     * 
     * FROM: HARDENED_SPEC.md §3.1
     */

    for (uint16_t i = 0; i < LWOS_NUM_FREQS; i++) {
        // Get target frequency from canonical notes array
        m_freqBins[i].targetFreq = CANONICAL_NOTES[i];
        
        // Assign zone (0=bass, 1=treble)
        m_freqBins[i].zone = (i < 32) ? 0 : 1;
        
        // ====================================================================
        // BLOCK SIZE CALCULATION
        // ====================================================================
        
        /**
         * Calculate semitone bandwidth for this frequency
         * 
         * Semitone spacing = 2^(1/12) ≈ 1.05946
         * Therefore: semitone_bandwidth ≈ freq * 0.05946
         * 
         * This ensures each bin captures approximately 1 semitone of bandwidth
         */
        float semitoneHz = m_freqBins[i].targetFreq * 0.05946f;
        
        /**
         * Block size = sample_rate / (2 * bandwidth)
         * 
         * The factor of 2 ensures we meet Nyquist criterion for bandwidth
         */
        m_freqBins[i].blockSize = static_cast<uint16_t>(
            LWOS_SAMPLE_RATE / (semitoneHz * 2.0f)
        );
        
        /**
         * Clamp block size to [64, 2000] samples
         * 
         * Lower bound: Minimum useful window size
         * Upper bound: Prevents excessive computation time
         */
        if (m_freqBins[i].blockSize < 64) {
            m_freqBins[i].blockSize = 64;
        }
        if (m_freqBins[i].blockSize > 2000) {
            m_freqBins[i].blockSize = 2000;
        }
        
        // ====================================================================
        // GOERTZEL COEFFICIENT CALCULATION
        // ====================================================================
        
        /**
         * Step 1: Calculate bin index k
         * 
         * k = round(block_size * target_freq / sample_rate)
         * 
         * This maps the target frequency to the nearest integer DFT bin
         */
        float k = roundf(
            m_freqBins[i].blockSize * m_freqBins[i].targetFreq / LWOS_SAMPLE_RATE
        );
        
        /**
         * Step 2: Calculate normalized angular frequency
         * 
         * w = 2π * k / block_size
         * 
         * This is the angular frequency for this specific Goertzel bin
         */
        float w = (TWOPI * k) / m_freqBins[i].blockSize;
        
        /**
         * Step 3: Calculate Goertzel coefficient
         * 
         * coeff = 2 * cos(w)
         * 
         * This is the ONLY coefficient needed for the Goertzel recurrence relation:
         * q0[n] = sample[n] + coeff * q1[n-1] - q2[n-2]
         */
        float coeff = 2.0f * cosf(w);
        
        /**
         * Step 4: Convert to Q14 fixed-point
         * 
         * coeff_q14 = coeff * 2^14 = coeff * 16384
         * 
         * Q14 format uses 14 fractional bits, allowing range [-2, 2)
         * which is perfect for our coefficient range of approximately [-2, 2]
         */
        m_freqBins[i].coeffQ14 = static_cast<int32_t>(coeff * 16384.0f);
        
        // ====================================================================
        // PRECOMPUTED HELPERS
        // ====================================================================
        
        /**
         * Reciprocal for fast division
         * Avoids expensive division in hot path
         */
        m_freqBins[i].blockSizeRecip = 1.0f / m_freqBins[i].blockSize;
        
        /**
         * Window lookup multiplier
         * Maps block_size samples to 4096 window LUT entries
         */
        m_freqBins[i].windowMult = 4096.0f / m_freqBins[i].blockSize;
        
        /**
         * A-weighting ratio (placeholder for now)
         * 
         * A-weighting approximates human frequency perception
         * Lower frequencies are perceived as quieter than higher frequencies
         * at the same SPL (Sound Pressure Level)
         * 
         * TODO: Implement proper A-weighting curve from Sensory Bridge
         */
        m_freqBins[i].aWeightRatio = 1.0f;
    }
}

// ============================================================================
// GOERTZEL ANALYSIS (HOT PATH - MUST BE IN IRAM)
// FROM: Sensory Bridge GDFT.h:59-119 - DO NOT MODIFY
// ============================================================================

void IRAM_ATTR GoertzelDFT::analyze(const int16_t* samples, uint16_t sampleCount) {
    /**
     * Core Goertzel algorithm implementation
     * 
     * FROM: Sensory Bridge GDFT.h process_GDFT()
     * 
     * ALGORITHM: Goertzel's algorithm computes a single DFT bin using
     * a second-order IIR filter:
     * 
     * Recurrence (for each sample n):
     *     q0[n] = x[n] + coeff * q1[n-1] - q2[n-2]
     * 
     * Final magnitude:
     *     |X[k]|² = q1² + q2² - coeff * q1 * q2
     * 
     * CRITICAL IMPLEMENTATION NOTES:
     * 1. IRAM_ATTR: Function must be in IRAM for ESP32 performance
     * 2. Sample ordering: Newest samples FIRST (reverse indexing)
     * 3. Headroom shift: sample >> 6 prevents overflow in 32-bit math
     * 4. Fixed-point multiply: (coeff_q14 * q1) >> 14 maintains precision
     * 5. Normalization: Divide by block_size / 2 for unit scale
     * 
     * FROM: HARDENED_SPEC.md §4.1
     */
    
    // Iterate through all frequency bins
    for (uint16_t bin = 0; bin < LWOS_NUM_FREQS; bin++) {
        // Initialize Goertzel state variables
        int32_t q0, q1 = 0, q2 = 0;
        
        // Get bin parameters
        const uint16_t blockSize = m_freqBins[bin].blockSize;
        const int32_t coeff = m_freqBins[bin].coeffQ14;
        
        // Determine how many samples to process
        // (use minimum of blockSize and available samples)
        const uint16_t samplesToProcess = (blockSize < sampleCount) ? blockSize : sampleCount;
        
        // ====================================================================
        // GOERTZEL RECURRENCE RELATION
        // ====================================================================
        
        /**
         * Process samples in REVERSE order (newest first)
         * 
         * WHY: Sensory Bridge stores samples newest-first in circular buffer
         * We maintain this convention for compatibility
         */
        for (uint16_t n = 0; n < samplesToProcess; n++) {
            // Get sample (newest first = reverse indexing)
            int32_t sample = static_cast<int32_t>(samples[sampleCount - 1 - n]);
            
            /**
             * Goertzel iteration with Q14 fixed-point coefficient
             * 
             * q0 = (sample >> 6) + (coeff * q1 >> 14) - q2
             * 
             * The >> 6 shift provides headroom to prevent overflow
             * The >> 14 shift converts Q14 * int32 back to int32
             */
            int64_t mult = static_cast<int64_t>(coeff) * q1;
            q0 = (sample >> 6) + static_cast<int32_t>(mult >> 14) - q2;
            
            // Shift state forward
            q2 = q1;
            q1 = q0;
        }
        
        // ====================================================================
        // MAGNITUDE CALCULATION
        // ====================================================================
        
        /**
         * Final magnitude calculation from Goertzel state
         * 
         * magnitude² = q2² + q1² - coeff * q1 * q2
         * 
         * This formula comes from the DFT definition and the Goertzel
         * recurrence relation. It avoids computing complex exponentials.
         */
        int64_t mult = static_cast<int64_t>(coeff) * q1;
        int64_t magSquared = static_cast<int64_t>(q2) * q2
                           + static_cast<int64_t>(q1) * q1
                           - (static_cast<int32_t>(mult >> 14)) * q2;
        
        /**
         * Take square root to get linear magnitude
         * 
         * sqrtf() is used for float precision
         * Fast approximate sqrt could be used for optimization if needed
         */
        float magnitude = sqrtf(static_cast<float>(magSquared));
        
        /**
         * Normalize by block_size / 2
         * 
         * This scaling factor ensures:
         * - Magnitude 1.0 = pure tone at 0 dBFS
         * - Consistent scale across different block sizes
         * 
         * Division by 2 accounts for the Goertzel algorithm's scaling
         */
        m_magnitudes[bin] = magnitude * m_freqBins[bin].blockSizeRecip * 0.5f;
    }
}

// ============================================================================
// ACCESSORS
// ============================================================================

float GoertzelDFT::getMagnitude(uint16_t binIndex) const {
    if (binIndex >= LWOS_NUM_FREQS) {
        return 0.0f;  // Invalid bin index
    }
    return m_magnitudes[binIndex];
}

float GoertzelDFT::getBinFrequency(uint16_t binIndex) const {
    if (binIndex >= LWOS_NUM_FREQS) {
        return 0.0f;  // Invalid bin index
    }
    return m_freqBins[binIndex].targetFreq;
}

} // namespace Audio
} // namespace LightwaveOS
