/**
 * @file ChromaExtractor.h
 * @brief 12-bin chroma vector extraction from harmonic spectrum
 *
 * Extracts pitch class distribution by folding frequency bins into semitone classes.
 * Used for harmonic stability detection in tempo tracking.
 *
 * Part of Phase 2C: Feature Extraction Pipeline
 */

#pragma once

#include <cstdint>

/**
 * @class ChromaExtractor
 * @brief Extracts 12-bin chroma from frequency spectrum
 *
 * Chroma represents the pitch class distribution (C, C#, D, ..., B) independent of octave.
 * Used to detect harmonic stability and filter out non-tonal onsets.
 *
 * Algorithm:
 * 1. Map each frequency bin to a semitone class (0-11)
 * 2. Accumulate magnitude into corresponding chroma bin
 * 3. Normalize to [0, 1] range
 *
 * Frequency-to-chroma mapping:
 * - A4 = 440 Hz → chroma bin 9 (A)
 * - Semitone ratio = 2^(1/12) ≈ 1.0595
 * - Bins are octave-folded
 */
class ChromaExtractor {
public:
    /**
     * @brief Construct chroma extractor
     */
    ChromaExtractor() = default;

    /**
     * @brief Extract 12-bin chroma vector from magnitude spectrum
     * @param harmonicMags Magnitude spectrum (typically 200-2000 Hz bins)
     * @param numBins Number of frequency bins in harmonicMags
     * @param chroma12 Output 12-bin chroma vector (must be pre-allocated)
     *
     * Assumptions:
     * - harmonicMags spans 200-2000 Hz (configurable via binFreqStart/binFreqEnd)
     * - Linear frequency spacing between bins
     * - Output chroma12[0] = C, chroma12[1] = C#, ..., chroma12[11] = B
     */
    void extract(const float* harmonicMags, uint8_t numBins, float* chroma12);

    /**
     * @brief Set frequency range for bin mapping
     * @param startHz Frequency of first bin (default: 200 Hz)
     * @param endHz Frequency of last bin (default: 2000 Hz)
     */
    void setFrequencyRange(float startHz, float endHz);

private:
    /**
     * @brief Map frequency to chroma bin index [0-11]
     * @param freqHz Frequency in Hz
     * @return Chroma bin index (0 = C, 11 = B)
     *
     * Uses equal temperament tuning with A4 = 440 Hz as reference.
     */
    uint8_t binToChroma(float freqHz) const;

    float m_binFreqStart = 200.0f;   ///< Start frequency for bin mapping
    float m_binFreqEnd = 2000.0f;    ///< End frequency for bin mapping
};
