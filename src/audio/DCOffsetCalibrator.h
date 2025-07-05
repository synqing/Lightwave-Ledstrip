#ifndef DC_OFFSET_CALIBRATOR_H
#define DC_OFFSET_CALIBRATOR_H

#include <Arduino.h>

/**
 * DCOffsetCalibrator - Robust DC offset removal for audio signals
 * 
 * Features:
 * - Dynamic DC offset calibration
 * - High-pass filter for DC blocking
 * - Automatic recalibration on drift detection
 * - Statistics tracking for debugging
 */
class DCOffsetCalibrator {
private:
    // Calibration state
    static constexpr uint32_t CALIBRATION_SAMPLES = 1600;  // ~100ms at 16kHz
    static constexpr uint32_t RECALIBRATION_INTERVAL = 10000;  // 10 seconds
    static constexpr float DRIFT_THRESHOLD_PERCENT = 0.5f;  // 0.5% of full scale
    static constexpr float FULL_SCALE_18BIT = 131072.0f;  // ±2^17 for 18-bit signed
    static constexpr float DRIFT_THRESHOLD = FULL_SCALE_18BIT * DRIFT_THRESHOLD_PERCENT / 100.0f;  // ~655 counts
    
    // DC offset tracking
    float dcOffset = 0.0f;
    float dcOffsetSmooth = 0.0f;  // Smoothed estimate
    uint32_t calibrationCount = 0;
    uint32_t lastCalibrationTime = 0;
    
    // High-pass filter state (DC blocker)
    static constexpr float DC_BLOCK_ALPHA = 0.98375f;  // ~16Hz cutoff at 16kHz
    float hpfPrevIn = 0.0f;
    float hpfPrevOut = 0.0f;
    
    // Statistics
    float minValue = 0.0f;
    float maxValue = 0.0f;
    float rmsValue = 0.0f;
    uint32_t sampleCount = 0;
    
    // Drift detection
    float dcAccumulator = 0.0f;
    uint32_t driftCheckSamples = 0;
    
public:
    DCOffsetCalibrator() = default;
    
    /**
     * Reset calibration state
     */
    void reset() {
        dcOffset = 0.0f;
        dcOffsetSmooth = 0.0f;
        calibrationCount = 0;
        hpfPrevIn = 0.0f;
        hpfPrevOut = 0.0f;
        dcAccumulator = 0.0f;
        driftCheckSamples = 0;
        lastCalibrationTime = millis();
    }
    
    /**
     * Process a single audio sample
     * @param sample Raw input sample
     * @return DC-blocked output sample
     */
    float processSample(int32_t sample) {
        float input = static_cast<float>(sample);
        
        // During calibration phase, accumulate DC offset
        if (calibrationCount < CALIBRATION_SAMPLES) {
            dcOffset = (dcOffset * calibrationCount + input) / (calibrationCount + 1);
            calibrationCount++;
            
            if (calibrationCount == CALIBRATION_SAMPLES) {
                Serial.printf("[DCCalibrator] Initial calibration complete: DC offset = %.2f\n", dcOffset);
                dcOffsetSmooth = dcOffset;
                lastCalibrationTime = millis();
            }
        }
        
        // Remove DC offset
        float dcRemoved = input - dcOffsetSmooth;
        
        // Apply high-pass filter: y[n] = x[n] - x[n-1] + α * y[n-1]
        float output = dcRemoved - hpfPrevIn + DC_BLOCK_ALPHA * hpfPrevOut;
        hpfPrevIn = dcRemoved;
        hpfPrevOut = output;
        
        // Update statistics
        updateStatistics(output);
        
        // Check for DC drift
        checkDrift(input);
        
        return output;
    }
    
    /**
     * Process a buffer of samples
     */
    void processBuffer(int32_t* input, float* output, size_t count) {
        for (size_t i = 0; i < count; i++) {
            output[i] = processSample(input[i]);
        }
    }
    
    /**
     * Get current DC offset estimate
     */
    float getDCOffset() const { return dcOffsetSmooth; }
    
    /**
     * Get calibration status
     */
    bool isCalibrated() const { return calibrationCount >= CALIBRATION_SAMPLES; }
    
    /**
     * Get signal statistics
     */
    void getStatistics(float& min, float& max, float& rms) const {
        min = minValue;
        max = maxValue;
        rms = rmsValue;
    }
    
    /**
     * Force recalibration
     */
    void recalibrate() {
        Serial.println("[DCCalibrator] Manual recalibration triggered");
        calibrationCount = 0;
        dcOffset = 0.0f;
    }
    
private:
    void updateStatistics(float sample) {
        if (sampleCount == 0) {
            minValue = maxValue = sample;
            rmsValue = 0.0f;
        } else {
            minValue = fmin(minValue, sample);
            maxValue = fmax(maxValue, sample);
            
            // Running RMS calculation
            float alpha = 0.001f;  // Smoothing factor
            rmsValue = sqrt(alpha * sample * sample + (1.0f - alpha) * rmsValue * rmsValue);
        }
        sampleCount++;
    }
    
    void checkDrift(float rawSample) {
        // Accumulate DC component
        dcAccumulator += rawSample;
        driftCheckSamples++;
        
        // Check every 1000 samples (~62.5ms at 16kHz)
        if (driftCheckSamples >= 1000) {
            float currentDC = dcAccumulator / driftCheckSamples;
            float drift = fabs(currentDC - dcOffsetSmooth);
            
            // Check for significant drift or periodic recalibration
            uint32_t now = millis();
            bool shouldRecalibrate = (drift > DRIFT_THRESHOLD) || 
                                   (now - lastCalibrationTime > RECALIBRATION_INTERVAL);
            
            if (shouldRecalibrate && calibrationCount >= CALIBRATION_SAMPLES) {
                Serial.printf("[DCCalibrator] Drift detected: %.2f (threshold: %.2f = %.2f%%)\n", 
                            drift, DRIFT_THRESHOLD, DRIFT_THRESHOLD_PERCENT);
                
                // Smooth transition to new DC offset
                float alpha = 0.1f;  // Smoothing factor
                dcOffsetSmooth = alpha * currentDC + (1.0f - alpha) * dcOffsetSmooth;
                
                lastCalibrationTime = now;
            }
            
            // Reset accumulators
            dcAccumulator = 0.0f;
            driftCheckSamples = 0;
        }
    }
};

#endif // DC_OFFSET_CALIBRATOR_H