#include "../include/audio/legacy_beat_detector.h"

LegacyBeatDetector::LegacyBeatDetector(float min_bpm, float max_bpm)
    : min_interval_ms(60000.0f / max_bpm),
      max_interval_ms(60000.0f / min_bpm)
{
    for (int i = 0; i < IBI_HISTORY_SIZE; ++i) {
        ibi_history[i] = 0;
    }
}

void LegacyBeatDetector::process(float energy, bool transient_detected) {
    unsigned long now = millis();
    beat_detected = false; // Reset beat flag each frame

    // Check for timeout - reset BPM if no beats for too long
    if (last_beat_time > 0 && (now - last_beat_time) > BEAT_TIMEOUT_MS) {
        // No beats detected for 3+ seconds, reset
        current_bpm = 120.0f;  // Default to 120 BPM
        ibi_count = 0;         // Clear history
        ibi_variance = -1.0f;  // Reset variance
        is_confident = false;
        last_beat_time = 0;    // Reset timer
        
        static bool timeout_logged = false;
        if (!timeout_logged) {
            Serial.println("BEAT TIMEOUT: Resetting BPM to 120");
            timeout_logged = true;
        }
    } else {
        static bool timeout_logged = true;
        timeout_logged = false;  // Reset flag when beats are active
    }

    // Debug output
    static int debug_count = 0;
    if (++debug_count % 200 == 0) {
        Serial.printf("LEGACY BEAT: energy=%.1f (thresh=%.1f), trans=%d, time_since=%lu\n", 
                     energy, ENERGY_THRESHOLD_MIN, transient_detected, now - last_beat_time);
    }

    // A beat is a transient with sufficient energy that is not too close to the last one
    if (transient_detected && energy > ENERGY_THRESHOLD_MIN && (now - last_beat_time) > DEBOUNCE_MS) {
        beat_detected = true;
        Serial.printf("BEAT DETECTED! energy=%.1f, BPM will update\n", energy);
        if (last_beat_time > 0) {
            unsigned long interval = now - last_beat_time;
            // Check if the interval is within a reasonable BPM range
            if (interval >= min_interval_ms && interval <= max_interval_ms) {
                addIBI(interval);
                analyzeIBIs();
            }
        }
        last_beat_time = now;
    }
}

float LegacyBeatDetector::getBPM() const {
    return current_bpm;
}

bool LegacyBeatDetector::isBeat() const {
    return beat_detected;
}

float LegacyBeatDetector::getConfidence() const {
    if (ibi_variance < 0 || ibi_count < 4) {
        return 0.0f; // Not enough data
    }
    // Lower variance means higher confidence. Normalize against max possible variance.
    float max_variance = pow((max_interval_ms - min_interval_ms) / 2.0f, 2);
    if (max_variance <= 0) return 0.5; // Avoid division by zero
    float confidence = 1.0f - (ibi_variance / max_variance);
    return constrain(confidence, 0.0f, 1.0f);
}

void LegacyBeatDetector::addIBI(unsigned long interval) {
    ibi_history[ibi_history_index] = interval;
    ibi_history_index = (ibi_history_index + 1) % IBI_HISTORY_SIZE;
    if (ibi_count < IBI_HISTORY_SIZE) {
        ibi_count++;
    }
}

void LegacyBeatDetector::analyzeIBIs() {
    if (ibi_count < IBI_HISTORY_SIZE / 2) {
        is_confident = false;
        ibi_variance = -1.0f;
        return;
    }

    // Calculate mean of the stored intervals
    float mean = 0;
    for (int i = 0; i < ibi_count; i++) {
        mean += ibi_history[i];
    }
    mean /= ibi_count;

    // Calculate variance
    float variance = 0;
    for (int i = 0; i < ibi_count; i++) {
        variance += pow(ibi_history[i] - mean, 2);
    }
    variance /= ibi_count;
    ibi_variance = variance;

    // Update BPM and confidence state
    if (mean > 0) {
        current_bpm = 60000.0f / mean;
    }
    
    // Confidence is high if the standard deviation is a small fraction of the mean
    float std_dev = sqrt(ibi_variance);
    if (std_dev < (mean * 0.15f)) {
        is_confident = true;
    } else {
        is_confident = false;
    }
} 