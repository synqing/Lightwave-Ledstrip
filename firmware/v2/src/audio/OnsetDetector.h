/**
 * @file OnsetDetector.h
 * @brief Spectral flux onset detection from Sensory Bridge / Emotiscope
 * 
 * FROM SENSORY BRIDGE / EMOTISCOPE - DO NOT MODIFY WITHOUT EVIDENCE
 * 
 * This module implements onset detection using spectral flux with half-wave
 * rectification. It takes frequency bin magnitudes from Goertzel analysis
 * and produces a novelty curve that indicates musical onsets (note attacks).
 * 
 * CANONICAL SOURCES:
 * - Sensory Bridge 4.1.1 GDFT.h calculate_novelty()
 * - Emotiscope 2.0 global_defines.h tempo constants
 * 
 * SPECIFICATION DOCUMENTS:
 * - planning/audio-pipeline-redesign/HARDENED_SPEC.md §5.1
 * - planning/audio-pipeline-redesign/prd.md §5.3
 * - planning/audio-pipeline-redesign/MAGIC_NUMBERS.md §4
 * 
 * ALGORITHM: Spectral Flux with Half-Wave Rectification
 * 1. Calculate difference: current_magnitude[i] - previous_magnitude[i]
 * 2. Half-wave rectify: Keep only positive differences (energy increases)
 * 3. Sum across all bins
 * 4. Normalize by bin count
 * 5. Perceptual scaling: Square root for dynamic range compression
 * 
 * WHY THIS WORKS:
 * - Energy *increases* indicate note onsets (attacks)
 * - Energy *decreases* are just decay (not musically interesting)
 * - Sum across bins captures overall spectral novelty
 * - Square root makes quiet and loud onsets more comparable
 * 
 * TIMING: Target <1ms per hop
 * MEMORY: ~8KB for novelty history buffer
 * 
 * @version 1.0.0
 * @date 2026-01-13
 */

#pragma once

#include <cstdint>
#include <cmath>
#include "AudioCanonicalConfig.h"

namespace LightwaveOS {
namespace Audio {

// ============================================================================
// CANONICAL CONSTANTS (FROM EMOTISCOPE 2.0)
// ============================================================================

/**
 * @brief Number of frequency bins from Goertzel analysis
 * FROM: Emotiscope global_defines.h NUM_FREQS
 * MUST MATCH: GoertzelDFT::LWOS_NUM_FREQS (64)
 */
constexpr uint16_t ONSET_NUM_FREQS = LWOS_NUM_FREQS;

// ============================================================================
// ONSET DETECTOR CLASS
// ============================================================================

/**
 * @brief Spectral flux onset detector with half-wave rectification
 * 
 * Implements canonical Sensory Bridge / Emotiscope onset detection algorithm.
 * Takes Goertzel bin magnitudes as input, produces novelty curve as output.
 * 
 * INPUTS: 64 frequency bin magnitudes (from GoertzelDFT)
 * OUTPUTS: Novelty value (onset strength) and circular history buffer
 * 
 * TIMING: Target <1ms per update @ 240MHz
 * MEMORY: ~8KB for novelty history (1024 floats × 4 bytes)
 */
class OnsetDetector {
public:
    OnsetDetector();
    ~OnsetDetector() = default;

    /**
     * @brief Initialize onset detector
     * 
     * Zeros history buffers and sets initial state.
     * 
     * @return true if initialization successful
     */
    bool init();

    /**
     * @brief Update onset detection with new frequency magnitudes
     * 
     * FROM: Sensory Bridge GDFT.h calculate_novelty()
     * 
     * ALGORITHM:
     * 1. Calculate spectral flux: sum of positive magnitude differences
     * 2. Normalize by bin count
     * 3. Perceptual scaling: square root
     * 4. Store in circular novelty history buffer
     * 5. Update previous magnitudes for next frame
     * 
     * TIMING: <1ms per call
     * 
     * @param magnitudes Current frequency bin magnitudes (64 floats)
     * @return Novelty value (onset strength) [0.0, inf) where higher = stronger onset
     */
    float update(const float* magnitudes);

    /**
     * @brief Get current novelty value (most recent onset strength)
     * 
     * @return Latest novelty value
     */
    float getCurrentNovelty() const { return m_currentNovelty; }

    /**
     * @brief Get novelty history buffer
     * 
     * FOR TEMPO TRACKING: The tempo tracker runs Goertzel on this buffer
     * to detect periodicity in onset events.
     * 
     * @return Pointer to circular novelty history buffer (1024 floats)
     */
    const float* getNoveltyHistory() const { return m_noveltyHistory; }

    /**
     * @brief Get current write position in novelty history
     * 
     * @return Index [0, 1023] of most recently written novelty value
     */
    uint16_t getNoveltyHistoryIndex() const { return m_historyIndex; }

    /**
     * @brief Reset onset detector state
     * 
     * Zeros all buffers. Use when audio session resets or DSP state clears.
     */
    void reset();

private:
    /**
     * @brief Calculate spectral flux from magnitude differences
     * 
     * FROM: Sensory Bridge GDFT.h calculate_novelty()
     * 
     * FORMULA:
     * novelty = sqrt(sum(max(0, current[i] - prev[i])) / NUM_BINS)
     * 
     * @param current Current frame magnitudes
     * @param previous Previous frame magnitudes
     * @return Novelty value (onset strength)
     */
    float calculateSpectralFlux(const float* current, const float* previous);

    // State variables
    float    m_previousMagnitudes[ONSET_NUM_FREQS];   ///< Previous frame for spectral flux
    float    m_noveltyHistory[NOVELTY_HISTORY_LENGTH]; ///< Circular buffer for tempo tracking
    uint16_t m_historyIndex;                          ///< Current write position in history
    float    m_currentNovelty;                        ///< Most recent novelty value
    bool     m_initialized;                           ///< Initialization guard
};

} // namespace Audio
} // namespace LightwaveOS
