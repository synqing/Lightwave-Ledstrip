/*
-----------------------------------------------------------------------------------------
    DC Offset Calibration for SPH0645 Microphone
    RESTORED: Battle-tested system from pre-corruption state
    Phase 1 implementation of the phased audio DSP reimplementation
-----------------------------------------------------------------------------------------
*/

#ifndef DC_OFFSET_CALIBRATOR_H
#define DC_OFFSET_CALIBRATOR_H

#include <Arduino.h>
#include <cstdint>

class DCOffsetCalibrator {
private:
    static const int CALIBRATION_BUFFER_SIZE = 1000;
    static const int STARTUP_DELAY_MS = 500;
    static const int NOISE_DETECTION_THRESHOLD = 1000;
    static constexpr float DEFAULT_OFFSET = -5200.0f;  // SPH0645 measured offset
    static constexpr float MAX_OFFSET_CHANGE_PERCENT = 0.1f; // 10% maximum change
    
    int32_t calibration_buffer[CALIBRATION_BUFFER_SIZE];
    float current_offset = DEFAULT_OFFSET;
    float alpha_initial = 0.1f;    // Fast learning during startup
    float alpha_runtime = 0.001f;  // Slow continuous adjustment
    bool calibrated = false;
    bool startup_completed = false;
    uint32_t startup_time = 0;
    uint32_t calibration_samples_collected = 0;
    float baseline_offset = DEFAULT_OFFSET;
    
public:
    DCOffsetCalibrator();
    
    // Main interface
    void begin();
    bool isCalibrated() const { return calibrated; }
    float getCurrentOffset() const { return current_offset; }
    void processCalibrationSample(int32_t raw_sample);
    void updateContinuousCalibration(int32_t raw_sample);
    
    // Safety and validation
    bool isNoiseDetected(const int32_t* samples, int sample_count) const;
    void reset();
    float getOffsetVariance() const;
    
    // Debug and monitoring
    void printStatus() const;
    bool isOffsetStable() const;
    
private:
    void performInitialCalibration();
    bool validateCalibration() const;
    void applyOffsetLimits(float& new_offset) const;
};

#endif 