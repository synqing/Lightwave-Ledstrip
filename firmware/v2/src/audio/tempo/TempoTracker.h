/**
 * @file TempoTracker.h
 * @brief Goertzel-based tempo tracker for LightwaveOS v2
 *
 * Features:
 * - Goertzel resonator bank with adaptive block sizes
 * - VU derivative separation for sustained notes + percussion
 * - Window-free Goertzel (novelty pre-smoothed)
 * - Quartic scaling, silent bin suppression, novelty decay
 *
 * Key features:
 * - True VU separation: spectral and VU curves normalized independently, combined in Goertzel
 * - Novelty decay multiplier (0.999 per frame) to fade old beat events
 * - Silence detection to prevent false beats during quiet periods
 * - Window-free Goertzel - novelty already smoothed
 * - Dual-rate architecture: spectral (31.25 Hz) + VU (62.5 Hz)
 * - Dynamic scaling with rate-matched updates (spectral: 31.25 Hz, VU: 62.5 Hz)
 *
 * Design goals:
 * - 96 bins (60-156 BPM) at 1.0 BPM resolution
 * - ~22 KB total memory footprint
 * - Hysteresis winner selection for stable tempo lock
 * - Interleaved Goertzel computation for CPU efficiency
 *
 * @author LightwaveOS Team
 * @version 1.0.0
 */

#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>
#include "../contracts/TempoOutput.h"

namespace lightwaveos {
namespace audio {

// ============================================================================
// Tuning Configuration
// ============================================================================

struct TempoTrackerTuning {
    // Hysteresis for winner selection
    float hysteresisThreshold = 1.1f;    // Ratio (e.g., 1.1 = 10% advantage required)
    uint8_t hysteresisFrames = 5;        // Consecutive frames to switch

    // Smoothing
    float magnitudeAlpha = 0.025f;       // Smoothing factor for magnitudes (0.0-1.0)
    float silentDecay = 0.995f;          // Decay factor for silent bins

    // Silence detection
    float silenceThreshold = 0.5f;       // 0.0-1.0 (higher = more sensitive)
};

// ============================================================================
// Configuration Constants
// ============================================================================

/// Number of tempo bins (1 BPM resolution from 70-190)
constexpr uint16_t NUM_TEMPI = 120;

/// BPM range - covers 95% of music
constexpr float TEMPO_LOW = 70.0f;
constexpr float TEMPO_HIGH = TEMPO_LOW + static_cast<float>(NUM_TEMPI);

/// Spectral novelty rate (8-band Goertzel = every 512 samples @ 16kHz)
/// This is the rate at which we receive fresh band data for spectral flux
constexpr float SPECTRAL_LOG_HZ = 31.25f;

/// VU novelty rate (every hop = 256 samples @ 16kHz)
/// RMS derivative is computed every audio hop for transient detection
constexpr float VU_LOG_HZ = 62.5f;

/// Spectral history length (~16 seconds at 31.25 Hz)
constexpr uint16_t SPECTRAL_HISTORY_LENGTH = 512;

/// VU history length (~8 seconds at 62.5 Hz)
constexpr uint16_t VU_HISTORY_LENGTH = 512;

/// Phase shift for beat alignment (percentage of beat period)
constexpr float BEAT_SHIFT_PERCENT = 0.08f;

/// Reference FPS for phase velocity calculation
constexpr float REFERENCE_FPS = 100.0f;

/// Novelty decay multiplier per frame
/// Applied to history buffers to fade old beat events
constexpr float NOVELTY_DECAY = 0.999f;

// ============================================================================
// Tempo Bin Structure
// ============================================================================

/**
 * @brief State for a single Goertzel tempo bin
 *
 * Each bin tracks a specific target BPM using Goertzel resonator algorithm.
 * Phase and magnitude are extracted for beat synchronization.
 */
struct TempoBin {
    float target_bpm;               ///< Target tempo (48-144 BPM)
    float target_hz;                ///< target_bpm / 60
    float coeff;                    ///< Goertzel coefficient: 2 * cos(2pi * hz / log_hz)
    float sine;                     ///< sin(w) for magnitude extraction
    float cosine;                   ///< cos(w) for magnitude extraction
    uint32_t block_size;            ///< Adaptive block size for this bin
    float phase;                    ///< Extracted phase [-pi, +pi]
    bool phase_inverted;
    float phase_radians_per_frame;  ///< Phase velocity per frame
    float magnitude;                ///< Normalized magnitude [0, 1]
    float magnitude_raw;            ///< Raw magnitude before normalization
    float beat;                     ///< sin(phase) for beat signal
};

// ============================================================================
// Output Structure (Moved to contracts/TempoOutput.h)
// ============================================================================


// ============================================================================
// TempoTracker Class
// ============================================================================

/**
 * @brief Goertzel-based tempo tracker
 *
 * Architecture:
 *   8-band Goertzel + RMS -> updateNovelty() -> spectral/VU buffers (separate)
 *                                                      |
 *   updateTempo() <- interleaved Goertzel (hybrid input) <- -+
 *         |
 *   advancePhase() -> winner phase tracking
 *         |
 *   getOutput() -> TempoOutput for effects
 *
 * Call sequence per audio frame:
 *   1. updateNovelty() - log spectral flux (31.25 Hz) and VU derivative (62.5 Hz)
 *   2. updateTempo()   - compute Goertzel magnitudes with hybrid input (interleaved)
 *   3. advancePhase()  - track winner phase
 *   4. getOutput()     - read current state
 *
 * Memory: ~22 KB total
 *   - TempoBin[96]: 5.4 KB
 *   - History buffers: 16 KB
 *   - Smooth array: 0.4 KB
 */
class TempoTracker {
public:
    /**
     * @brief Initialize all state
     *
     * Must be called before any other method.
     * Computes Goertzel coefficients and clears buffers.
     */
    void init();

    /**
     * @brief Update tuning parameters
     * @param tuning New tuning configuration
     */
    void setTuning(const TempoTrackerTuning& tuning) { tuning_ = tuning; }

    /**
     * @brief Update novelty from 8-band Goertzel and RMS
     *
     * Dual-rate architecture:
     * - Spectral flux computed from 8-band deltas when bands_ready=true (31.25 Hz)
     * - VU derivative computed from RMS every call (62.5 Hz)
     *
     * @param bands 8-band Goertzel magnitudes (can be nullptr if not ready)
     * @param num_bands Number of bands (8)
     * @param rms Current RMS value [0,1]
     * @param bands_ready True when fresh 8-band data available (31.25 Hz)
     */
    void updateNovelty(const float* bands, uint8_t num_bands, float rms, bool bands_ready);

    /**
     * @brief Update Goertzel magnitudes (interleaved)
     *
     * Computes 2 bins per call to spread CPU load across frames.
     * Updates winner selection with hysteresis.
     *
     * @param delta_sec Time since last call (for phase integration)
     */
    void updateTempo(float delta_sec);

    /**
     * @brief Advance winner phase
     *
     * Integrates phase at winner's BPM rate and detects beat ticks.
     *
     * @param delta_sec Time since last call
     */
    void advancePhase(float delta_sec);

    /**
     * @brief Get current output state
     *
     * @return TempoOutput with BPM, phase, confidence, beat_tick
     */
    TempoOutput getOutput() const;

    float getNovelty() const { return 0.0f; } // Placeholder for compatibility with debug logging

    // ========================================================================
    // Debug Accessors
    // ========================================================================

    /**
     * @brief Get all tempo bins (for visualization)
     */
    const TempoBin* getBins() const { return tempi_; }

    /**
     * @brief Get smoothed magnitudes (for spectrum display)
     */
    const float* getSmoothed() const { return tempi_smooth_; }

    /**
     * @brief Get current winner bin index
     */
    uint16_t getWinnerBin() const { return winner_bin_; }

    /**
     * @brief Get spectral novelty history (for debugging)
     */
    const float* getSpectralHistory() const { return spectral_curve_; }

    /**
     * @brief Get VU history (for debugging)
     */
    const float* getVuHistory() const { return vu_curve_; }

    /**
     * @brief Get current spectral history write index
     */
    uint16_t getSpectralIndex() const { return spectral_index_; }

    /**
     * @brief Get current VU history write index
     */
    uint16_t getVuIndex() const { return vu_index_; }

private:
    // ========================================================================
    // Internal Methods
    // ========================================================================

    /**
     * @brief Compute Goertzel magnitude for a single bin
     *
     * Runs Goertzel IIR filter over novelty history and extracts
     * magnitude and phase.
     *
     * @param bin Bin index [0, NUM_TEMPI)
     * @return Raw magnitude (before normalization)
     */
    float computeMagnitude(uint16_t bin);

    /**
     * @brief Normalize a buffer with adaptive scaling
     *
     * Uses exponential moving average of max value for stable normalization.
     *
     * @param buffer Source buffer
     * @param normalized Destination buffer
     * @param scale Current scale factor (updated)
     * @param tau Adaptation time constant
     * @param length Buffer length
     */
    void normalizeBuffer(float* buffer, float* normalized, float& scale, float tau, uint16_t length);

    /**
     * @brief Update winner bin with hysteresis
     *
     * Requires challenger to maintain 10% advantage for 5 consecutive frames.
     */
    void updateWinner();

    /**
     * @brief Validate and clamp winner_bin_ to safe range
     *
     * Returns a safe bin index in [0, NUM_TEMPI-1]. If winner_bin_ is corrupted
     * or out of bounds, returns a safe default (NUM_TEMPI / 2).
     *
     * @return Valid bin index in [0, NUM_TEMPI-1]
     */
    uint16_t validateWinnerBin() const;

private:
    // ========================================================================
    // State Variables
    // ========================================================================

    TempoTrackerTuning tuning_;

    // Tempo bins (96 x 56 bytes = 5.4 KB)
    TempoBin tempi_[NUM_TEMPI];

    // Smoothed magnitudes for winner selection (96 x 4 bytes = 384 bytes)
    float tempi_smooth_[NUM_TEMPI];

    // Spectral novelty buffers (31.25 Hz, ~8 KB total)
    float spectral_curve_[SPECTRAL_HISTORY_LENGTH];
    uint16_t spectral_index_;
    float bands_last_[8];  // Last 8-band values for spectral flux delta

    // VU novelty buffers (62.5 Hz, ~4 KB total)
    float vu_curve_[VU_HISTORY_LENGTH];
    uint16_t vu_index_;
    float rms_last_;
    float vu_accum_;
    uint8_t vu_accum_count_;

    // Dynamic scaling state
    float novelty_scale_;
    float vu_scale_;
    uint8_t spectral_scale_count_;   ///< Counter for spectral scale updates (31.25 Hz)
    uint8_t vu_scale_count_;        ///< Counter for VU scale updates (62.5 Hz)
    uint16_t calc_bin_;

    // Winner tracking
    uint16_t winner_bin_;
    uint16_t candidate_bin_;
    uint8_t candidate_frames_;
    float power_sum_;
    float confidence_;

    // Output state
    float current_phase_;
    bool beat_tick_;
    uint32_t last_tick_ms_;
    uint32_t time_ms_;

    // Silence detection
    bool silence_detected_ = false;
    float silence_level_ = 0.0f;
    void checkSilence();
};

} // namespace audio
} // namespace lightwaveos

