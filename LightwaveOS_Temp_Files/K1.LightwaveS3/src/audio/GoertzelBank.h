/**
 * @file GoertzelBank.h
 * @brief Multi-bin Goertzel DFT with per-bin window sizing
 *
 * Implements Goertzel algorithm for efficient single-frequency DFT computation.
 * Supports 24 bins (RhythmBank) or 64 bins (HarmonyBank) with per-bin N.
 *
 * Design:
 * - Q14 fixed-point coefficients for computational efficiency
 * - Variable window lengths (256-1536 samples) per bin
 * - Uses AudioRingBuffer::copyLast() for efficient sample extraction
 * - Hann windowing via lookup table (from K1_GoertzelTables_16k.h)
 *
 * Memory: numBins * sizeof(BinState) ~= 24*12 bytes = 288 bytes (RhythmBank)
 *         or 64*12 bytes = 768 bytes (HarmonyBank)
 * Thread-safety: Single-threaded (caller must synchronize)
 *
 * Used by: RhythmBank and HarmonyBank wrappers (Phase 2 integration)
 *
 * @author LightwaveOS Team
 * @version 2.0.0 - Tempo Tracking Dual-Bank Architecture
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>

// Forward declaration for AudioRingBuffer template
namespace lightwaveos {
namespace audio {
    template<typename T, size_t CAPACITY>
    class AudioRingBuffer;
}
}

namespace lightwaveos {
namespace audio {

/**
 * @brief Configuration for a single Goertzel bin
 *
 * Defines frequency, window size, and pre-computed coefficient.
 * Source: K1_GoertzelTables_16k.h (kHarmonyBins_16k_64 or kRhythmBins_16k_24)
 */
struct GoertzelConfig {
    float freq_hz;         ///< Target frequency in Hz
    uint16_t windowSize;   ///< Number of samples for this bin (N)
    int16_t coeff_q14;     ///< Pre-computed 2*cos(2*pi*f/Fs) in Q14 format
};

/**
 * @brief Multi-bin Goertzel DFT bank
 *
 * Processes multiple frequency bins with individual window sizes.
 * Each bin extracts N samples from ring buffer, applies Hann window,
 * and computes magnitude via Goertzel algorithm.
 *
 * Example usage:
 * @code
 * // Define bins (typically from K1_GoertzelTables_16k.h)
 * GoertzelConfig rhythmBins[24] = {...};
 * GoertzelBank bank(24, rhythmBins);
 *
 * // Per-hop processing
 * AudioRingBuffer<float, 2048> ringBuffer;
 * float magnitudes[24];
 * bank.compute(ringBuffer, magnitudes);
 * @endcode
 */
class GoertzelBank {
public:
    /**
     * @brief Construct Goertzel bank
     *
     * @param numBins Number of frequency bins (24 for Rhythm, 64 for Harmony)
     * @param configs Array of bin configurations (must remain valid for lifetime)
     *
     * Preconditions:
     * - configs array must have numBins elements
     * - configs must remain valid for lifetime of GoertzelBank
     * - windowSize for all bins must be <= ring buffer capacity
     */
    GoertzelBank(uint8_t numBins, const GoertzelConfig* configs);

    /**
     * @brief Destructor - releases heap memory
     */
    ~GoertzelBank();

    /**
     * @brief Compute magnitudes for all bins
     *
     * For each bin:
     * 1. Extract last N samples from ring buffer
     * 2. Apply Hann window (TODO: integrate window LUT in Phase 3)
     * 3. Run Goertzel algorithm with Q14 coefficient
     * 4. Compute magnitude and store in output array
     *
     * @param ringBuffer Time-domain sample buffer (capacity >= max window size)
     * @param magnitudes Output array (must have space for numBins floats)
     *
     * Preconditions:
     * - ringBuffer.size() >= maximum windowSize across all bins
     * - magnitudes array has space for numBins elements
     *
     * CPU cost: ~10-20 µs per bin (24 bins ~= 0.24-0.48 ms, 64 bins ~= 0.64-1.28 ms)
     */
    void compute(const AudioRingBuffer<float, 2048>& ringBuffer, float* magnitudes);

    /**
     * @brief Get number of bins
     * @return Number of bins in this bank
     */
    uint8_t getNumBins() const {
        return m_numBins;
    }

    /**
     * @brief Get bin configuration
     * @param bin Bin index (0 to numBins-1)
     * @return Bin configuration (frequency, windowSize, coefficient)
     */
    const GoertzelConfig& getConfig(uint8_t bin) const {
        return m_configs[bin];
    }

private:
    /**
     * @brief Goertzel state for a single bin
     *
     * Maintains Q14 fixed-point state variables for iterative computation.
     */
    struct BinState {
        int32_t q1_q14;        ///< Goertzel state variable 1 (Q14 fixed-point)
        int32_t q2_q14;        ///< Goertzel state variable 2 (Q14 fixed-point)
        int32_t coeff_q14;     ///< Pre-computed coefficient (Q14 fixed-point)
        uint16_t windowSize;   ///< N samples for this bin
    };

    const GoertzelConfig* m_configs;  ///< Bin configurations (not owned)
    BinState* m_bins;                 ///< Per-bin Goertzel state (heap-allocated)
    uint8_t m_numBins;                ///< Number of bins
    float* m_windowBuffer;            ///< Temp buffer for windowed samples (heap)
    uint16_t m_maxWindowSize;         ///< Maximum window size across all bins

    /**
     * @brief Process a single bin
     *
     * @param binIndex Bin index (0 to numBins-1)
     * @param ringBuffer Ring buffer for sample extraction
     * @return Magnitude (non-negative float)
     */
    float processBin(uint8_t binIndex, const AudioRingBuffer<float, 2048>& ringBuffer);

    /**
     * @brief Apply Hann window to samples
     *
     * Phase 2B: Placeholder implementation (uniform window)
     * Phase 3: Will use pre-computed LUT from K1_GoertzelTables_16k.h
     *
     * @param samples Input/output sample buffer
     * @param n Number of samples
     */
    void applyHannWindow(float* samples, uint16_t n);
};

} // namespace audio
} // namespace lightwaveos
