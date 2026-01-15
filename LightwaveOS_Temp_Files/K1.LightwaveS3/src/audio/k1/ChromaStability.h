/**
 * @file ChromaStability.h
 * @brief Rolling cosine similarity for chroma stability
 *
 * Measures how stable the chroma distribution is over time.
 * Uses rolling window of W frames (configurable).
 *
 * @author LightwaveOS Team
 * @version 1.0.0 - K1 Migration
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace lightwaveos {
namespace audio {
namespace k1 {

/**
 * @brief Chroma stability calculator
 *
 * Computes rolling cosine similarity between consecutive chroma frames.
 */
class ChromaStability {
public:
    /**
     * @brief Constructor
     */
    ChromaStability();

    /**
     * @brief Initialize chroma stability calculator
     *
     * @param window_size Rolling window size (number of frames)
     */
    void init(size_t window_size = 8);

    /**
     * @brief Update with new chroma frame
     *
     * @param chroma Current chroma array (12 elements)
     * @return Stability value [0.0, 1.0] (higher = more stable)
     */
    float update(const float* chroma);

    /**
     * @brief Reset state
     */
    void reset();

    /**
     * @brief Destructor
     */
    ~ChromaStability();

private:
    std::vector<float*> m_history;       ///< History buffer (window_size frames)
    size_t m_windowSize;                 ///< Window size
    size_t m_writeIdx;                   ///< Write index (circular)
    bool m_initialized;                  ///< Initialization flag

    /**
     * @brief Compute cosine similarity between two chroma vectors
     *
     * @param a First chroma array
     * @param b Second chroma array
     * @return Cosine similarity [0.0, 1.0]
     */
    float cosineSimilarity(const float* a, const float* b) const;
};

} // namespace k1
} // namespace audio
} // namespace lightwaveos

