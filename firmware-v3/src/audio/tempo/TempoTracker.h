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
 * - Dual-rate architecture: spectral (25 Hz) + VU (50 Hz) @ 12.8kHz sample rate
 * - Dynamic scaling with rate-matched updates (spectral: 25 Hz, VU: 50 Hz)
 *
 * Design goals (Emotiscope alignment with Expert Review fixes):
 * - 96 bins (48-143 BPM) at 1.0 BPM probe spacing (~3 BPM resolving power)
 * - ~28 KB total memory footprint (1024-sample buffer + window LUT)
 * - Time-based hysteresis (200ms) for stable tempo lock
 * - Exponential decay window for recency-weighted beat detection
 * - Interleaved Goertzel computation for CPU efficiency
 *
 * @author LightwaveOS Team
 * @version 1.0.0
 */

#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace lightwaveos {
namespace audio {

// ============================================================================
// Configuration Constants
// ============================================================================

/// Number of tempo bins (1 BPM resolution from 48-144)
constexpr uint16_t NUM_TEMPI = 96;

/// Number of frequency bins for spectral flux (matches Emotiscope's NUM_FREQS)
/// 64 semitone-spaced bins from A1 (55 Hz) to C7 (2093 Hz)
constexpr uint16_t NUM_FREQS = 64;

/// BPM range - Emotiscope alignment (48-143 BPM)
/// Covers ambient/downtempo while avoiding octave ambiguity at high tempos
constexpr float TEMPO_LOW = 48.0f;
constexpr float TEMPO_HIGH = TEMPO_LOW + static_cast<float>(NUM_TEMPI - 1);  // 143 BPM

/// Spectral novelty rate - Emotiscope alignment (50 Hz)
/// Provides 20ms temporal resolution for transient detection
/// CRITICAL: Higher rate captures kick/snare attacks better than 25 Hz
constexpr float SPECTRAL_LOG_HZ = 50.0f;

/// VU novelty rate (every hop = 256 samples @ 12.8kHz)
/// RMS derivative is computed every audio hop for transient detection
/// CRITICAL: Must match actual sample rate (12800/256 = 50 Hz)
constexpr float VU_LOG_HZ = 50.0f;

/// Spectral history length (~20.48 seconds at 50 Hz)
/// Emotiscope alignment: 1024 samples provides ~3 BPM resolving power
constexpr uint16_t SPECTRAL_HISTORY_LENGTH = 1024;

/// VU history length (~10 seconds at 50 Hz)
constexpr uint16_t VU_HISTORY_LENGTH = 512;

/// Phase shift for beat alignment (percentage of beat period)
constexpr float BEAT_SHIFT_PERCENT = 0.08f;

/// Reference FPS for phase velocity calculation
constexpr float REFERENCE_FPS = 100.0f;

/// Novelty decay multiplier per frame
/// Applied to history buffers to fade old beat events
constexpr float NOVELTY_DECAY = 0.999f;

/// Time-based hysteresis (Expert Review FIX #3)
/// Preserves consistent 200ms hysteresis regardless of novelty rate
constexpr float HYSTERESIS_TIME_MS = 200.0f;
constexpr int HYSTERESIS_FRAMES = static_cast<int>(HYSTERESIS_TIME_MS * SPECTRAL_LOG_HZ / 1000.0f);  // 10 frames at 50 Hz

/// Exponential decay window rate (Expert Review FIX #1)
/// Controls rolloff steepness for recency-weighted novelty
constexpr float WINDOW_DECAY_RATE = 5.0f;

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
// Output Structure
// ============================================================================

/**
 * @brief Tempo tracker output for effect consumption
 *
 * Provides all the information effects need for beat-synchronized visuals.
 * Named TempoTrackerOutput to avoid ODR conflict with contracts/TempoOutput.h
 */
struct TempoTrackerOutput {
    float bpm;                      ///< Current detected tempo
    float phase01;                  ///< Phase [0, 1) - 0 = beat instant
    float confidence;               ///< Confidence [0, 1] - how dominant this tempo is
    bool beat_tick;                 ///< True for one frame at beat instant
    bool locked;                    ///< True if confidence > threshold
    float beat_strength;            ///< Smoothed magnitude of winning bin
};

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
 *   getOutput() -> TempoTrackerOutput for effects
 *
 * Call sequence per audio frame:
 *   1. updateNovelty() - log spectral flux (25 Hz) and VU derivative (50 Hz) @ 12.8kHz
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
     * @brief Update novelty from 64-bin Goertzel and RMS (Emotiscope parity)
     *
     * Dual-rate architecture:
     * - Spectral flux computed from 64-bin deltas when bins_ready=true (25 Hz @ 12.8kHz)
     * - VU derivative computed from RMS every call (50 Hz @ 12.8kHz)
     *
     * @param bins 64-bin Goertzel magnitudes (can be nullptr if not ready)
     * @param num_bins Number of bins (NUM_FREQS = 64)
     * @param rms Current RMS value [0,1]
     * @param bins_ready True when fresh 64-bin data available (25 Hz)
     */
    void updateNovelty(const float* bins, uint16_t num_bins, float rms, bool bins_ready);

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
     * @return TempoTrackerOutput with BPM, phase, confidence, beat_tick
     */
    TempoTrackerOutput getOutput() const;

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
     * @brief Initialize exponential decay window LUT (Expert Review FIX #1)
     *
     * Creates recency-weighted window: newest samples weighted 1.0,
     * oldest samples weighted ~0.007 (with WINDOW_DECAY_RATE=5.0).
     */
    void initWindowLut();

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

    // Tempo bins (96 x 56 bytes = 5.4 KB)
    TempoBin tempi_[NUM_TEMPI];

    // Smoothed magnitudes for winner selection (96 x 4 bytes = 384 bytes)
    float tempi_smooth_[NUM_TEMPI];

    // Spectral novelty buffers (50 Hz, ~4 KB for curve + ~4 KB for window LUT)
    float spectral_curve_[SPECTRAL_HISTORY_LENGTH];
    uint16_t spectral_index_;
    float bins_last_[NUM_FREQS];  // Last 64-bin values for spectral flux delta (Emotiscope parity)

    // Exponential decay window LUT (Expert Review FIX #1)
    // Weights recent samples higher than historical
    float window_lut_[SPECTRAL_HISTORY_LENGTH];

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

