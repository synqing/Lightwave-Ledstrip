/**
 * @file StyleDetector.cpp
 * @brief Music style classification implementation
 *
 * Part of the Musical Intelligence System (MIS) - Phase 2
 *
 * Analyzes accumulated audio features over a sliding window to classify
 * the dominant musical style. Uses rolling statistics to avoid expensive
 * per-frame computations while maintaining responsiveness.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "StyleDetector.h"
#include <cmath>
#include <algorithm>

namespace lightwaveos {
namespace audio {

// Helper: clamp value to 0-1 range
static inline float clamp01(float x) {
    return (x < 0.0f) ? 0.0f : ((x > 1.0f) ? 1.0f : x);
}

StyleDetector::StyleDetector() {
    reset();
}

void StyleDetector::reset() {
    m_classification = StyleClassification{};
    m_features.reset();
    m_hopCount = 0;
    m_windowStartTimeS = 0.0f;
    m_prevChordRoot = 0;
    m_prevFlux = 0.0f;
    m_rmsSum = 0.0f;
    m_fluxSum = 0.0f;
    m_fluxSqSum = 0.0f;
    m_beatConfSum = 0.0f;
    m_beatConfSqSum = 0.0f;
    for (int i = 0; i < 8; ++i) {
        m_bandSums[i] = 0.0f;
    }
}

void StyleDetector::update(float rms, float flux, const float* bands,
                           float beatConfidence, bool chordChanged) {
    m_hopCount++;
    m_classification.framesAnalyzed = m_hopCount;

    // Accumulate running statistics
    m_rmsSum += rms;
    m_fluxSum += flux;
    m_fluxSqSum += flux * flux;
    m_beatConfSum += beatConfidence;
    m_beatConfSqSum += beatConfidence * beatConfidence;

    for (int i = 0; i < 8; ++i) {
        m_bandSums[i] += bands[i];
    }

    // Track RMS range for dynamic range calculation
    if (rms < m_features.rmsMin) {
        m_features.rmsMin = rms;
    }
    if (rms > m_features.rmsMax) {
        m_features.rmsMax = rms;
    }

    // Count chord changes
    if (chordChanged) {
        m_features.chordChanges++;
    }

    // Need minimum hops before classification is meaningful
    if (m_hopCount < m_tuning.minHopsForClassification) {
        return;
    }

    // Compute features from accumulated statistics
    float invCount = 1.0f / static_cast<float>(m_hopCount);

    // Beat confidence statistics
    m_features.beatConfidenceAvg = m_beatConfSum * invCount;
    float beatConfMeanSq = m_features.beatConfidenceAvg * m_features.beatConfidenceAvg;
    m_features.beatConfidenceVar = (m_beatConfSqSum * invCount) - beatConfMeanSq;
    if (m_features.beatConfidenceVar < 0.0f) {
        m_features.beatConfidenceVar = 0.0f;
    }

    // Spectral balance (bass/mid/treble ratios)
    float totalBands = 0.0f;
    for (int i = 0; i < 8; ++i) {
        totalBands += m_bandSums[i];
    }

    if (totalBands > 0.001f) {
        float invTotal = 1.0f / totalBands;
        m_features.bassRatio = (m_bandSums[0] + m_bandSums[1]) * invTotal;
        m_features.midRatio = (m_bandSums[2] + m_bandSums[3] + m_bandSums[4] + m_bandSums[5]) * invTotal;
        m_features.trebleRatio = (m_bandSums[6] + m_bandSums[7]) * invTotal;
    }

    // Dynamic range
    m_features.dynamicRange = m_features.rmsMax - m_features.rmsMin;

    // Flux statistics
    m_features.fluxMean = m_fluxSum * invCount;
    float fluxMeanSq = m_features.fluxMean * m_features.fluxMean;
    m_features.fluxVariance = (m_fluxSqSum * invCount) - fluxMeanSq;
    if (m_features.fluxVariance < 0.0f) {
        m_features.fluxVariance = 0.0f;
    }

    // Chord change rate (changes per second, assuming 62.5 Hz hop rate)
    float windowDurationS = static_cast<float>(m_hopCount) / 62.5f;
    if (windowDurationS > 0.1f) {
        m_features.chordChangeRate = static_cast<float>(m_features.chordChanges) / windowDurationS;
    }

    // Compute style weights and select dominant
    computeStyleWeights();
    selectDominantStyle();

    // Apply decay to old statistics if window is full (sliding window effect)
    // This provides a gradual "forgetting" of old data without expensive ring buffers
    if (m_hopCount > m_tuning.analysisWindowHops) {
        float decay = 0.99f;  // Gradual decay to forget old data
        m_rmsSum *= decay;
        m_fluxSum *= decay;
        m_fluxSqSum *= decay;
        m_beatConfSum *= decay;
        m_beatConfSqSum *= decay;
        for (int i = 0; i < 8; ++i) {
            m_bandSums[i] *= decay;
        }
        m_features.chordChanges = static_cast<uint32_t>(
            static_cast<float>(m_features.chordChanges) * decay
        );
        // Don't reset hopCount - it's used for framesAnalyzed tracking
    }
}

void StyleDetector::computeStyleWeights() {
    float weights[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    // =========================================================================
    // RHYTHMIC_DRIVEN: Strong beat + bass emphasis
    // Characteristics: High beat confidence, high bass ratio, low beat variance
    // =========================================================================
    float rhythmicScore = 0.0f;
    if (m_features.beatConfidenceAvg > m_tuning.beatConfidenceThreshold) {
        rhythmicScore += 0.5f;
    }
    if (m_features.bassRatio > m_tuning.bassRatioThreshold) {
        rhythmicScore += 0.3f;
    }
    // Low beat variance indicates consistent beat pattern (EDM, hip-hop)
    if (m_features.beatConfidenceVar < 0.1f && m_features.beatConfidenceAvg > 0.3f) {
        rhythmicScore += 0.2f;
    }
    weights[0] = clamp01(rhythmicScore);

    // =========================================================================
    // HARMONIC_DRIVEN: Chord changes dominate, less beat emphasis
    // Characteristics: High chord change rate, lower beat confidence, mid emphasis
    // =========================================================================
    float harmonicScore = 0.0f;
    if (m_features.chordChangeRate > m_tuning.chordChangeRateThreshold) {
        harmonicScore += 0.5f;
    }
    if (m_features.beatConfidenceAvg < m_tuning.beatConfidenceThreshold) {
        harmonicScore += 0.2f;
    }
    // Mid-range emphasis typical of harmonic content (piano, guitar, strings)
    if (m_features.midRatio > 0.4f) {
        harmonicScore += 0.3f;
    }
    weights[1] = clamp01(harmonicScore);

    // =========================================================================
    // MELODIC_DRIVEN: Treble/melody emphasis, moderate beat
    // Characteristics: High treble ratio, moderate beat, low bass
    // =========================================================================
    float melodicScore = 0.0f;
    if (m_features.trebleRatio > m_tuning.trebleRatioThreshold) {
        melodicScore += 0.5f;
    }
    // Moderate beat (not too strong, not absent) - supports but doesn't dominate
    if (m_features.beatConfidenceAvg > 0.2f && m_features.beatConfidenceAvg < 0.6f) {
        melodicScore += 0.3f;
    }
    // Less bass than rhythmic music
    if (m_features.bassRatio < m_tuning.bassRatioThreshold) {
        melodicScore += 0.2f;
    }
    weights[2] = clamp01(melodicScore);

    // =========================================================================
    // TEXTURE_DRIVEN: High flux variance, no clear beat, ambient
    // Characteristics: High spectral flux variance, low beat confidence, flat spectrum
    // =========================================================================
    float textureScore = 0.0f;
    if (m_features.fluxVariance > m_tuning.fluxVarianceThreshold) {
        textureScore += 0.4f;
    }
    // Low beat confidence (no clear rhythm pattern)
    if (m_features.beatConfidenceAvg < 0.2f) {
        textureScore += 0.3f;
    }
    // Relatively flat spectrum (no dominant frequency range)
    float spectrumFlatness = 1.0f - std::abs(m_features.bassRatio - m_features.trebleRatio);
    textureScore += spectrumFlatness * 0.3f;
    weights[3] = clamp01(textureScore);

    // =========================================================================
    // DYNAMIC_DRIVEN: Wide dynamic range, crescendo/decrescendo
    // Characteristics: High dynamic range, variable beat, mid emphasis
    // =========================================================================
    float dynamicScore = 0.0f;
    if (m_features.dynamicRange > m_tuning.dynamicRangeThreshold) {
        dynamicScore += 0.6f;
    }
    // Variable beat (orchestral often has irregular accents)
    if (m_features.beatConfidenceVar > 0.15f) {
        dynamicScore += 0.2f;
    }
    // Mid-range dominant (orchestral instruments)
    if (m_features.midRatio > 0.5f) {
        dynamicScore += 0.2f;
    }
    weights[4] = clamp01(dynamicScore);

    // Smooth weights toward new values using IIR filter
    // This prevents rapid style switching on transient features
    for (int i = 0; i < 5; ++i) {
        m_classification.styleWeights[i] +=
            (weights[i] - m_classification.styleWeights[i]) * m_tuning.styleAlpha;
    }
}

void StyleDetector::selectDominantStyle() {
    // Find highest weight among all style candidates
    float maxWeight = 0.0f;
    int maxIdx = -1;
    for (int i = 0; i < 5; ++i) {
        if (m_classification.styleWeights[i] > maxWeight) {
            maxWeight = m_classification.styleWeights[i];
            maxIdx = i;
        }
    }

    // Map index to MusicStyle enum (index + 1 because UNKNOWN = 0)
    MusicStyle newStyle = (maxIdx >= 0) ?
        static_cast<MusicStyle>(maxIdx + 1) : MusicStyle::UNKNOWN;

    // Apply hysteresis: only switch style if new style beats current by threshold
    // This prevents rapid flip-flopping between similar-weight styles
    if (m_classification.detected != MusicStyle::UNKNOWN) {
        int currentIdx = static_cast<int>(m_classification.detected) - 1;
        if (currentIdx >= 0 && currentIdx < 5) {
            float currentWeight = m_classification.styleWeights[currentIdx];

            // Only switch if new style beats current by hysteresis margin
            if (maxWeight < currentWeight + m_tuning.hysteresisThreshold) {
                newStyle = m_classification.detected;  // Keep current style
                maxWeight = currentWeight;
            }
        }
    }

    m_classification.detected = newStyle;
    m_classification.confidence = clamp01(maxWeight);
}

} // namespace audio
} // namespace lightwaveos
