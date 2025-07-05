/*
 * DCOffsetNode - Pluggable DC Offset Calibration Module
 * ======================================================
 * 
 * Wraps the battle-tested DC Offset Calibrator in AudioNode interface.
 * 
 * CRITICAL: SPH0645 microphone requires -5200 offset, NOT +360!
 * This was empirically determined through extensive testing.
 * 
 * FEATURES:
 * - Automatic calibration on startup
 * - Continuous offset tracking
 * - High-pass filter for DC drift removal
 * - Multiple operational modes
 */

#ifndef DC_OFFSET_NODE_H
#define DC_OFFSET_NODE_H

#include "../audio_node.h"
#include "../dc_offset_calibrator.h"

class DCOffsetNode : public AudioNode {
public:
    DCOffsetNode() : AudioNode("DCOffset", AudioNodeType::PROCESSOR) {
        calibrator.begin();
    }
    
    // Remove DC offset from audio samples
    bool process(AudioBuffer& input, AudioBuffer& output) override {
        if (!enabled) {
            // Pass through if disabled
            memcpy(output.data, input.data, input.size * sizeof(float));
            output.size = input.size;
            output.timestamp = input.timestamp;
            output.is_silence = input.is_silence;
            output.metadata = input.metadata;
            return true;
        }
        
        uint32_t start = micros();
        
        // Convert float to int16_t for calibrator
        int16_t samples[512];
        for (size_t i = 0; i < input.size && i < 512; i++) {
            samples[i] = (int16_t)input.data[i];
        }
        
        // Process calibration if needed
        if (!calibrator.isCalibrated()) {
            for (size_t i = 0; i < input.size; i++) {
                calibrator.processCalibrationSample(samples[i]);
            }
            if (calibrator.isCalibrated()) {
                Serial.println("DCOffsetNode: Calibration complete!");
                calibrator.printStatus();
            }
        } else {
            // Continuous calibration
            for (size_t i = 0; i < input.size; i++) {
                calibrator.updateContinuousCalibration(samples[i]);
            }
        }
        
        // Apply DC offset correction
        for (size_t i = 0; i < input.size; i++) {
            // Apply offset and high-pass filter
            float corrected = input.data[i] + calibrator.getCurrentOffset();
            
            // Simple high-pass filter to remove DC drift
            static float prev_input = 0.0f;
            static float prev_output = 0.0f;
            float filtered = hp_alpha * (prev_output + corrected - prev_input);
            prev_input = corrected;
            prev_output = filtered;
            
            output.data[i] = filtered;
        }
        
        // Update metadata
        output.size = input.size;
        output.timestamp = input.timestamp;
        output.is_silence = input.is_silence;
        output.metadata = input.metadata;
        output.metadata.dc_offset = calibrator.getCurrentOffset();
        
        measureProcessTime(start);
        return true;
    }
    
    // Configure from JSON
    bool configure(JsonObject& config) override {
        if (config.containsKey("enabled")) {
            enabled = config["enabled"];
        }
        
        // Mode configuration removed - not supported by DCOffsetCalibrator
        
        if (config.containsKey("high_pass_alpha")) {
            hp_alpha = config["high_pass_alpha"];
        }
        
        if (config.containsKey("recalibrate")) {
            if (config["recalibrate"]) {
                calibrator.reset();
                Serial.println("DCOffsetNode: Recalibration triggered");
            }
        }
        
        return true;
    }
    
    // Get current configuration
    void getConfig(JsonObject& config) override {
        AudioNode::getConfig(config);
        config["calibrated"] = calibrator.isCalibrated();
        config["current_offset"] = calibrator.getCurrentOffset();
        // baseline_offset and mode not available in DCOffsetCalibrator
        config["high_pass_alpha"] = hp_alpha;
    }
    
    // Get performance metrics
    void getMetrics(JsonObject& metrics) override {
        AudioNode::getMetrics(metrics);
        // samples_collected not available in DCOffsetCalibrator
        metrics["variance"] = calibrator.getOffsetVariance();
        metrics["stable"] = calibrator.isOffsetStable();
    }
    
private:
    DCOffsetCalibrator calibrator;
    float hp_alpha = 0.999f;  // High-pass filter coefficient (1-2 Hz cutoff at 16kHz)
};

#endif // DC_OFFSET_NODE_H