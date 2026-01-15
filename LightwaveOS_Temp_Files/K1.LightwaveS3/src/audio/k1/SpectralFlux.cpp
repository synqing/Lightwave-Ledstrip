/**
 * @file SpectralFlux.cpp
 * @brief Implementation of Spectral Flux Calculator
 */

#include "SpectralFlux.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace lightwaveos {
namespace audio {
namespace k1 {

SpectralFlux::SpectralFlux()
    : m_currentFlux(0.0f),
      m_previousFlux(0.0f),
      m_historyIndex(0),
      m_historyFull(false) {
    reset();
}

float SpectralFlux::process(const float magnitude[K1FFTConfig::MAGNITUDE_BINS]) {
    // Copy current magnitude
    std::memcpy(m_currentMagnitude, magnitude, sizeof(m_currentMagnitude));

    // Calculate flux: Σ max(0, current[k] - previous[k])
    m_previousFlux = m_currentFlux;
    m_currentFlux = calculateFlux(m_currentMagnitude, m_previousMagnitude);

    // Add to history for adaptive threshold
    addToHistory(m_currentFlux);

    // Swap buffers: previous = current for next frame
    std::memcpy(m_previousMagnitude, m_currentMagnitude, sizeof(m_previousMagnitude));

    return m_currentFlux;
}

float SpectralFlux::calculateFlux(const float current[K1FFTConfig::MAGNITUDE_BINS],
                                  const float previous[K1FFTConfig::MAGNITUDE_BINS]) const {
    float flux = 0.0f;

    for (uint16_t k = 0; k < K1FFTConfig::MAGNITUDE_BINS; k++) {
        // Half-wave rectified: only count positive changes
        float diff = current[k] - previous[k];
        if (diff > 0.0f) {
            flux += diff;
        }
    }

    return flux;
}

void SpectralFlux::addToHistory(float flux) {
    // Add flux to circular buffer
    m_fluxHistory[m_historyIndex] = flux;

    // Advance circular buffer pointer
    m_historyIndex++;
    if (m_historyIndex >= FLUX_HISTORY_SIZE) {
        m_historyIndex = 0;
        m_historyFull = true;  // Completed first full rotation
    }
}

void SpectralFlux::getFluxStatistics(float& median, float& stddev) const {
    // Determine how many values are in history
    uint16_t count = m_historyFull ? FLUX_HISTORY_SIZE : m_historyIndex;

    if (count == 0) {
        median = 0.0f;
        stddev = 0.0f;
        return;
    }

    // Create sorted copy for median calculation
    float sorted[FLUX_HISTORY_SIZE];
    std::memcpy(sorted, m_fluxHistory, count * sizeof(float));
    std::sort(sorted, sorted + count);

    // Calculate median
    if (count % 2 == 1) {
        median = sorted[count / 2];
    } else {
        median = (sorted[count / 2 - 1] + sorted[count / 2]) * 0.5f;
    }

    // Calculate mean for variance
    float mean = std::accumulate(m_fluxHistory, m_fluxHistory + count, 0.0f) / count;

    // Calculate variance
    float variance = 0.0f;
    for (uint16_t i = 0; i < count; i++) {
        float diff = m_fluxHistory[i] - mean;
        variance += diff * diff;
    }
    variance /= count;

    // Standard deviation
    stddev = std::sqrt(variance);
}

void SpectralFlux::reset() {
    std::memset(m_currentMagnitude, 0, sizeof(m_currentMagnitude));
    std::memset(m_previousMagnitude, 0, sizeof(m_previousMagnitude));
    std::memset(m_fluxHistory, 0, sizeof(m_fluxHistory));

    m_currentFlux = 0.0f;
    m_previousFlux = 0.0f;
    m_historyIndex = 0;
    m_historyFull = false;
}

}  // namespace k1
}  // namespace audio
}  // namespace lightwaveos
