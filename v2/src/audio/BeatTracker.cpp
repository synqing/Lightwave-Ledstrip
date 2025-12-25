/**
 * @file BeatTracker.cpp
 * @brief Beat/tempo tracker implementation
 */

#include "BeatTracker.h"

#if FEATURE_AUDIO_SYNC

#include <cmath>
#include <algorithm>

namespace lightwaveos {
namespace audio {

// Band weights for spectral flux (bass-heavy for kick detection)
static constexpr float BAND_WEIGHTS[CONTROLBUS_NUM_BANDS] = {
    BASS_WEIGHT,   // Band 0: 60 Hz (sub-bass)
    BASS_WEIGHT,   // Band 1: 120 Hz (bass)
    MID_WEIGHT,    // Band 2: 250 Hz (low-mid)
    MID_WEIGHT,    // Band 3: 500 Hz (mid)
    MID_WEIGHT,    // Band 4: 1000 Hz (upper-mid)
    HIGH_WEIGHT,   // Band 5: 2000 Hz (presence)
    HIGH_WEIGHT,   // Band 6: 4000 Hz (brilliance)
    HIGH_WEIGHT    // Band 7: 7800 Hz (air)
};

// Precompute weight sum for normalization
static float computeWeightSum() {
    float sum = 0.0f;
    for (int i = 0; i < CONTROLBUS_NUM_BANDS; ++i) {
        sum += BAND_WEIGHTS[i];
    }
    return sum;
}

static const float WEIGHT_SUM = computeWeightSum();

BeatTracker::BeatTracker() {
    reset();
}

void BeatTracker::reset() {
    for (int i = 0; i < CONTROLBUS_NUM_BANDS; ++i) {
        m_prevBands[i] = 0.0f;
    }

    m_emaMean = 0.0f;
    m_emaVar = 0.0f;
    m_threshold = 0.1f;

    m_beatDetected = false;
    m_beatStrength = 0.0f;
    m_lastWeightedFlux = 0.0f;

    m_lastBeatTime = AudioTime{};
    m_hasLastBeat = false;

    for (int i = 0; i < IBI_HISTORY_SIZE; ++i) {
        m_ibiHistory[i] = 0;
    }
    m_ibiIndex = 0;
    m_ibiCount = 0;

    m_currentBPM = 120.0f;
    m_ibiVariance = -1.0f;
}

void BeatTracker::process(const AudioTime& now, const float bands[CONTROLBUS_NUM_BANDS], float rms) {
    // Reset beat flag each hop
    m_beatDetected = false;
    m_beatStrength = 0.0f;

    // 1. Compute band-weighted spectral flux
    float flux = computeWeightedFlux(bands);
    m_lastWeightedFlux = flux;

    // 2. Update adaptive threshold
    updateThreshold(flux);

    // 3. Check RMS floor gate (silence suppression)
    if (rms < RMS_FLOOR) {
        // Too quiet - don't trigger beats, but still update prev bands
        for (int i = 0; i < CONTROLBUS_NUM_BANDS; ++i) {
            m_prevBands[i] = bands[i];
        }
        return;
    }

    // 4. Check debounce (minimum interval since last beat)
    bool debounce_ok = true;
    if (m_hasLastBeat) {
        int64_t samples_since_beat = AudioTime_SamplesBetween(m_lastBeatTime, now);
        if (samples_since_beat < static_cast<int64_t>(MIN_BEAT_INTERVAL_SAMPLES)) {
            debounce_ok = false;
        }
    }

    // 5. Beat detection: flux > threshold AND debounce OK
    if (flux > m_threshold && debounce_ok) {
        m_beatDetected = true;

        // Beat strength = how far above threshold (normalized)
        float excess = flux - m_threshold;
        float max_excess = 1.0f - m_threshold;  // Max possible excess
        m_beatStrength = std::min(1.0f, excess / std::max(0.01f, max_excess));

        // Record inter-beat interval
        if (m_hasLastBeat) {
            int64_t interval = AudioTime_SamplesBetween(m_lastBeatTime, now);

            // Validate interval is within BPM range
            // 60 BPM = 16000 samples/beat, 180 BPM = 5333 samples/beat
            float samples_per_beat_min = SAMPLE_RATE * 60.0f / MAX_BPM;  // 5333
            float samples_per_beat_max = SAMPLE_RATE * 60.0f / MIN_BPM;  // 16000

            if (interval >= static_cast<int64_t>(samples_per_beat_min) &&
                interval <= static_cast<int64_t>(samples_per_beat_max)) {
                addIBI(interval);
                analyzeTempo();
            }
        }

        m_lastBeatTime = now;
        m_hasLastBeat = true;
    }

    // Update previous bands for next hop
    for (int i = 0; i < CONTROLBUS_NUM_BANDS; ++i) {
        m_prevBands[i] = bands[i];
    }
}

float BeatTracker::computeWeightedFlux(const float bands[CONTROLBUS_NUM_BANDS]) {
    float weighted_sum = 0.0f;

    for (int i = 0; i < CONTROLBUS_NUM_BANDS; ++i) {
        // Positive-only delta (onset detection)
        float delta = bands[i] - m_prevBands[i];
        if (delta > 0.0f) {
            weighted_sum += BAND_WEIGHTS[i] * delta;
        }
    }

    // Normalize by weight sum so output is roughly in [0, 1] range
    return weighted_sum / WEIGHT_SUM;
}

void BeatTracker::updateThreshold(float flux) {
    // Update EMA of mean
    m_emaMean = m_emaMean + ONSET_EMA_ALPHA * (flux - m_emaMean);

    // Update EMA of variance (for std computation)
    float deviation = flux - m_emaMean;
    m_emaVar = m_emaVar + ONSET_EMA_ALPHA * (deviation * deviation - m_emaVar);

    // Compute adaptive threshold = mean + k * std
    float std = std::sqrt(std::max(0.0f, m_emaVar));
    m_threshold = m_emaMean + ONSET_THRESHOLD_K * std;

    // Clamp threshold to reasonable range
    m_threshold = std::max(0.02f, std::min(0.8f, m_threshold));
}

void BeatTracker::addIBI(int64_t interval_samples) {
    m_ibiHistory[m_ibiIndex] = interval_samples;
    m_ibiIndex = (m_ibiIndex + 1) % IBI_HISTORY_SIZE;

    if (m_ibiCount < IBI_HISTORY_SIZE) {
        m_ibiCount++;
    }
}

void BeatTracker::analyzeTempo() {
    if (m_ibiCount < MIN_IBI_FOR_TEMPO) {
        m_ibiVariance = -1.0f;
        return;
    }

    // Calculate mean of stored intervals
    double mean = 0.0;
    for (int i = 0; i < m_ibiCount; ++i) {
        mean += static_cast<double>(m_ibiHistory[i]);
    }
    mean /= m_ibiCount;

    // Calculate variance
    double variance = 0.0;
    for (int i = 0; i < m_ibiCount; ++i) {
        double diff = static_cast<double>(m_ibiHistory[i]) - mean;
        variance += diff * diff;
    }
    variance /= m_ibiCount;
    m_ibiVariance = static_cast<float>(variance);

    // Convert mean interval (samples) to BPM
    if (mean > 0.0) {
        // BPM = 60 * sample_rate / samples_per_beat
        float bpm = 60.0f * SAMPLE_RATE / static_cast<float>(mean);

        // Clamp to valid range
        m_currentBPM = std::max(MIN_BPM, std::min(MAX_BPM, bpm));
    }
}

float BeatTracker::getConfidence() const {
    if (m_ibiVariance < 0.0f || m_ibiCount < MIN_IBI_FOR_TEMPO) {
        return 0.0f;  // Not enough data
    }

    // Lower variance = higher confidence
    // Max variance for MIN_BPM to MAX_BPM range
    float samples_per_beat_min = SAMPLE_RATE * 60.0f / MAX_BPM;
    float samples_per_beat_max = SAMPLE_RATE * 60.0f / MIN_BPM;
    float max_variance = std::pow((samples_per_beat_max - samples_per_beat_min) / 2.0f, 2);

    if (max_variance <= 0.0f) {
        return 0.5f;  // Avoid division by zero
    }

    float confidence = 1.0f - (m_ibiVariance / max_variance);
    return std::max(0.0f, std::min(1.0f, confidence));
}

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
