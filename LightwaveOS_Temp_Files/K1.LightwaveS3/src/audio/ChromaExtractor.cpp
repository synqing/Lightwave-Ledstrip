/**
 * @file ChromaExtractor.cpp
 * @brief Chroma extraction implementation
 */

#include "ChromaExtractor.h"
#include <cmath>
#include <cstring>  // memset

void ChromaExtractor::extract(const float* harmonicMags, uint8_t numBins, float* chroma12) {
    if (!harmonicMags || !chroma12 || numBins == 0) {
        return;
    }

    // Initialize chroma bins to zero
    memset(chroma12, 0, 12 * sizeof(float));

    // Frequency step between bins
    float freqStep = (m_binFreqEnd - m_binFreqStart) / static_cast<float>(numBins - 1);

    // Accumulate magnitudes into chroma bins
    for (uint8_t i = 0; i < numBins; i++) {
        float freq = m_binFreqStart + i * freqStep;
        uint8_t chromaBin = binToChroma(freq);
        chroma12[chromaBin] += harmonicMags[i];
    }

    // Normalize to [0, 1] range
    float maxChroma = 0.0f;
    for (uint8_t i = 0; i < 12; i++) {
        if (chroma12[i] > maxChroma) {
            maxChroma = chroma12[i];
        }
    }

    if (maxChroma > 0.0f) {
        for (uint8_t i = 0; i < 12; i++) {
            chroma12[i] /= maxChroma;
        }
    }
}

void ChromaExtractor::setFrequencyRange(float startHz, float endHz) {
    m_binFreqStart = startHz;
    m_binFreqEnd = endHz;
}

uint8_t ChromaExtractor::binToChroma(float freqHz) const {
    // Equal temperament tuning with A4 = 440 Hz as reference
    // A4 = 440 Hz corresponds to MIDI note 69
    // Semitones from A4 = 12 * log2(freq / 440)

    if (freqHz <= 0.0f) {
        return 0;
    }

    // Compute semitones from A4
    float semitonesFromA4 = 12.0f * log2f(freqHz / 440.0f);

    // Convert to positive semitone index (octave-folded)
    // A4 (440 Hz) → chroma bin 9 (A)
    // C4 (261.63 Hz) → chroma bin 0 (C)
    int semitoneIndex = static_cast<int>(roundf(semitonesFromA4));

    // Fold into [0, 11] range (octave folding)
    // Note: % operator behavior with negative numbers is implementation-defined
    // Use modulo with adjustment for negative values
    int chromaBin = semitoneIndex % 12;
    if (chromaBin < 0) {
        chromaBin += 12;
    }

    // A4 is chroma bin 9 (A), need to shift so C is bin 0
    // A4 semitone offset from C4 = 9 semitones
    chromaBin = (chromaBin + 9) % 12;

    return static_cast<uint8_t>(chromaBin);
}
