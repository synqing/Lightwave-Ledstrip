/**
 * @file ChromaStability.h
 * @brief Chroma vector stability tracking over time
 *
 * Measures harmonic stability by computing cosine similarity between
 * current chroma vector and windowed average. Used to filter out
 * unstable/atonal onsets in tempo tracking.
 *
 * Part of Phase 2C: Feature Extraction Pipeline
 */

#pragma once

#include <cstdint>

/**
 * @class ChromaStability
 * @brief Tracks chroma vector stability over time
 *
 * Computes cosine similarity between current chroma and windowed average
 * to detect harmonic stability. Stable harmony indicates tonal content,
 * while instability may indicate transients or noise.
 *
 * Algorithm:
 * 1. Maintain ring buffer of last N chroma vectors
 * 2. Compute average chroma over window
 * 3. Cosine similarity = dot(current, average) / (||current|| * ||average||)
 * 4. Stability ∈ [0, 1], where 1 = perfectly stable
 *
 * Typical thresholds:
 * - > 0.7: Stable tonal content
 * - 0.4-0.7: Moderately stable
 * - < 0.4: Unstable/atonal
 */
class ChromaStability {
public:
    /**
     * @brief Construct chroma stability tracker
     * @param windowSize Number of frames to average (default: 8)
     *
     * Memory usage: windowSize * 12 * sizeof(float) bytes
     * At 8 frames: 384 bytes
     */
    explicit ChromaStability(uint8_t windowSize = 8);

    /**
     * @brief Destructor - frees history buffer
     */
    ~ChromaStability();

    /**
     * @brief Update stability with new chroma vector
     * @param chroma12 12-bin chroma vector (normalized [0, 1])
     *
     * Updates ring buffer and recomputes stability.
     * Call once per audio frame.
     */
    void update(const float* chroma12);

    /**
     * @brief Get current stability metric
     * @return Stability ∈ [0, 1], where 1 = perfectly stable
     */
    float getStability() const { return m_stability; }

    /**
     * @brief Check if chroma is stable above threshold
     * @param threshold Stability threshold (default: 0.7)
     * @return True if stability >= threshold
     */
    bool isStable(float threshold = 0.7f) const {
        return m_stability >= threshold;
    }

    /**
     * @brief Reset state (clears history)
     */
    void reset();

private:
    /**
     * @brief Compute cosine similarity between current and average chroma
     * @param current Current 12-bin chroma vector
     */
    void computeStability(const float* current);

    float* m_chromaHistory;   ///< Ring buffer [12 * windowSize]
    float m_stability;        ///< Current stability metric [0, 1]
    uint8_t m_windowSize;     ///< Number of frames to average
    uint8_t m_writeIndex;     ///< Ring buffer write position
    uint8_t m_framesRecorded; ///< Number of frames recorded (up to windowSize)
};
