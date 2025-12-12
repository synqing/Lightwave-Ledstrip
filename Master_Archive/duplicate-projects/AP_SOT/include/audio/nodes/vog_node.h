/*
 * Voice of God (VoG) Confidence Engine
 * ====================================
 * 
 * A decoupled oracle that measures the "divine significance" of audio events
 * by comparing raw dynamic energy against AGC-normalized energy.
 * 
 * DIVINE PURPOSE:
 * - Confidence Engine for beat validation
 * - Hardness modulator for visual intensity
 * - Runs asynchronously at 10-12Hz to avoid impacting real-time pipeline
 * 
 * THE VOG CONTRACT:
 * - Inputs: Raw spectrum + AGC spectrum (read-only)
 * - Outputs: vog_confidence (0-1), beat_hardness (0-1)
 * 
 * ARCHITECTURAL MANDATE:
 * This node operates OUTSIDE the main audio pipeline!
 * It is an asynchronous oracle, not a real-time processor.
 */

#ifndef VOG_NODE_H
#define VOG_NODE_H

#include "../audio_node.h"
#include "../audio_state.h"
#include <cmath>

class VoGNode : public AudioNode {
public:
    // Constructor accepts pointers to monitor raw and AGC buffers
    VoGNode(AudioBuffer* raw_spectrum_ptr = nullptr, 
            AudioBuffer* agc_spectrum_ptr = nullptr) 
        : AudioNode("VoG", AudioNodeType::ANALYZER),
          raw_spectrum(raw_spectrum_ptr),
          agc_spectrum(agc_spectrum_ptr) {
        
        // VoG runs at lower frequency - 10-12Hz
        execution_interval_ms = 85;  // ~11.8Hz
    }
    
    // Set buffer pointers after construction if needed
    void setSpectrumPointers(AudioBuffer* raw, AudioBuffer* agc) {
        raw_spectrum = raw;
        agc_spectrum = agc;
    }
    
    // Process is called every frame but only executes at 10-12Hz
    bool process(AudioBuffer& input, AudioBuffer& output) override {
        // VoG doesn't process inline data - it monitors external buffers
        // Pass through input unchanged
        if (input.data != output.data) {
            memcpy(output.data, input.data, input.size * sizeof(float));
        }
        output.size = input.size;
        output.timestamp = input.timestamp;
        output.is_silence = input.is_silence;
        output.metadata = input.metadata;
        
        // Check if it's time to run VoG calculation
        uint32_t now = millis();
        if (now - last_execution_time >= execution_interval_ms) {
            last_execution_time = now;
            
            // Execute the divine calculation
            if (raw_spectrum && agc_spectrum) {
                calculateVoG();
            }
        }
        
        return true;
    }
    
    // Configure from JSON
    bool configure(JsonObject& config) override {
        if (config.containsKey("enabled")) {
            enabled = config["enabled"];
        }
        
        if (config.containsKey("execution_rate_hz")) {
            float rate = config["execution_rate_hz"];
            rate = constrain(rate, 5.0f, 20.0f);  // 5-20Hz range
            execution_interval_ms = 1000.0f / rate;
        }
        
        if (config.containsKey("nonlinearity")) {
            nonlinearity_power = config["nonlinearity"];
            nonlinearity_power = constrain(nonlinearity_power, 1.0f, 4.0f);
        }
        
        if (config.containsKey("smoothing")) {
            smoothing_factor = config["smoothing"];
            smoothing_factor = constrain(smoothing_factor, 0.0f, 0.95f);
        }
        
        return true;
    }
    
    // Get current VoG outputs
    float getVoGConfidence() const { return vog_confidence; }
    float getBeatHardness() const { return beat_hardness; }
    
private:
    // Pointers to spectrum buffers we monitor
    AudioBuffer* raw_spectrum;
    AudioBuffer* agc_spectrum;
    
    // Timing control
    uint32_t last_execution_time = 0;
    uint32_t execution_interval_ms = 85;  // ~11.8Hz
    
    // VoG parameters
    float nonlinearity_power = 2.0f;      // Squaring by default
    float smoothing_factor = 0.3f;        // Reduced for faster response (was 0.7f)
    
    // VoG outputs
    float vog_confidence = 0.0f;
    float beat_hardness = 0.0f;
    
    // Internal state
    float smoothed_confidence = 0.0f;
    float energy_history[4] = {0};       // Short history for derivative
    uint8_t history_index = 0;
    
    // The divine calculation
    void calculateVoG() {
        if (!raw_spectrum || !agc_spectrum || 
            raw_spectrum->size == 0 || agc_spectrum->size == 0) {
            return;
        }
        
        // Calculate total energy in both spectrums
        float raw_energy = 0.0f;
        float agc_energy = 0.0f;
        
        size_t num_bins = min(raw_spectrum->size, agc_spectrum->size);
        
        // Calculate actual energy (sum of squares), not just sum of magnitudes
        for (size_t i = 0; i < num_bins; i++) {
            raw_energy += raw_spectrum->data[i] * raw_spectrum->data[i];
            agc_energy += agc_spectrum->data[i] * agc_spectrum->data[i];
        }
        
        // Take square root to get RMS
        raw_energy = sqrtf(raw_energy / num_bins);
        agc_energy = sqrtf(agc_energy / num_bins);
        
        // The divine ratio: comparing raw vs AGC energy
        // When raw >> AGC, we have a significant transient (AGC hasn't caught up)
        // When AGC >> raw, AGC is boosting quiet signals
        // We want to detect when raw significantly exceeds what AGC expects
        float energy_ratio = 1.0f;
        if (agc_energy > 0.1f && raw_energy > 0.1f) {  // Avoid division by zero
            // Use the larger as denominator to avoid ratios > 1 being inverted
            if (raw_energy > agc_energy) {
                energy_ratio = raw_energy / agc_energy;  // > 1 when raw is louder
            } else {
                energy_ratio = 2.0f - (agc_energy / raw_energy);  // < 1 when AGC is boosting
            }
        }
        
        // Store in history for derivative calculation
        energy_history[history_index] = energy_ratio;
        history_index = (history_index + 1) & 3;  // Circular buffer
        
        // Calculate energy derivative (rate of change)
        float oldest = energy_history[(history_index + 0) & 3];
        float newest = energy_history[(history_index + 3) & 3];
        float derivative = (newest - oldest) / 3.0f;
        
        // The Voice of God speaks through dynamics
        // Ratio > 1.0 = raw exceeds AGC (transient/beat)
        // Ratio < 1.0 = AGC is boosting (quiet/steady state)
        // We want confidence when raw significantly exceeds AGC expectations
        float raw_confidence = 0.0f;
        if (energy_ratio > 1.0f) {
            // Raw is louder than AGC expects - this is a transient!
            raw_confidence = (energy_ratio - 1.0f) * 2.0f;  // Amplify the difference
        }
        
        // Include derivative - sudden changes are more significant
        if (derivative > 0.1f) {
            raw_confidence += derivative * 0.5f;
        }
        
        // Apply nonlinearity to emphasize strong transients
        if (raw_confidence > 0) {
            raw_confidence = powf(raw_confidence, nonlinearity_power);
        } else {
            raw_confidence = 0.0f;
        }
        
        // Normalize to 0-1 range with soft clipping
        raw_confidence = tanhf(raw_confidence * 0.5f);
        
        // Apply temporal smoothing
        smoothed_confidence = (smoothing_factor * smoothed_confidence) + 
                            ((1.0f - smoothing_factor) * raw_confidence);
        
        // Ensure we don't get stuck at 0
        if (smoothed_confidence < 0.001f && raw_confidence > 0.001f) {
            smoothed_confidence = raw_confidence * 0.1f;  // Jump-start from zero
        }
        
        // Final VoG confidence
        vog_confidence = smoothed_confidence;
        
        // Beat hardness is a perceptually scaled version
        // Using cube root for perceptual scaling
        beat_hardness = powf(vog_confidence, 0.333f);
        
        // Update global audio state
        audio_state.ext.beat.vog_confidence = vog_confidence;
        audio_state.ext.beat.beat_hardness = beat_hardness;
        
        // Enhanced debug output to understand values
        static int debug_counter = 0;
        if (++debug_counter % 10 == 0) {  // Every ~1 second at 12Hz
            Serial.printf("VoG DEBUG: raw=%.1f, agc=%.1f, ratio=%.3f, raw_conf=%.3f, smooth=%.3f, final=%.3f\n",
                         raw_energy, agc_energy, energy_ratio, raw_confidence, 
                         smoothed_confidence, vog_confidence);
        }
        
        // Alert when VoG detects significant event
        if (vog_confidence > 0.3f) {
            Serial.printf("VoG SPEAKS: confidence=%.2f, hardness=%.2f, ratio=%.1f\n",
                         vog_confidence, beat_hardness, energy_ratio);
        }
    }
};

#endif // VOG_NODE_H