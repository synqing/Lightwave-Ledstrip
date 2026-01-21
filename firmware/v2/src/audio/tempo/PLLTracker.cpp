/**
 * @file PLLTracker.cpp
 * @brief Phase-Locked Loop tempo tracker from Emotiscope 2.0
 * 
 * FROM EMOTISCOPE 2.0 - DO NOT MODIFY WITHOUT EVIDENCE
 * 
 * This implementation is a PORT of the Emotiscope tempo tracking algorithm,
 * adapted for LightwaveOS timing (8ms audio hop, 50Hz novelty).
 * 
 * CANONICAL SOURCES:
 * - Emotiscope 2.0 tempo.h (all tempo functions)
 * - Emotiscope 2.0 global_defines.h
 * 
 * SPECIFICATION:
 * - planning/audio-pipeline-redesign/HARDENED_SPEC.md §6
 * 
 * @version 1.0.0
 * @date 2026-01-13
 */

#include "PLLTracker.h"
#include <cstring>  // For memset
#include <cmath>    // For cos, sin, atan2, sqrt

namespace LightwaveOS {
namespace Audio {

// ============================================================================
// MATHEMATICAL CONSTANTS
// ============================================================================

constexpr float PI = 3.141592653589793f;
constexpr float TWO_PI = 6.283185307179586f;

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

PLLTracker::PLLTracker()
    : m_dominantBin(0)
    , m_dominantBpm(120.0f)
    , m_confidence(0.0f)
    , m_beatPhase(0.0f)
    , m_phaseInverted(false)
    , m_onBeat(false)
    , m_prevPhase(0.0f)
    , m_initialized(false)
{
    // Zero-initialize arrays
    memset(m_tempoBins, 0, sizeof(m_tempoBins));
    memset(m_tempoSmooth, 0, sizeof(m_tempoSmooth));
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool PLLTracker::init() {
    if (m_initialized) {
        return true;  // Already initialized
    }

    // Precompute Goertzel coefficients
    precomputeTempoCoefficients();

    // Reset state
    reset();

    m_initialized = true;
    return true;
}

void PLLTracker::reset() {
    // Zero smoothing buffers
    memset(m_tempoSmooth, 0, sizeof(m_tempoSmooth));
    
    // Reset phase tracking
    m_dominantBin = 0;
    m_dominantBpm = 120.0f;  // Default to 120 BPM
    m_confidence = 0.0f;
    m_beatPhase = 0.0f;
    m_phaseInverted = false;
    m_onBeat = false;
    m_prevPhase = 0.0f;
    
    // Reset phase for all bins
    for (uint16_t i = 0; i < NUM_TEMPO_BINS; i++) {
        m_tempoBins[i].phase = 0.0f;
        m_tempoBins[i].phaseInverted = false;
    }
}

// ============================================================================
// GOERTZEL COEFFICIENT PRECOMPUTATION
// FROM: Emotiscope tempo.h:51-118 init_tempo_goertzel_constants()
// ============================================================================

void PLLTracker::precomputeTempoCoefficients() {
    /**
     * Precompute Goertzel constants for all tempo bins
     * 
     * FROM: Emotiscope tempo.h init_tempo_goertzel_constants()
     * 
     * CRITICAL: This runs Goertzel on the NOVELTY CURVE, not raw audio!
     * - Sample rate: NOVELTY_LOG_HZ (50 Hz)
     * - Target frequencies: BPM / 60 (Hz)
     * - Block sizes: Variable based on tempo resolution
     * 
     * FROM: HARDENED_SPEC.md §6.3
     */
    
    for (uint16_t i = 0; i < NUM_TEMPO_BINS; i++) {
        // Target tempo in BPM and Hz
        float bpm = static_cast<float>(TEMPO_LOW_BPM + i);
        m_tempoBins[i].targetBpm = bpm;
        m_tempoBins[i].targetHz = bpm / 60.0f;  // Convert BPM to Hz
        
        // ====================================================================
        // BLOCK SIZE CALCULATION
        // ====================================================================
        
        /**
         * Calculate block size for tempo resolution
         * 
         * Similar to frequency Goertzel, but for tempo domain:
         * - max_distance_hz = 2% of target tempo (tempo resolution)
         * - block_size = sample_rate / (max_distance * 2)
         * 
         * Example: 120 BPM = 2 Hz
         * - max_distance = 2 * 0.02 = 0.04 Hz
         * - block_size = 50 / (0.04 * 2) = 625 samples
         */
        float maxDistHz = m_tempoBins[i].targetHz * 0.02f;
        m_tempoBins[i].blockSize = static_cast<uint16_t>(
            NOVELTY_LOG_HZ / (maxDistHz * 0.5f)
        );
        
        // Clamp to novelty history length
        if (m_tempoBins[i].blockSize > NOVELTY_HISTORY_LENGTH) {
            m_tempoBins[i].blockSize = NOVELTY_HISTORY_LENGTH;
        }
        
        // Window step for variable block sizes
        m_tempoBins[i].windowStep = 4096.0f / m_tempoBins[i].blockSize;
        
        // ====================================================================
        // GOERTZEL COEFFICIENT CALCULATION
        // ====================================================================
        
        /**
         * Calculate Goertzel coefficient for tempo detection
         * 
         * Same formula as frequency Goertzel, but different sample rate:
         * - k = round(block_size * target_hz / NOVELTY_LOG_HZ)
         * - w = 2π * k / block_size
         * - coeff = 2 * cos(w)
         */
        float k = roundf(m_tempoBins[i].blockSize * m_tempoBins[i].targetHz
                         / NOVELTY_LOG_HZ);
        float w = (TWO_PI * k) / m_tempoBins[i].blockSize;
        
        // Store trig values for magnitude AND phase calculation
        m_tempoBins[i].cosine = cosf(w);
        m_tempoBins[i].sine = sinf(w);
        m_tempoBins[i].coeff = 2.0f * m_tempoBins[i].cosine;
        
        // ====================================================================
        // PHASE SYNCHRONIZATION RATE
        // ====================================================================
        
        /**
         * Phase advance per reference frame
         * 
         * This is how much the beat phase advances each 1/100th second.
         * Used for smooth beat synchronization in visual effects.
         * 
         * Formula: (2π * tempo_hz) / REFERENCE_FPS
         * 
         * Example: 120 BPM = 2 Hz
         * - phase_per_frame = (2π * 2) / 100 = 0.1257 radians per frame
         */
        m_tempoBins[i].phasePerFrame =
            (TWO_PI * m_tempoBins[i].targetHz) / REFERENCE_FPS;
    }
}

// ============================================================================
// TEMPO UPDATE (MAIN ENTRY POINT)
// ============================================================================

void PLLTracker::update(const float* noveltyHistory, uint16_t historyIndex, float deltaTimeMs) {
    /**
     * Main tempo tracking update
     * 
     * FROM: Emotiscope tempo.h (multiple functions combined)
     * 
     * ALGORITHM:
     * 1. Run Goertzel on novelty curve for all tempo bins
     * 2. Update tempo phases and smooth magnitudes
     * 3. Find dominant tempo
     * 4. Detect beat events
     */
    
    // Calculate all tempo magnitudes and phases
    for (uint16_t i = 0; i < NUM_TEMPO_BINS; i++) {
        calculateTempoMagnitude(i, noveltyHistory, historyIndex);
    }
    
    // Update phases and smooth magnitudes
    float deltaFrames = deltaTimeMs / (1000.0f / REFERENCE_FPS);
    updateTempoPhases(deltaFrames);
    
    // Update dominant tempo outputs
    m_dominantBpm = m_tempoBins[m_dominantBin].targetBpm;
    m_beatPhase = m_tempoBins[m_dominantBin].phase;
    m_phaseInverted = m_tempoBins[m_dominantBin].phaseInverted;
    
    // Detect beat (phase zero crossing)
    // Beat occurs when phase goes from negative to positive
    m_onBeat = (m_prevPhase < 0.0f && m_beatPhase >= 0.0f);
    m_prevPhase = m_beatPhase;
}

// ============================================================================
// TEMPO MAGNITUDE CALCULATION
// FROM: Emotiscope tempo.h:152-215 calculate_magnitude_of_tempo()
// ============================================================================

void PLLTracker::calculateTempoMagnitude(uint16_t tempoBin, const float* noveltyHistory,
                                         uint16_t historyIndex) {
    /**
     * Calculate tempo magnitude and phase using Goertzel on novelty curve
     * 
     * FROM: Emotiscope tempo.h calculate_magnitude_of_tempo()
     * 
     * CRITICAL DIFFERENCES FROM AUDIO GOERTZEL:
     * 1. Input: Novelty curve (not raw audio)
     * 2. Sample rate: 50 Hz (not 16 kHz)
     * 3. Phase extraction: Uses atan2(imag, real) for beat sync
     * 4. Both magnitude AND phase are extracted
     * 
     * FROM: HARDENED_SPEC.md §6.4
     */
    
    float q1 = 0.0f, q2 = 0.0f;
    const uint16_t blockSize = m_tempoBins[tempoBin].blockSize;
    const float coeff = m_tempoBins[tempoBin].coeff;
    
    // Goertzel iteration on novelty curve (circular buffer)
    for (uint16_t i = 0; i < blockSize; i++) {
        // Read from circular novelty history (newest first)
        int32_t index = historyIndex - i;
        if (index < 0) {
            index += NOVELTY_HISTORY_LENGTH;
        }
        
        float sample = noveltyHistory[index];
        
        // Goertzel iteration (no windowing for now - simplified)
        // TODO: Add window function for better spectral resolution
        float q0 = coeff * q1 - q2 + sample;
        q2 = q1;
        q1 = q0;
    }
    
    // ====================================================================
    // PHASE CALCULATION (CRITICAL FOR BEAT SYNC)
    // ====================================================================
    
    /**
     * Extract phase via atan2(imag, real)
     * 
     * This is different from frequency Goertzel, which only needs magnitude.
     * For tempo tracking, we need phase to know WHERE we are in the beat.
     * 
     * Real part: q1 - q2 * cos(w)
     * Imaginary part: q2 * sin(w)
     * Phase: atan2(imag, real)
     */
    float real = q1 - q2 * m_tempoBins[tempoBin].cosine;
    float imag = q2 * m_tempoBins[tempoBin].sine;
    
    // Calculate phase with optional beat shift
    m_tempoBins[tempoBin].phase = atan2f(imag, real) + (PI * BEAT_SHIFT_PERCENT);
    
    // ====================================================================
    // MAGNITUDE CALCULATION
    // ====================================================================
    
    /**
     * Calculate magnitude from Goertzel state
     * 
     * Same formula as frequency Goertzel:
     * magnitude² = q1² + q2² - q1 * q2 * coeff
     */
    float magSquared = q1 * q1 + q2 * q2 - q1 * q2 * coeff;
    float magnitude = sqrtf(magSquared);
    
    // Normalize by block size / 2
    m_tempoBins[tempoBin].magnitudeNorm = magnitude / (blockSize * 0.5f);
}

// ============================================================================
// TEMPO PHASE UPDATE AND SMOOTHING
// FROM: Emotiscope tempo.h:371-407 update_tempi_phase() + sync_beat_phase()
// ============================================================================

void PLLTracker::updateTempoPhases(float deltaFrames) {
    /**
     * Update tempo phases and smooth magnitudes
     * 
     * FROM: Emotiscope tempo.h update_tempi_phase() + sync_beat_phase()
     * 
     * ALGORITHM:
     * 1. Smooth tempo magnitudes (active: 97.5% old + 2.5% new, inactive: 99.5% decay)
     * 2. Advance phase for active tempos
     * 3. Find dominant tempo (highest smoothed magnitude)
     * 4. Calculate confidence (dominant / sum)
     * 
     * FROM: HARDENED_SPEC.md §6.5, §6.6
     */
    
    float maxMag = 0.0f;
    float sumMag = 0.0f;
    
    for (uint16_t i = 0; i < NUM_TEMPO_BINS; i++) {
        float mag = m_tempoBins[i].magnitudeNorm;
        
        // ====================================================================
        // MAGNITUDE SMOOTHING
        // ====================================================================
        
        /**
         * Smooth tempo magnitudes over time
         * 
         * FROM: Emotiscope tempo.h update_tempi_phase()
         * 
         * Active tempos (mag > 0.005):
         * - smooth = smooth * 0.975 + mag * 0.025
         * - Time constant: ~0.79 seconds to 63% convergence @ 50Hz
         * 
         * Inactive tempos (mag <= 0.005):
         * - smooth = smooth * 0.995
         * - Slow decay for tempo memory
         */
        if (mag > 0.005f) {
            // Smooth active tempos
            m_tempoSmooth[i] = m_tempoSmooth[i] * 0.975f + mag * 0.025f;
            
            // ================================================================
            // PHASE SYNCHRONIZATION
            // ================================================================
            
            /**
             * Advance beat phase based on tempo and time delta
             * 
             * FROM: Emotiscope tempo.h sync_beat_phase()
             * 
             * Phase advances continuously at tempo rate:
             * - phase += phasePerFrame * deltaFrames
             * 
             * CRITICAL: Wrap at PI (not 2*PI) to track half-beats:
             * - phaseInverted = false: Downbeat
             * - phaseInverted = true: Offbeat
             */
            float advance = m_tempoBins[i].phasePerFrame * deltaFrames;
            m_tempoBins[i].phase += advance;
            
            // Wrap at PI for half-beat tracking
            if (m_tempoBins[i].phase > PI) {
                m_tempoBins[i].phase -= TWO_PI;
                m_tempoBins[i].phaseInverted = !m_tempoBins[i].phaseInverted;
            }
        } else {
            // Decay inactive tempos
            m_tempoSmooth[i] *= 0.995f;
        }
        
        // Track maximum and sum for confidence
        sumMag += m_tempoSmooth[i];
        if (m_tempoSmooth[i] > maxMag) {
            maxMag = m_tempoSmooth[i];
            m_dominantBin = i;
        }
    }
    
    // ====================================================================
    // CONFIDENCE CALCULATION
    // ====================================================================
    
    /**
     * Calculate tempo confidence
     * 
     * confidence = dominant_magnitude / sum_of_all_magnitudes
     * 
     * Range: [0, 1]
     * - 1.0 = very confident single tempo
     * - 0.5 = two equally strong tempos
     * - 0.0 = no tempo detected
     */
    m_confidence = (sumMag > 0.0f) ? (maxMag / sumMag) : 0.0f;
}

} // namespace Audio
} // namespace LightwaveOS
