/**
 * @file StyleDetector.h
 * @brief Music style classification for adaptive audio-visual response
 *
 * Part of the Musical Intelligence System (MIS) - Phase 2
 * Detects music style to select appropriate visual response strategies.
 */

#pragma once

#include <cstdint>
#include "contracts/StyleDetector.h"  // MusicStyle enum definition

namespace lightwaveos {
namespace audio {

/**
 * @brief Style classification result with confidence
 */
struct StyleClassification {
    MusicStyle detected = MusicStyle::UNKNOWN;
    float confidence = 0.0f;           ///< 0-1 how confident we are
    float styleWeights[5] = {0};       ///< Probability for each style (excluding UNKNOWN)
    uint32_t framesAnalyzed = 0;       ///< How long we've been tracking

    /// Get weight for a specific style
    float getWeight(MusicStyle style) const {
        if (style == MusicStyle::UNKNOWN) return 0.0f;
        return styleWeights[static_cast<uint8_t>(style) - 1];
    }
};

/**
 * @brief Tuning parameters for style detection
 */
struct StyleDetectorTuning {
    // Analysis window
    uint32_t analysisWindowHops = 250;  ///< ~4 seconds at 62.5 Hz hop rate
    uint32_t minHopsForClassification = 62;  ///< ~1 second minimum

    // Feature thresholds
    float beatConfidenceThreshold = 0.4f;   ///< Above this = rhythmic music
    float bassRatioThreshold = 0.3f;        ///< Bass/total energy ratio for RHYTHMIC
    float trebleRatioThreshold = 0.25f;     ///< Treble/total energy ratio for MELODIC
    float dynamicRangeThreshold = 0.4f;     ///< For DYNAMIC_DRIVEN
    float fluxVarianceThreshold = 0.1f;     ///< High variance = TEXTURE_DRIVEN
    float chordChangeRateThreshold = 0.5f;  ///< Chord changes per second for HARMONIC

    // Smoothing
    float styleAlpha = 0.05f;  ///< How fast to adapt style weights (slower = more stable)
    float hysteresisThreshold = 0.15f;  ///< Minimum weight difference to switch style
};

/**
 * @brief Rolling statistics for style detection
 */
struct StyleFeatures {
    // Beat/rhythm features
    float beatConfidenceAvg = 0.0f;
    float beatConfidenceVar = 0.0f;

    // Spectral balance features
    float bassRatio = 0.0f;       ///< bands[0-1] / total
    float midRatio = 0.0f;        ///< bands[2-5] / total
    float trebleRatio = 0.0f;     ///< bands[6-7] / total

    // Dynamic features
    float rmsMin = 1.0f;
    float rmsMax = 0.0f;
    float dynamicRange = 0.0f;    ///< rmsMax - rmsMin

    // Harmonic features
    float chordChangeRate = 0.0f; ///< Chord changes per second
    uint32_t chordChanges = 0;

    // Timbral features
    float fluxMean = 0.0f;
    float fluxVariance = 0.0f;

    void reset() {
        beatConfidenceAvg = 0.0f;
        beatConfidenceVar = 0.0f;
        bassRatio = 0.0f;
        midRatio = 0.0f;
        trebleRatio = 0.0f;
        rmsMin = 1.0f;
        rmsMax = 0.0f;
        dynamicRange = 0.0f;
        chordChangeRate = 0.0f;
        chordChanges = 0;
        fluxMean = 0.0f;
        fluxVariance = 0.0f;
    }
};

/**
 * @brief Music style detector using rolling feature analysis
 *
 * Analyzes audio features over a sliding window to classify music style.
 * Used by effects to select appropriate visual response strategies.
 */
class StyleDetector {
public:
    StyleDetector();

    /**
     * @brief Update with new audio hop data
     * @param rms Current RMS energy
     * @param flux Current spectral flux
     * @param bands 8-band spectrum (0-1 normalized)
     * @param beatConfidence Beat detection confidence (0-1)
     * @param chordChanged True if chord root changed this hop
     */
    void update(float rms, float flux, const float* bands,
                float beatConfidence, bool chordChanged);

    /**
     * @brief Get current style classification
     */
    const StyleClassification& getClassification() const { return m_classification; }

    /**
     * @brief Get current detected style
     */
    MusicStyle getStyle() const { return m_classification.detected; }

    /**
     * @brief Get classification confidence
     */
    float getConfidence() const { return m_classification.confidence; }

    /**
     * @brief Get current feature state (for debugging)
     */
    const StyleFeatures& getFeatures() const { return m_features; }

    /**
     * @brief Reset detector state
     */
    void reset();

    /**
     * @brief Set tuning parameters
     */
    void setTuning(const StyleDetectorTuning& tuning) { m_tuning = tuning; }

private:
    StyleClassification m_classification;
    StyleFeatures m_features;
    StyleDetectorTuning m_tuning;

    // Rolling window state
    uint32_t m_hopCount = 0;
    float m_windowStartTimeS = 0.0f;

    // Previous state for derivative calculation
    uint8_t m_prevChordRoot = 0;
    float m_prevFlux = 0.0f;

    // Running sums for incremental statistics
    float m_rmsSum = 0.0f;
    float m_fluxSum = 0.0f;
    float m_fluxSqSum = 0.0f;
    float m_beatConfSum = 0.0f;
    float m_beatConfSqSum = 0.0f;
    float m_bandSums[8] = {0};

    /**
     * @brief Compute style weights from current features
     */
    void computeStyleWeights();

    /**
     * @brief Select dominant style with hysteresis
     */
    void selectDominantStyle();
};

} // namespace audio
} // namespace lightwaveos
