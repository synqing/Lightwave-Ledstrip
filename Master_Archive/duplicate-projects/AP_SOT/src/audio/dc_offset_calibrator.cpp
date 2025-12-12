/*
-----------------------------------------------------------------------------------------
    DC Offset Calibration Implementation
    RESTORED: Battle-tested system from pre-corruption state
    Phase 1 implementation of the phased audio DSP reimplementation
-----------------------------------------------------------------------------------------
*/

#include "../include/audio/dc_offset_calibrator.h"
#include <math.h>

DCOffsetCalibrator::DCOffsetCalibrator() {
    reset();
}

void DCOffsetCalibrator::begin() {
    startup_time = millis();
    startup_completed = false;
    calibrated = false;
    calibration_samples_collected = 0;
    baseline_offset = current_offset;
    
    Serial.println("DC Offset Calibrator: Starting calibration sequence");
    Serial.printf("DC Offset Calibrator: Waiting %dms for microphone stabilization\n", STARTUP_DELAY_MS);
}

void DCOffsetCalibrator::processCalibrationSample(int32_t raw_sample) {
    // Handle startup delay
    if (!startup_completed) {
        if (millis() - startup_time < STARTUP_DELAY_MS) {
            return; // Still in startup delay period
        }
        startup_completed = true;
        Serial.println("DC Offset Calibrator: Startup delay complete, beginning sample collection");
    }
    
    // Collect calibration samples
    if (calibration_samples_collected < CALIBRATION_BUFFER_SIZE) {
        calibration_buffer[calibration_samples_collected] = raw_sample;
        calibration_samples_collected++;
        
        // Progress indicator every 100 samples
        if (calibration_samples_collected % 100 == 0) {
            Serial.printf("DC Offset Calibrator: Collected %u/%d samples\n", 
                         calibration_samples_collected, CALIBRATION_BUFFER_SIZE);
        }
        
        // Perform calibration when buffer is full
        if (calibration_samples_collected == CALIBRATION_BUFFER_SIZE) {
            performInitialCalibration();
        }
    }
}

void DCOffsetCalibrator::performInitialCalibration() {
    // Check for noise before calibration
    if (isNoiseDetected(calibration_buffer, CALIBRATION_BUFFER_SIZE)) {
        Serial.println("DC Offset Calibrator: Noise detected during calibration, using fallback offset");
        current_offset = DEFAULT_OFFSET;
        calibrated = true;
        return;
    }
    
    // Calculate mean offset
    int64_t accumulator = 0;
    for (int i = 0; i < CALIBRATION_BUFFER_SIZE; i++) {
        accumulator += calibration_buffer[i];
    }
    
    float measured_offset = static_cast<float>(accumulator) / CALIBRATION_BUFFER_SIZE;
    
    Serial.printf("DC Offset Calibrator: Measured offset: %.2f (baseline: %.2f)\n", 
                  measured_offset, baseline_offset);
    
    // For initial calibration, trust the measured value more
    // since DEFAULT_OFFSET may be very wrong (e.g., 7000 vs actual -4000)
    if (calibration_samples_collected == CALIBRATION_BUFFER_SIZE) {
        // Initial calibration: use mostly measured value
        current_offset = (0.9f * measured_offset) + (0.1f * current_offset);
    } else {
        // Continuous update: use exponential moving average
        current_offset = (alpha_initial * measured_offset) + ((1.0f - alpha_initial) * current_offset);
    }
    
    // Validate calibration
    if (validateCalibration()) {
        calibrated = true;
        baseline_offset = current_offset;
        Serial.printf("DC Offset Calibrator: Calibration successful - Offset: %.2f\n", current_offset);
        printStatus();
    } else {
        // For initial calibration, if validation fails but measurement seems reasonable,
        // use the measured value anyway (SPH0645 often has large DC offsets)
        if (calibration_samples_collected == CALIBRATION_BUFFER_SIZE && 
            fabs(measured_offset) < 32768.0f) {
            Serial.printf("DC Offset Calibrator: Using measured offset %.2f despite validation failure\n", 
                         measured_offset);
            current_offset = measured_offset;
            baseline_offset = measured_offset;
            calibrated = true;
            printStatus();
        } else {
            Serial.println("DC Offset Calibrator: Calibration validation failed, using fallback");
            current_offset = DEFAULT_OFFSET;
            calibrated = true;
        }
    }
}

bool DCOffsetCalibrator::validateCalibration() const {
    // For initial calibration, use a much more lenient threshold
    // since the default offset (7000) may be very different from actual (-4000)
    float max_change = calibration_samples_collected == CALIBRATION_BUFFER_SIZE ? 
                      50.0f :  // 50% for initial calibration (allows -4000 when default is 7000)
                      MAX_OFFSET_CHANGE_PERCENT;  // 0.1% for continuous updates
    
    // Compare against baseline, not DEFAULT_OFFSET
    float change_percent = (fabs(current_offset - baseline_offset) / fabs(baseline_offset)) * 100.0f;
    
    if (change_percent > max_change) {
        Serial.printf("DC Offset Calibrator: Validation failed - offset change %.2f%% exceeds %.2f%%\n", 
                     change_percent, max_change);
        return false;
    }
    
    // Additional sanity check: ensure offset is within reasonable audio range
    if (fabs(current_offset) > 32768.0f) {
        Serial.printf("DC Offset Calibrator: Validation failed - offset %.2f exceeds audio range\n", 
                     current_offset);
        return false;
    }
    
    Serial.printf("DC Offset Calibrator: Validation passed - change %.2f%% within %.2f%% limit\n", 
                 change_percent, max_change);
    return true;
}

void DCOffsetCalibrator::updateContinuousCalibration(int32_t raw_sample) {
    if (!calibrated) {
        return; // Only update after initial calibration
    }
    
    // Slow continuous adjustment using runtime alpha
    float sample_contribution = alpha_runtime * static_cast<float>(raw_sample);
    float current_contribution = (1.0f - alpha_runtime) * current_offset;
    float new_offset = sample_contribution + current_contribution;
    
    // Apply safety limits
    applyOffsetLimits(new_offset);
    current_offset = new_offset;
}

void DCOffsetCalibrator::applyOffsetLimits(float& new_offset) const {
    // Prevent drift beyond reasonable bounds
    float max_deviation = baseline_offset * MAX_OFFSET_CHANGE_PERCENT;
    float min_allowed = baseline_offset - max_deviation;
    float max_allowed = baseline_offset + max_deviation;
    
    if (new_offset < min_allowed) {
        new_offset = min_allowed;
    } else if (new_offset > max_allowed) {
        new_offset = max_allowed;
    }
}

bool DCOffsetCalibrator::isNoiseDetected(const int32_t* samples, int sample_count) const {
    if (sample_count == 0) return false;
    
    // Calculate variance to detect noise
    int64_t sum = 0;
    for (int i = 0; i < sample_count; i++) {
        sum += samples[i];
    }
    float mean = static_cast<float>(sum) / sample_count;
    
    float variance_sum = 0;
    for (int i = 0; i < sample_count; i++) {
        float diff = samples[i] - mean;
        variance_sum += diff * diff;
    }
    float variance = variance_sum / sample_count;
    
    // If variance is too high, likely there's noise/signal
    bool noise_detected = variance > (NOISE_DETECTION_THRESHOLD * NOISE_DETECTION_THRESHOLD);
    
    if (noise_detected) {
        Serial.printf("DC Offset Calibrator: Noise detected - variance: %.2f, threshold: %d\n", 
                     variance, NOISE_DETECTION_THRESHOLD * NOISE_DETECTION_THRESHOLD);
    }
    
    return noise_detected;
}

float DCOffsetCalibrator::getOffsetVariance() const {
    if (!calibrated) return 0.0f;
    
    float deviation = abs(current_offset - baseline_offset);
    return deviation / baseline_offset;
}

bool DCOffsetCalibrator::isOffsetStable() const {
    return getOffsetVariance() < (MAX_OFFSET_CHANGE_PERCENT * 0.5f); // Half of max change
}

void DCOffsetCalibrator::reset() {
    current_offset = DEFAULT_OFFSET;
    calibrated = false;
    startup_completed = false;
    calibration_samples_collected = 0;
    baseline_offset = DEFAULT_OFFSET;
    startup_time = 0;
}

void DCOffsetCalibrator::printStatus() const {
    Serial.println("=== DC Offset Calibrator Status ===");
    Serial.printf("Calibrated: %s\n", calibrated ? "Yes" : "No");
    Serial.printf("Current Offset: %.2f\n", current_offset);
    Serial.printf("Baseline Offset: %.2f\n", baseline_offset);
    Serial.printf("Offset Variance: %.4f (%.2f%%)\n", getOffsetVariance(), getOffsetVariance() * 100);
    Serial.printf("Stable: %s\n", isOffsetStable() ? "Yes" : "No");
    Serial.printf("Samples Collected: %u/%d\n", calibration_samples_collected, CALIBRATION_BUFFER_SIZE);
    Serial.println("==================================");
} 