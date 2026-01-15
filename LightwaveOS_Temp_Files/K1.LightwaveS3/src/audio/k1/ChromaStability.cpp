/**
 * @file ChromaStability.cpp
 * @brief Chroma stability implementation
 *
 * @author LightwaveOS Team
 * @version 1.0.0 - K1 Migration
 */

#include "ChromaStability.h"
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace lightwaveos {
namespace audio {
namespace k1 {

ChromaStability::ChromaStability()
    : m_windowSize(8)
    , m_writeIdx(0)
    , m_initialized(false)
{
}

void ChromaStability::init(size_t window_size) {
    if (m_initialized) {
        // Cleanup existing history
        for (auto* frame : m_history) {
            if (frame != nullptr) {
                free(frame);
            }
        }
        m_history.clear();
    }

    m_windowSize = window_size;
    m_writeIdx = 0;

    // Allocate history buffer
    for (size_t i = 0; i < window_size; i++) {
        float* frame = static_cast<float*>(malloc(12 * sizeof(float)));
        if (frame == nullptr) {
            // Cleanup on failure
            for (auto* f : m_history) {
                if (f != nullptr) free(f);
            }
            m_history.clear();
            return;
        }
        memset(frame, 0, 12 * sizeof(float));
        m_history.push_back(frame);
    }

    m_initialized = true;
}

float ChromaStability::cosineSimilarity(const float* a, const float* b) const {
    if (a == nullptr || b == nullptr) {
        return 0.0f;
    }

    float dot = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;

    for (int i = 0; i < 12; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    float denom = std::sqrt(norm_a) * std::sqrt(norm_b);
    if (denom < 0.001f) {
        return 0.0f;
    }

    return dot / denom;
}

float ChromaStability::update(const float* chroma) {
    if (!m_initialized || chroma == nullptr) {
        return 0.0f;
    }

    // Store current frame
    memcpy(m_history[m_writeIdx], chroma, 12 * sizeof(float));

    // Compute average cosine similarity with previous frames
    float sum_similarity = 0.0f;
    size_t count = 0;

    for (size_t i = 0; i < m_windowSize; i++) {
        if (i == m_writeIdx) continue;  // Skip current frame

        float* prev_frame = m_history[i];
        if (prev_frame != nullptr) {
            float sim = cosineSimilarity(chroma, prev_frame);
            sum_similarity += sim;
            count++;
        }
    }

    // Advance write index
    m_writeIdx = (m_writeIdx + 1) % m_windowSize;

    // Return average similarity
    if (count > 0) {
        return sum_similarity / static_cast<float>(count);
    }
    return 0.0f;
}

void ChromaStability::reset() {
    if (m_initialized) {
        for (auto* frame : m_history) {
            if (frame != nullptr) {
                memset(frame, 0, 12 * sizeof(float));
            }
        }
        m_writeIdx = 0;
    }
}

ChromaStability::~ChromaStability() {
    for (auto* frame : m_history) {
        if (frame != nullptr) {
            free(frame);
        }
    }
    m_history.clear();
}

} // namespace k1
} // namespace audio
} // namespace lightwaveos

