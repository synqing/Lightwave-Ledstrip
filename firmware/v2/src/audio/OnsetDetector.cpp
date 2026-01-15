/**
 * @file OnsetDetector.cpp
 * @brief Spectral flux onset detection from Sensory Bridge / Emotiscope
 * 
 * FROM SENSORY BRIDGE / EMOTISCOPE - DO NOT MODIFY WITHOUT EVIDENCE
 * 
 * This implementation is a VERBATIM PORT of the Sensory Bridge onset
 * detection algorithm (calculate_novelty function).
 * 
 * CANONICAL SOURCE:
 * - Sensory Bridge 4.1.1 GDFT.h calculate_novelty()
 * 
 * SPECIFICATION:
 * - planning/audio-pipeline-redesign/HARDENED_SPEC.md §5.1
 * 
 * @version 1.0.0
 * @date 2026-01-13
 */

#include "OnsetDetector.h"
#include <cstring>  // For memset

namespace LightwaveOS {
namespace Audio {

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

OnsetDetector::OnsetDetector()
    : m_historyIndex(0)
    , m_currentNovelty(0.0f)
    , m_initialized(false)
{
    // Zero-initialize arrays
    memset(m_previousMagnitudes, 0, sizeof(m_previousMagnitudes));
    memset(m_noveltyHistory, 0, sizeof(m_noveltyHistory));
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool OnsetDetector::init() {
    if (m_initialized) {
        return true;  // Already initialized
    }

    // Reset state
    reset();

    m_initialized = true;
    return true;
}

void OnsetDetector::reset() {
    // Zero all buffers
    memset(m_previousMagnitudes, 0, sizeof(m_previousMagnitudes));
    memset(m_noveltyHistory, 0, sizeof(m_noveltyHistory));
    
    // Reset indices
    m_historyIndex = 0;
    m_currentNovelty = 0.0f;
}

// ============================================================================
// ONSET DETECTION (FROM SENSORY BRIDGE calculate_novelty)
// ============================================================================

float OnsetDetector::update(const float* magnitudes) {
    /**
     * Calculate onset strength using spectral flux
     * 
     * FROM: Sensory Bridge GDFT.h:201-242 calculate_novelty()
     * 
     * ALGORITHM:
     * 1. For each frequency bin: current - previous
     * 2. Half-wave rectify: only positive differences (energy increases)
     * 3. Sum all positive differences
     * 4. Normalize by bin count
     * 5. Perceptual scaling: square root
     * 6. Store in circular buffer
     * 7. Update previous frame
     * 
     * FROM: HARDENED_SPEC.md §5.1
     */
    
    // Calculate spectral flux
    m_currentNovelty = calculateSpectralFlux(magnitudes, m_previousMagnitudes);
    
    // Store in circular history buffer for tempo tracking
    m_noveltyHistory[m_historyIndex] = m_currentNovelty;
    
    // Advance circular buffer index
    m_historyIndex++;
    if (m_historyIndex >= NOVELTY_HISTORY_LENGTH) {
        m_historyIndex = 0;
    }
    
    // Update previous magnitudes for next frame
    memcpy(m_previousMagnitudes, magnitudes, sizeof(m_previousMagnitudes));
    
    return m_currentNovelty;
}

// ============================================================================
// SPECTRAL FLUX CALCULATION
// FROM: Sensory Bridge GDFT.h:201-242 - DO NOT MODIFY
// ============================================================================

float OnsetDetector::calculateSpectralFlux(const float* current, const float* previous) {
    /**
     * Spectral flux with half-wave rectification
     * 
     * FROM: Sensory Bridge GDFT.h calculate_novelty()
     * 
     * FORMULA:
     * for each bin i:
     *     diff = current[i] - previous[i]
     *     if diff > 0:
     *         sum += diff
     * 
     * novelty = sqrt(sum / NUM_FREQS)
     * 
     * WHY HALF-WAVE RECTIFICATION:
     * - Energy INCREASES indicate note onsets (attacks)
     * - Energy DECREASES are just decay (not musically interesting)
     * - This is the key insight that makes spectral flux work
     * 
     * WHY SQUARE ROOT:
     * - Perceptual scaling (compresses dynamic range)
     * - Makes quiet and loud onsets more comparable
     * - Follows Stevens' power law for loudness perception
     */
    
    float noveltySum = 0.0f;
    
    // Sum positive differences across all frequency bins
    for (uint16_t i = 0; i < ONSET_NUM_FREQS; i++) {
        // Calculate spectral difference
        float diff = current[i] - previous[i];
        
        // Half-wave rectification: only count energy INCREASES
        // FROM: Sensory Bridge - "if (novelty_bin < 0.0) novelty_bin = 0.0"
        if (diff > 0.0f) {
            noveltySum += diff;
        }
    }
    
    // Normalize by bin count
    // FROM: Sensory Bridge - "novelty_now /= NUM_FREQS"
    float noveltyNormalized = noveltySum / ONSET_NUM_FREQS;
    
    // Perceptual scaling: square root for dynamic range compression
    // FROM: Sensory Bridge - "novelty_curve[i] = sqrt(float(novelty_now))"
    float novelty = sqrtf(noveltyNormalized);
    
    return novelty;
}

} // namespace Audio
} // namespace LightwaveOS
