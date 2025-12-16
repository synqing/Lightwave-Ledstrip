#ifndef LEGACY_BEAT_DETECTOR_H
#define LEGACY_BEAT_DETECTOR_H

#include <Arduino.h>

/*
 * LegacyBeatDetector: A robust, time-tested beat detection algorithm.
 * =================================================================
 *
 * This class is a faithful reimplementation of the beat detection logic 
 * from the original, stable Emotiscope firmware. It is designed to be
 * simple, efficient, and compatible with the project's scaled-integer
 * audio pipeline.
 *
 * It operates on two core principles:
 * 1.  Energy Thresholding: A beat must have a minimum energy level.
 * 2.  Transient Detection: A beat is marked by a sharp increase in energy.
 * 3.  Interval Analysis: Tempo is derived from the timing variance of 
 *     recent, valid beat intervals.
 *
 * This approach avoids the pitfalls of spectral flux on integer-based
 * data and provides a reliable foundation for beat-driven effects.
 */

class LegacyBeatDetector {
public:
    LegacyBeatDetector(float min_bpm = 80.0f, float max_bpm = 165.0f);

    void process(float energy, bool transient_detected);

    float getBPM() const;
    bool isBeat() const;
    float getConfidence() const;

private:
    // Constants
    static const int IBI_HISTORY_SIZE = 16;
    static constexpr float ENERGY_THRESHOLD_MIN = 8000.0f; // Calibrated for actual energy values (3k-35k range)
    static constexpr unsigned long DEBOUNCE_MS = 100;
    static constexpr unsigned long BEAT_TIMEOUT_MS = 3000; // Reset after 3 seconds of no beats

    // Inter-Beat Interval (IBI) history
    unsigned long ibi_history[IBI_HISTORY_SIZE];
    int ibi_history_index = 0;
    int ibi_count = 0;
    float ibi_variance = -1.0f;

    // Timing and state
    unsigned long last_beat_time = 0;
    bool beat_detected = false;
    
    // Configuration
    const unsigned long min_interval_ms;
    const unsigned long max_interval_ms;
    
    // Output
    float current_bpm = 120.0f;
    bool is_confident = false;

    void addIBI(unsigned long interval);
    void analyzeIBIs();
};

#endif // LEGACY_BEAT_DETECTOR_H 