/**
 * @file NoiseFloor.cpp
 * @brief Implementation of adaptive noise floor tracker
 *
 * @author LightwaveOS Team
 * @version 2.0.0 - Tempo Tracking Dual-Bank Architecture
 */

#include "NoiseFloor.h"
#include <cstring>
#include <cstdlib>

namespace lightwaveos {
namespace audio {

// ============================================================================
// Initialization
// ============================================================================

NoiseFloor::NoiseFloor(float timeConstant, uint8_t numBins)
    : m_floor(nullptr),
      m_numBins(numBins),
      m_alpha(0.0f),
      m_timeConstant(timeConstant)
{
    // Allocate per-bin storage
    m_floor = static_cast<float*>(malloc(numBins * sizeof(float)));

    // Compute alpha decay factor
    // For 62.5 Hz hop rate (16ms hop duration):
    // alpha = 1.0 - exp(-hopDuration / timeConstant)
    const float hopDuration = 0.016f;  // 16ms at 62.5 Hz
    m_alpha = 1.0f - expf(-hopDuration / timeConstant);

    // Clamp alpha to reasonable range
    m_alpha = std::max(0.001f, std::min(0.5f, m_alpha));

    // Initialize floor values to small epsilon
    reset();
}

NoiseFloor::~NoiseFloor() {
    if (m_floor != nullptr) {
        free(m_floor);
        m_floor = nullptr;
    }
}

// ============================================================================
// Core Operations
// ============================================================================

void NoiseFloor::update(uint8_t bin, float magnitude) {
    // Bounds check
    if (bin >= m_numBins || m_floor == nullptr) {
        return;
    }

    // Clamp magnitude to non-negative
    if (magnitude < 0.0f) {
        magnitude = 0.0f;
    }

    // Exponential moving average update
    // floor = (1 - alpha) * floor + alpha * magnitude
    m_floor[bin] = (1.0f - m_alpha) * m_floor[bin] + m_alpha * magnitude;

    // Enforce minimum floor to prevent divide-by-zero
    if (m_floor[bin] < MIN_FLOOR) {
        m_floor[bin] = MIN_FLOOR;
    }
}

float NoiseFloor::getFloor(uint8_t bin) const {
    // Bounds check
    if (bin >= m_numBins || m_floor == nullptr) {
        return MIN_FLOOR;
    }

    return m_floor[bin];
}

float NoiseFloor::getThreshold(uint8_t bin, float multiplier) const {
    // Bounds check
    if (bin >= m_numBins || m_floor == nullptr) {
        return MIN_FLOOR * multiplier;
    }

    return m_floor[bin] * multiplier;
}

bool NoiseFloor::isAboveFloor(uint8_t bin, float magnitude, float threshold) const {
    // Bounds check
    if (bin >= m_numBins || m_floor == nullptr) {
        return false;
    }

    // Check if magnitude exceeds floor * threshold
    // Use small epsilon to prevent false positives when floor is near-zero
    const float eps = 1e-9f;
    return magnitude > (m_floor[bin] * threshold + eps);
}

void NoiseFloor::reset() {
    if (m_floor == nullptr) {
        return;
    }

    // Reset all bins to minimum floor value
    for (uint8_t i = 0; i < m_numBins; i++) {
        m_floor[i] = MIN_FLOOR;
    }
}

} // namespace audio
} // namespace lightwaveos
