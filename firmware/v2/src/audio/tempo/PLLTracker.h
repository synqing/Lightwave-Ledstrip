/**
 * @file PLLTracker.h
 * @brief Phase-Locked Loop tempo tracker from Emotiscope 2.0
 * 
 * FROM EMOTISCOPE 2.0 - DO NOT MODIFY WITHOUT EVIDENCE
 * 
 * This module implements tempo detection using a second-stage Goertzel algorithm
 * that runs on the novelty curve (not raw audio). It detects periodicity in
 * onset events to estimate tempo (BPM) and track beat phase for synchronization.
 * 
 * CANONICAL SOURCES:
 * - Emotiscope 2.0 tempo.h (init/calculate/update functions)
 * - Emotiscope 2.0 global_defines.h (tempo constants)
 * 
 * SPECIFICATION DOCUMENTS:
 * - planning/audio-pipeline-redesign/HARDENED_SPEC.md §6
 * - planning/audio-pipeline-redesign/prd.md §5.4
 * - planning/audio-pipeline-redesign/MAGIC_NUMBERS.md §5
 * 
 * ALGORITHM: Two-Stage Goertzel + PLL
 * 1. Run Goertzel on novelty curve (96 tempo bins from 60-156 BPM)
 * 2. Find dominant tempo (highest magnitude)
 * 3. Extract phase via atan2(imag, real) for beat sync
 * 4. Smooth magnitudes over time (0.975 retention, 0.025 update)
 * 5. Calculate confidence (dominant / sum)
 * 
 * KEY INSIGHT: This is SECOND-STAGE Goertzel
 * - Stage 1: Audio → Frequency bins (GoertzelDFT)
 * - Stage 2: Novelty curve → Tempo bins (PLLTracker)
 * 
 * TIMING: Target <2ms per update (96 tempo bins)
 * MEMORY: ~10KB for tempo bins + smoothing buffers
 * 
 * @version 1.0.0
 * @date 2026-01-13
 */

#pragma once

#include <cstdint>
#include <cmath>
#include "../AudioCanonicalConfig.h"

namespace LightwaveOS {
namespace Audio {

// ============================================================================
// CANONICAL CONSTANTS (FROM EMOTISCOPE 2.0)
// ============================================================================

/**
 * @brief Number of tempo bins to track
 * FROM: Emotiscope global_defines.h NUM_TEMPI
 * WHY: 96 bins provides 1 BPM resolution from 60-156 BPM
 */
constexpr uint8_t NUM_TEMPO_BINS = 96;

/**
 * @brief Minimum tempo in BPM
 * FROM: Emotiscope global_defines.h TEMPO_LOW
 * WHY: 60 BPM is slowest typical music tempo
 */
constexpr uint8_t TEMPO_LOW_BPM = 60;

/**
 * @brief Maximum tempo in BPM
 * FROM: Emotiscope global_defines.h TEMPO_HIGH
 * CALCULATED: TEMPO_LOW + NUM_TEMPI = 60 + 96 = 156
 */
constexpr uint8_t TEMPO_HIGH_BPM = 156;

/**
 * @brief Reference frame rate for phase synchronization
 * FROM: Emotiscope global_defines.h REFERENCE_FPS
 * WHY: Phase advances 100 times per second for smooth sync
 */
constexpr uint8_t REFERENCE_FPS = 100;

/**
 * @brief Beat phase shift adjustment
 * FROM: Emotiscope global_defines.h BEAT_SHIFT_PERCENT
 * WHY: 0.0 = no shift (phase aligned to onset peaks)
 */
constexpr float BEAT_SHIFT_PERCENT = 0.0f;

// ============================================================================
// TEMPO BIN STRUCTURE (FROM EMOTISCOPE tempo.h)
// ============================================================================

/**
 * @brief Tempo bin metadata for Goertzel analysis on novelty curve
 * 
 * FROM: Emotiscope 2.0 tempo.h struct tempo
 * 
 * Each bin tracks one specific BPM value with:
 * - Goertzel coefficients for tempo detection
 * - Magnitude (tempo strength)
 * - Phase (beat position within measure)
 * - Phase advance rate (for synchronization)
 */
struct TempoBin {
    float    targetHz;           ///< Tempo in Hz (BPM / 60.0)
    float    targetBpm;          ///< Human-readable BPM
    uint16_t blockSize;          ///< Goertzel window size (samples)
    float    windowStep;         ///< Window lookup increment (4096 / blockSize)
    float    cosine;             ///< cos(w) for Goertzel
    float    sine;               ///< sin(w) for phase calculation
    float    coeff;              ///< 2 * cos(w) - Goertzel coefficient
    float    magnitude;          ///< Raw Goertzel magnitude
    float    magnitudeNorm;      ///< Normalized magnitude [0, 1]
    float    phase;              ///< Beat phase [-PI, PI]
    float    phasePerFrame;      ///< Phase advance per reference frame
    bool     phaseInverted;      ///< Tracks downbeat vs offbeat
};

// ============================================================================
// TEMPO TRACKER CLASS
// ============================================================================

/**
 * @brief Phase-Locked Loop tempo tracker with Goertzel on novelty curve
 * 
 * Implements canonical Emotiscope 2.0 tempo tracking algorithm.
 * Takes novelty curve as input, produces BPM and beat phase as output.
 * 
 * INPUTS: Novelty curve history (1024 samples @ 50Hz)
 * OUTPUTS: Dominant BPM, confidence, beat phase, on-beat flag
 * 
 * TIMING: Target <2ms per update @ 240MHz
 * MEMORY: ~10KB for tempo bins + smoothing buffers
 */
class PLLTracker {
public:
    PLLTracker();
    ~PLLTracker() = default;

    /**
     * @brief Initialize tempo tracker
     * 
     * FROM: Emotiscope tempo.h init_tempo_goertzel_constants()
     * 
     * Precomputes Goertzel coefficients for all 96 tempo bins.
     * 
     * @return true if initialization successful
     */
    bool init();

    /**
     * @brief Update tempo tracking with novelty history
     * 
     * FROM: Emotiscope tempo.h calculate_magnitude_of_tempo() + update_tempi_phase()
     * 
     * ALGORITHM:
     * 1. Run Goertzel on novelty curve for all tempo bins
     * 2. Extract magnitude and phase for each tempo
     * 3. Smooth magnitudes over time
     * 4. Find dominant tempo (highest magnitude)
     * 5. Calculate confidence
     * 6. Detect beat events (phase zero crossings)
     * 
     * @param noveltyHistory Circular novelty buffer (1024 floats)
     * @param historyIndex Current write position in buffer
     * @param deltaTimeMs Time since last update (milliseconds)
     */
    void update(const float* noveltyHistory, uint16_t historyIndex, float deltaTimeMs);

    /**
     * @brief Get dominant tempo in BPM
     * 
     * @return BPM [60, 156] of strongest detected tempo
     */
    float getDominantBPM() const { return m_dominantBpm; }

    /**
     * @brief Get tempo confidence
     * 
     * @return Confidence [0, 1] where 1.0 = very confident single tempo
     */
    float getConfidence() const { return m_confidence; }

    /**
     * @brief Get beat phase of dominant tempo
     * 
     * @return Phase [-PI, PI] where 0 = downbeat
     */
    float getBeatPhase() const { return m_beatPhase; }

    /**
     * @brief Check if currently on downbeat vs offbeat
     * 
     * @return false = downbeat, true = offbeat
     */
    bool isPhaseInverted() const { return m_phaseInverted; }

    /**
     * @brief Check if on beat (phase zero crossing)
     * 
     * @return true when beat occurs (phase crosses zero)
     */
    bool isOnBeat() const { return m_onBeat; }

    /**
     * @brief Reset tempo tracker state
     * 
     * Zeros all buffers and phase state.
     */
    void reset();

private:
    /**
     * @brief Precompute Goertzel coefficients for all tempo bins
     * 
     * FROM: Emotiscope tempo.h init_tempo_goertzel_constants()
     */
    void precomputeTempoCoefficients();

    /**
     * @brief Calculate tempo magnitude and phase using Goertzel
     * 
     * FROM: Emotiscope tempo.h calculate_magnitude_of_tempo()
     * 
     * Runs Goertzel on novelty curve for one tempo bin.
     * 
     * @param tempoBin Tempo bin index [0, 95]
     * @param noveltyHistory Circular novelty buffer
     * @param historyIndex Current position
     */
    void calculateTempoMagnitude(uint16_t tempoBin, const float* noveltyHistory, uint16_t historyIndex);

    /**
     * @brief Update tempo phase and detect beats
     * 
     * FROM: Emotiscope tempo.h update_tempi_phase() + sync_beat_phase()
     * 
     * @param deltaFrames Time delta in reference frames (@ 100 FPS)
     */
    void updateTempoPhases(float deltaFrames);

    // Tempo bins and state
    TempoBin m_tempoBins[NUM_TEMPO_BINS];       ///< Tempo bin metadata (96 bins)
    float    m_tempoSmooth[NUM_TEMPO_BINS];     ///< Smoothed magnitudes
    uint16_t m_dominantBin;                     ///< Index of strongest tempo
    float    m_dominantBpm;                     ///< BPM of dominant tempo
    float    m_confidence;                      ///< Tempo confidence [0, 1]
    float    m_beatPhase;                       ///< Phase of dominant tempo [-PI, PI]
    bool     m_phaseInverted;                   ///< Downbeat vs offbeat
    bool     m_onBeat;                          ///< Beat event flag
    float    m_prevPhase;                       ///< Previous phase (for zero crossing)
    bool     m_initialized;                     ///< Initialization guard
};

} // namespace Audio
} // namespace LightwaveOS
