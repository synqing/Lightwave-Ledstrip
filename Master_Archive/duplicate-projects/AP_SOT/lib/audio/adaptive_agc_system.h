/*
 * Adaptive AGC System - Based on SENSORY_BRIDGE approach
 * ======================================================
 * Tracks minimum silent levels and provides dynamic AGC
 * to ensure clean visualization without distortion
 */

#ifndef ADAPTIVE_AGC_SYSTEM_H
#define ADAPTIVE_AGC_SYSTEM_H

#include <Arduino.h>
#include <math.h>

class AdaptiveAGCSystem {
private:
    // AGC Constants from SENSORY_BRIDGE
    static constexpr float AGC_FLOOR_SCALING_FACTOR = 0.01f;
    static constexpr float AGC_FLOOR_MIN_CLAMP_RAW = 10.0f;
    static constexpr float AGC_FLOOR_MAX_CLAMP_RAW = 30000.0f;
    static constexpr float AGC_FLOOR_MIN_CLAMP_SCALED = 0.5f;
    static constexpr float AGC_FLOOR_MAX_CLAMP_SCALED = 100.0f;
    static constexpr float AGC_FLOOR_RECOVERY_RATE = 50.0f;
    static constexpr float AGC_DEADBAND_FACTOR = 1.50f;
    
    // State tracking
    float min_silent_level_tracker = 65535.0f;
    float dynamic_agc_floor_raw = 10.0f;
    float dynamic_agc_floor_scaled = 0.5f;
    float goertzel_max_value = 1.0f;
    
    // Sweet spot detection
    enum SweetSpotState {
        SILENT = -1,
        NORMAL = 0,
        LOUD = 1
    };
    
    SweetSpotState current_state = NORMAL;
    SweetSpotState pending_state = NORMAL;
    uint32_t state_change_time = 0;
    static constexpr uint32_t MIN_STATE_DURATION = 1500; // 1.5 seconds
    
    // Smoothing
    float max_waveform_smooth = 0.0f;
    static constexpr float AMPLITUDE_SMOOTH_FACTOR = 0.2f;  // 20% new, 80% old
    static constexpr float ATTACK_RATE = 0.0050f;  // Fast attack
    static constexpr float RELEASE_RATE = 0.0025f; // Slow release
    
    // Calibrated thresholds
    float sweet_spot_min_level = 100.0f;  // Set by noise calibration
    float sweet_spot_max_level = 20000.0f;
    
public:
    void init() {
        min_silent_level_tracker = 65535.0f;
        dynamic_agc_floor_raw = 10.0f;
        dynamic_agc_floor_scaled = 0.5f;
        goertzel_max_value = 1.0f;
        current_state = NORMAL;
        pending_state = NORMAL;
        state_change_time = millis();
        max_waveform_smooth = 0.0f;
    }
    
    // Set thresholds from noise calibration
    void setThresholds(float min_level, float max_level) {
        sweet_spot_min_level = min_level;
        sweet_spot_max_level = max_level;
        Serial.printf("AGC Thresholds: min=%.1f, max=%.1f\n", min_level, max_level);
    }
    
    // Update with current audio amplitude
    void updateAmplitude(int16_t* samples, int num_samples) {
        // Find peak in current chunk
        float max_val = 0.0f;
        for (int i = 0; i < num_samples; i++) {
            float abs_val = fabsf(samples[i]);
            if (abs_val > max_val) {
                max_val = abs_val;
            }
        }
        
        // Smooth the amplitude to prevent jitter
        max_waveform_smooth = (max_val * AMPLITUDE_SMOOTH_FACTOR) + 
                             (max_waveform_smooth * (1.0f - AMPLITUDE_SMOOTH_FACTOR));
        
        // Update sweet spot state with hysteresis
        updateSweetSpotState();
        
        // Track minimum silent level
        if (current_state == SILENT) {
            if (max_waveform_smooth < min_silent_level_tracker) {
                min_silent_level_tracker = max_waveform_smooth;
            } else {
                // Slowly recover upwards
                min_silent_level_tracker += AGC_FLOOR_RECOVERY_RATE;
                if (min_silent_level_tracker > 65535.0f) {
                    min_silent_level_tracker = 65535.0f;
                }
            }
        }
        
        // Update dynamic AGC floor
        dynamic_agc_floor_raw = min_silent_level_tracker;
        dynamic_agc_floor_raw = constrain(dynamic_agc_floor_raw, 
                                         AGC_FLOOR_MIN_CLAMP_RAW, 
                                         AGC_FLOOR_MAX_CLAMP_RAW);
        
        dynamic_agc_floor_scaled = dynamic_agc_floor_raw * AGC_FLOOR_SCALING_FACTOR;
        dynamic_agc_floor_scaled = constrain(dynamic_agc_floor_scaled,
                                           AGC_FLOOR_MIN_CLAMP_SCALED,
                                           AGC_FLOOR_MAX_CLAMP_SCALED);
    }
    
    // Process frequency magnitudes with AGC
    void processMagnitudes(float* magnitudes, int num_bins, float* noise_floor = nullptr) {
        // Find max value for normalization
        float max_value = 0.0f;
        
        // Apply noise floor subtraction if available
        for (int i = 0; i < num_bins; i++) {
            if (noise_floor && noise_floor[i] > 0) {
                // Subtract noise floor with 1.5x margin
                magnitudes[i] -= noise_floor[i] * 1.5f;
                if (magnitudes[i] < 0.0f) {
                    magnitudes[i] = 0.0f;
                }
            }
            
            if (magnitudes[i] > max_value) {
                max_value = magnitudes[i];
            }
        }
        
        // Ensure minimum based on dynamic floor
        if (max_value < dynamic_agc_floor_scaled) {
            max_value = dynamic_agc_floor_scaled;
        }
        
        // Apply attack/release envelope
        if (max_value > goertzel_max_value) {
            float delta = max_value - goertzel_max_value;
            goertzel_max_value += delta * ATTACK_RATE;
        } else if (goertzel_max_value > max_value) {
            float delta = goertzel_max_value - max_value;
            goertzel_max_value -= delta * RELEASE_RATE;
        }
        
        // Deadband AGC - reset if below threshold
        if (max_value < dynamic_agc_floor_scaled * AGC_DEADBAND_FACTOR) {
            goertzel_max_value = dynamic_agc_floor_scaled * AGC_DEADBAND_FACTOR;
        }
        
        // Normalize magnitudes
        if (goertzel_max_value > 0.001f) {
            float multiplier = 1.0f / goertzel_max_value;
            for (int i = 0; i < num_bins; i++) {
                magnitudes[i] *= multiplier;
                // Final clamp to [0,1]
                magnitudes[i] = constrain(magnitudes[i], 0.0f, 1.0f);
            }
        }
    }
    
    // Get current state
    typedef SweetSpotState State;
    State getState() const { return current_state; }
    bool isSilent() const { return current_state == SILENT; }
    float getDynamicFloor() const { return dynamic_agc_floor_scaled; }
    float getAGCLevel() const { return goertzel_max_value; }
    
    // Additional getters for metrics
    float getCurrentGain() const { return goertzel_max_value > 0 ? 1.0f / goertzel_max_value : 1.0f; }
    float getTargetGain() const { return 1.0f; }  // Always targeting normalized output
    float getNoiseFloor() const { return dynamic_agc_floor_scaled; }
    
private:
    void updateSweetSpotState() {
        // Determine target state based on amplitude
        SweetSpotState target_state;
        
        if (max_waveform_smooth <= sweet_spot_min_level) {
            target_state = SILENT;
        } else if (max_waveform_smooth >= sweet_spot_max_level) {
            target_state = LOUD;
        } else {
            target_state = NORMAL;
        }
        
        // Apply hysteresis
        uint32_t now = millis();
        
        if (target_state != pending_state) {
            pending_state = target_state;
            state_change_time = now;
        } else if (pending_state != current_state) {
            // Check if we've been in pending state long enough
            if (now - state_change_time >= MIN_STATE_DURATION) {
                current_state = pending_state;
                
                const char* state_names[] = {"SILENT", "NORMAL", "LOUD"};
                Serial.printf("AGC State: %s (amp=%.1f)\n", 
                             state_names[current_state + 1], max_waveform_smooth);
            }
        }
    }
};

#endif // ADAPTIVE_AGC_SYSTEM_H