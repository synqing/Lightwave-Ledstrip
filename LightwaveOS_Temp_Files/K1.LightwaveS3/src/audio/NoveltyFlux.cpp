/**
 * @file NoveltyFlux.cpp
 * @brief Spectral flux computation implementation
 */

#include "NoveltyFlux.h"
#include "NoiseFloor.h"
#include <cstring>  // memcpy

NoveltyFlux::NoveltyFlux(uint8_t numBins)
    : m_prevMags(nullptr)
    , m_numBins(numBins)
    , m_initialized(false)
{
    if (numBins > 0) {
        m_prevMags = new float[numBins];
        memset(m_prevMags, 0, numBins * sizeof(float));
    }
}

NoveltyFlux::~NoveltyFlux() {
    delete[] m_prevMags;
}

float NoveltyFlux::compute(const float* currentMags, const NoiseFloor& noiseFloor) {
    if (!currentMags || !m_prevMags || m_numBins == 0) {
        return 0.0f;
    }

    // First frame: store magnitudes and return 0 (no delta yet)
    if (!m_initialized) {
        memcpy(m_prevMags, currentMags, m_numBins * sizeof(float));
        m_initialized = true;
        return 0.0f;
    }

    // Compute half-wave rectified spectral flux
    float flux = 0.0f;
    uint16_t activeCount = 0;  // Count bins above noise floor

    for (uint8_t i = 0; i < m_numBins; i++) {
        float delta = currentMags[i] - m_prevMags[i];

        // Half-wave rectification: only positive changes (energy increases)
        if (delta > 0.0f) {
            // Noise floor thresholding
            float threshold = noiseFloor.getThreshold(i);
            if (currentMags[i] > threshold) {
                flux += delta;
                activeCount++;
            }
        }
    }

    // Update magnitude history for next frame
    memcpy(m_prevMags, currentMags, m_numBins * sizeof(float));

    // Normalize by number of active bins (avoid division by zero)
    if (activeCount > 0) {
        flux /= static_cast<float>(activeCount);
    }

    return flux;
}

void NoveltyFlux::reset() {
    if (m_prevMags) {
        memset(m_prevMags, 0, m_numBins * sizeof(float));
    }
    m_initialized = false;
}
