/**
 * @file BeatTracker.h
 * @brief Beat/tempo tracker using band-weighted spectral flux
 *
 * DECISION RECORD: Use band-weighted spectral-flux transient detection with
 * adaptive threshold + RMS floor gating.
 *
 * Algorithm:
 * 1. Compute positive deltas: d[i] = max(0, bands[i] - prev_bands[i])
 * 2. Compute weighted flux: F = Σ w[i] * d[i] with bass bands weighted heavier
 * 3. Gate with RMS floor (prevents silence triggering)
 * 4. Adaptive threshold: ema_mean + k * ema_std (tracks music dynamics)
 * 5. When flux > threshold AND RMS > floor → beat detected
 * 6. Track inter-beat intervals (IBI) to estimate BPM
 *
 * Timing:
 * - Operates at BEAT LANE cadence (62.5 Hz / 16ms)
 * - Uses AudioTime for timestamping (sample_index is king)
 * - BPM range: 60-180 (prevents octave errors)
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "../config/features.h"

#if FEATURE_AUDIO_SYNC

#include "contracts/AudioTime.h"
#include "contracts/ControlBus.h"
#include "../config/audio_config.h"

namespace lightwaveos {
namespace audio {

/**
 * @brief Beat tracker using band-weighted spectral flux with adaptive threshold
 *
 * Called every beat-lane hop (256 samples / 16ms) with current band energies.
 * Detects beats and estimates tempo from inter-beat intervals.
 */
class BeatTracker {
public:
    BeatTracker();

    /**
     * @brief Reset tracker state
     */
    void reset();

    /**
     * @brief Process one beat-lane hop
     *
     * @param now AudioTime at end of this hop
     * @param bands Current 8-band energies [0,1] from Goertzel
     * @param rms Current RMS energy [0,1] for silence gating
     */
    void process(const AudioTime& now, const float bands[CONTROLBUS_NUM_BANDS], float rms);

    // ========================================================================
    // Results (valid after process())
    // ========================================================================

    /// True for one hop when a beat was detected
    bool isBeat() const { return m_beatDetected; }

    /// Current BPM estimate (60-180 range)
    float getBPM() const { return m_currentBPM; }

    /// Tempo tracking confidence [0,1]
    float getConfidence() const;

    /// Beat strength/confidence [0,1] - how strong this beat was
    float getBeatStrength() const { return m_beatStrength; }

    /// Last weighted spectral flux value (for debugging)
    float getWeightedFlux() const { return m_lastWeightedFlux; }

    /// Current adaptive threshold (for debugging)
    float getThreshold() const { return m_threshold; }

    /// AudioTime of last detected beat
    const AudioTime& getLastBeatTime() const { return m_lastBeatTime; }

    /// True if we have enough IBI history for reliable tempo
    bool hasValidTempo() const { return m_ibiCount >= MIN_IBI_FOR_TEMPO; }

private:
    // ========================================================================
    // Configuration (from audio_config.h)
    // ========================================================================

    static constexpr int IBI_HISTORY_SIZE = 16;
    static constexpr int MIN_IBI_FOR_TEMPO = 4;  // Need 4 beats for tempo estimate

    // Debounce: minimum samples between beats (prevents double-triggers)
    // At 120 BPM, beat interval = 8000 samples. Allow 60% = 4800 samples min.
    static constexpr uint64_t MIN_BEAT_INTERVAL_SAMPLES = 4000;  // ~250ms @ 16kHz

    // ========================================================================
    // State
    // ========================================================================

    // Previous band values for delta computation
    float m_prevBands[CONTROLBUS_NUM_BANDS] = {0};

    // Adaptive threshold state (EMA of mean and variance)
    float m_emaMean = 0.0f;
    float m_emaVar = 0.0f;
    float m_threshold = 0.1f;  // Initial threshold

    // Beat detection output
    bool m_beatDetected = false;
    float m_beatStrength = 0.0f;
    float m_lastWeightedFlux = 0.0f;

    // Timing
    AudioTime m_lastBeatTime;
    bool m_hasLastBeat = false;

    // Inter-beat interval history (for tempo estimation)
    int64_t m_ibiHistory[IBI_HISTORY_SIZE] = {0};  // Intervals in samples
    int m_ibiIndex = 0;
    int m_ibiCount = 0;

    // Tempo estimation
    float m_currentBPM = 120.0f;
    float m_ibiVariance = -1.0f;  // Negative = not enough data

    // ========================================================================
    // Internal Methods
    // ========================================================================

    /**
     * @brief Compute band-weighted spectral flux
     * @param bands Current band energies
     * @return Weighted flux value
     */
    float computeWeightedFlux(const float bands[CONTROLBUS_NUM_BANDS]);

    /**
     * @brief Update adaptive threshold from flux value
     * @param flux Current weighted flux
     */
    void updateThreshold(float flux);

    /**
     * @brief Add an inter-beat interval to history
     * @param interval_samples Interval in samples
     */
    void addIBI(int64_t interval_samples);

    /**
     * @brief Analyze IBI history to estimate tempo
     */
    void analyzeTempo();
};

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
