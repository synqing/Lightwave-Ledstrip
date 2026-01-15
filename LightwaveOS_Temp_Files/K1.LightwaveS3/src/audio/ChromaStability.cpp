/**
 * @file ChromaStability.cpp
 * @brief Chroma stability tracking implementation
 */

#include "ChromaStability.h"
#include <cstring>  // memset, memcpy
#include <cmath>    // sqrtf

ChromaStability::ChromaStability(uint8_t windowSize)
    : m_chromaHistory(nullptr)
    , m_stability(0.0f)
    , m_windowSize(windowSize)
    , m_writeIndex(0)
    , m_framesRecorded(0)
{
    if (windowSize > 0) {
        // Allocate ring buffer: 12 chroma bins * windowSize frames
        size_t bufferSize = 12 * windowSize;
        m_chromaHistory = new float[bufferSize];
        memset(m_chromaHistory, 0, bufferSize * sizeof(float));
    }
}

ChromaStability::~ChromaStability() {
    delete[] m_chromaHistory;
}

void ChromaStability::update(const float* chroma12) {
    if (!chroma12 || !m_chromaHistory || m_windowSize == 0) {
        return;
    }

    // Write current chroma to ring buffer
    float* writePos = m_chromaHistory + (m_writeIndex * 12);
    memcpy(writePos, chroma12, 12 * sizeof(float));

    // Advance ring buffer index
    m_writeIndex = (m_writeIndex + 1) % m_windowSize;

    // Track number of frames recorded (up to windowSize)
    if (m_framesRecorded < m_windowSize) {
        m_framesRecorded++;
    }

    // Compute stability
    computeStability(chroma12);
}

void ChromaStability::computeStability(const float* current) {
    if (m_framesRecorded == 0) {
        m_stability = 0.0f;
        return;
    }

    // Compute average chroma over window
    float avgChroma[12] = {0};
    for (uint8_t frame = 0; frame < m_framesRecorded; frame++) {
        const float* frameChroma = m_chromaHistory + (frame * 12);
        for (uint8_t bin = 0; bin < 12; bin++) {
            avgChroma[bin] += frameChroma[bin];
        }
    }
    for (uint8_t bin = 0; bin < 12; bin++) {
        avgChroma[bin] /= static_cast<float>(m_framesRecorded);
    }

    // Compute cosine similarity: dot(current, avg) / (||current|| * ||avg||)
    float dotProduct = 0.0f;
    float normCurrent = 0.0f;
    float normAvg = 0.0f;

    for (uint8_t bin = 0; bin < 12; bin++) {
        dotProduct += current[bin] * avgChroma[bin];
        normCurrent += current[bin] * current[bin];
        normAvg += avgChroma[bin] * avgChroma[bin];
    }

    normCurrent = sqrtf(normCurrent);
    normAvg = sqrtf(normAvg);

    // Avoid division by zero
    if (normCurrent > 0.0f && normAvg > 0.0f) {
        m_stability = dotProduct / (normCurrent * normAvg);

        // Clamp to [0, 1] (numerical precision may cause slight overflow)
        if (m_stability < 0.0f) m_stability = 0.0f;
        if (m_stability > 1.0f) m_stability = 1.0f;
    } else {
        m_stability = 0.0f;
    }
}

void ChromaStability::reset() {
    if (m_chromaHistory) {
        memset(m_chromaHistory, 0, 12 * m_windowSize * sizeof(float));
    }
    m_stability = 0.0f;
    m_writeIndex = 0;
    m_framesRecorded = 0;
}
