/**
 * @file K1Types.h
 * @brief K1 Dual-Bank Goertzel Front-End - Core Data Types
 *
 * Defines the fundamental data structures for the K1 audio processing pipeline.
 * All timestamps use sample counter (no system timers).
 *
 * @author LightwaveOS Team
 * @version 1.0.0 - K1 Migration
 */

#pragma once

#include <cstdint>
#include "K1Spec.h"

namespace lightwaveos {
namespace audio {
namespace k1 {

// ============================================================================
// Audio Chunk
// ============================================================================

/**
 * @brief Single hop of audio samples
 *
 * Contains 128 samples (8ms at 16kHz) with sample counter timestamp.
 */
struct AudioChunk {
    int16_t samples[HOP_SAMPLES];        ///< Mono audio samples
    uint32_t n = HOP_SAMPLES;            ///< Number of samples (always 128)
    uint64_t sample_counter_end;         ///< Inclusive end sample index
};

// ============================================================================
// Audio Feature Frame
// ============================================================================

/**
 * @brief Complete feature frame output from K1 front-end
 *
 * Published every hop (125 Hz). Harmony fields are only valid when
 * harmony_valid is true (every 2 hops = 62.5 Hz).
 */
struct AudioFeatureFrame {
    // Timestamps (derived from sample counter)
    uint64_t t_samples;                  ///< End sample index of this frame
    uint32_t hop_index;                   ///< Increments each 128-sample chunk
    float t_us;                          ///< Derived from t_samples and FS_HZ

    // Rhythm (every hop, 125 Hz)
    float rhythm_bins[RHYTHM_BINS];      ///< Post-noise, post-AGC magnitudes
    float rhythm_energy;                  ///< RMS (windowed)
    float rhythm_novelty;                 ///< Flux-style novelty (rhythm bank only)

    // Harmony (every 2 hops, 62.5 Hz)
    bool harmony_valid;                  ///< True only on harmony ticks
    float harmony_bins[HARMONY_BINS];    ///< Post-noise, post-AGC magnitudes
    float chroma12[12];                  ///< Sum-normalized chroma (12 pitch classes)
    float chroma_stability;               ///< Rolling cosine mean/var metric
    float key_clarity;                    ///< Simple "peakiness" metric for gating

    // Quality flags
    bool is_silence;                      ///< RMS below threshold for M hops
    bool is_clipping;                     ///< Any sample hits near int16 rails
    bool overload;                        ///< Compute overrun / dropped harmony tick
};

// ============================================================================
// Goertzel Bin Specification
// ============================================================================

/**
 * @brief Specification for a single Goertzel bin
 *
 * Contains precomputed coefficients and window length for efficient processing.
 * The norm_q field is added in Phase 2 (magnitude normalization).
 */
struct GoertzelBinSpec {
    float freq_hz;                       ///< Target frequency (Hz)
    uint16_t N;                          ///< Window length (samples)
    uint16_t k;                          ///< Reference DFT-bin index (rounded)
    int16_t coeff_q14;                   ///< Q14: 2*cos(2πk/N) * 16384
    // norm_q?? will be added in Phase 2 for magnitude normalization
};

} // namespace k1
} // namespace audio
} // namespace lightwaveos

