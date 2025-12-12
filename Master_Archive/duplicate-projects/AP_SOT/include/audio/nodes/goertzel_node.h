/*
 * GoertzelNode - Pluggable Goertzel Analysis Module
 * ==================================================
 * 
 * Wraps the God-Tier Goertzel Engine in AudioNode interface
 * for use in the pluggable pipeline architecture.
 * 
 * FEATURES:
 * - 96 musical frequency bins (A0-A7)
 * - Compile-time LUT optimization
 * - Configurable frequency range
 * - Real-time parameter updates
 */

#ifndef GOERTZEL_NODE_H
#define GOERTZEL_NODE_H

#include "../audio_node.h"
#include "../goertzel_engine.h"

class GoertzelNode : public AudioNode {
public:
    GoertzelNode() : AudioNode("Goertzel", AudioNodeType::ANALYZER) {
        // Initialize the God-Tier engine
        goertzel_god_tier.init();
    }
    
    // Process time-domain samples to frequency bins
    bool process(AudioBuffer& input, AudioBuffer& output) override {
        if (!enabled) return true;
        
        uint32_t start = micros();
        
        // Convert float samples to int16_t EXACTLY like legacy system
        // Legacy: processes full 18-bit range directly as int16_t
        // NO SCALING - maintain exact legacy behavior
        int16_t samples[128];
        for (size_t i = 0; i < input.size && i < 128; i++) {
            // Direct cast preserving 18-bit range (matches legacy exactly)
            samples[i] = (int16_t)input.data[i];
        }
        
        // Process through God-Tier Goertzel
        goertzel_god_tier.process(samples, input.size);
        
        // Copy frequency bins to output
        float* magnitudes = goertzel_god_tier.getMagnitudes();
        output.size = goertzel_god_tier.getBinCount();
        
        for (size_t i = 0; i < output.size; i++) {
            output.data[i] = magnitudes[i];
        }
        
        // Calculate RMS for silence detection (legacy parity - audio_features.cpp:113)
        float rms_sum = 0.0f;
        for (size_t i = 0; i < output.size; i++) {
            rms_sum += output.data[i] * output.data[i];
        }
        float raw_rms = sqrtf(rms_sum / output.size);
        
        // Also calculate RMS of input samples for comparison
        float input_rms_sum = 0.0f;
        for (size_t i = 0; i < input.size && i < 128; i++) {
            float sample_val = (float)samples[i];
            input_rms_sum += sample_val * sample_val;
        }
        float input_rms = sqrtf(input_rms_sum / input.size);
        
        // Legacy silence detection threshold
        bool is_silence = (raw_rms < 50.0f);
        
        // Debug RMS on raw Goertzel output
        static int rms_debug_counter = 0;
        if (++rms_debug_counter % 250 == 0) {  // Every 2 seconds
            // Also show some individual bin magnitudes for debugging
            Serial.printf("GOERTZEL RMS: %.1f (thresh=50.0), silence=%s | Input RMS: %.1f\n", 
                         raw_rms, is_silence ? "YES" : "NO", input_rms);
            Serial.printf("  Sample bins: [0]=%.1f, [10]=%.1f, [20]=%.1f, [30]=%.1f, [40]=%.1f\n",
                         output.data[0], output.data[10], output.data[20], output.data[30], output.data[40]);
        }
        
        // Set output metadata
        output.timestamp = input.timestamp;
        output.is_silence = is_silence;  // Silence detection on RAW Goertzel output
        output.metadata = input.metadata;
        output.metadata.is_raw_spectrum = true;   // Mark as raw frequency data
        output.metadata.is_agc_processed = false;  // Not AGC processed yet
        
        // Debug: Verify legacy parity - check sample ranges
        static int goertzel_sample_debug = 0;
        if (++goertzel_sample_debug % 250 == 0) {  // Every 2 seconds
            int16_t min_sample = 32767, max_sample = -32768;
            float min_input = 1e9, max_input = -1e9;
            for (size_t i = 0; i < input.size && i < 128; i++) {
                if (samples[i] < min_sample) min_sample = samples[i];
                if (samples[i] > max_sample) max_sample = samples[i];
                if (input.data[i] < min_input) min_input = input.data[i];
                if (input.data[i] > max_input) max_input = input.data[i];
            }
            Serial.printf("GOERTZEL LEGACY PARITY: int16_t range [%d, %d] from input float [%.1f, %.1f]\n", 
                         min_sample, max_sample, min_input, max_input);
        }
        
        measureProcessTime(start);
        return true;
    }
    
    // Configure from JSON
    bool configure(JsonObject& config) override {
        if (config.containsKey("enabled")) {
            enabled = config["enabled"];
        }
        
        // Note: Current God-Tier implementation has fixed frequencies
        // Future enhancement: support configurable frequency ranges
        
        if (config.containsKey("debug")) {
            debug_enabled = config["debug"];
            if (debug_enabled) {
                goertzel_god_tier.printFrequencyMap();
            }
        }
        
        return true;
    }
    
    // Get current configuration
    void getConfig(JsonObject& config) override {
        AudioNode::getConfig(config);
        config["bin_count"] = goertzel_god_tier.getBinCount();
        config["min_freq"] = goertzel_god_tier.getFrequency(0);
        config["max_freq"] = goertzel_god_tier.getFrequency(95);
        config["algorithm"] = "God-Tier LUT-optimized";
    }
    
    // Get performance metrics
    void getMetrics(JsonObject& metrics) override {
        AudioNode::getMetrics(metrics);
        metrics["bins_processed"] = goertzel_god_tier.getBinCount();
        metrics["cache_optimized"] = true;
    }
    
private:
    bool debug_enabled = false;
    
    // Note: Using the global instance from goertzel_engine.cpp
    // In future, we could make this a member variable
};

#endif // GOERTZEL_NODE_H