/*
 * MultibandAGCNode - Pluggable Multiband AGC Module
 * ==================================================
 * 
 * Wraps the cochlear-inspired Multiband AGC system in AudioNode interface.
 * 
 * CRITICAL: This node is for VISUALIZATION ONLY!
 * Beat detection must use RAW frequency data, not AGC-processed.
 * 
 * FEATURES:
 * - 4-band cochlear processing (bass/low-mid/high-mid/treble)
 * - Independent gain control per band
 * - Cross-band coupling to prevent artifacts
 * - Dynamic time constants
 * - A-weighting support
 */

#ifndef MULTIBAND_AGC_NODE_H
#define MULTIBAND_AGC_NODE_H

#include "../audio_node.h"
#include "../multiband_agc_system.h"

class MultibandAGCNode : public AudioNode {
public:
    MultibandAGCNode() : AudioNode("MultibandAGC", AudioNodeType::PROCESSOR) {
        // Initialize with default sample rate
        agc.init(16000.0f);
    }
    
    // Process frequency bins through multiband AGC
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
        
        // Process through multiband AGC
        agc.process(input.data, output.data, input.size, input.is_silence);
        
        // Copy metadata
        output.size = input.size;
        output.timestamp = input.timestamp;
        output.is_silence = input.is_silence;
        output.metadata = input.metadata;
        output.metadata.is_raw_spectrum = false;  // No longer raw after AGC
        output.metadata.is_agc_processed = true;  // Mark as AGC processed
        
        measureProcessTime(start);
        return true;
    }
    
    // Configure from JSON
    bool configure(JsonObject& config) override {
        if (config.containsKey("enabled")) {
            enabled = config["enabled"];
        }
        
        if (config.containsKey("a_weighting")) {
            agc.setAWeighting(config["a_weighting"]);
        }
        
        if (config.containsKey("sample_rate")) {
            float sample_rate = config["sample_rate"];
            agc.init(sample_rate);
        }
        
        // Future: Allow per-band configuration
        // if (config.containsKey("bands")) { ... }
        
        return true;
    }
    
    // Get current configuration
    void getConfig(JsonObject& config) override {
        AudioNode::getConfig(config);
        config["band_count"] = 4;
        config["bands"] = "bass/low-mid/high-mid/treble";
        config["cross_coupling"] = true;
        config["dynamic_time_constants"] = true;
    }
    
    // Get performance metrics
    void getMetrics(JsonObject& metrics) override {
        AudioNode::getMetrics(metrics);
        
        // Get band-specific metrics
        JsonArray bands = metrics.createNestedArray("bands");
        for (int i = 0; i < 4; i++) {
            JsonObject band = bands.createNestedObject();
            float gain, energy, ceiling;
            agc.getBandInfo(i, gain, energy, ceiling);
            
            band["index"] = i;
            band["gain"] = gain;
            band["energy"] = energy;
            band["ceiling"] = ceiling;
        }
    }
    
    // Get AGC system for direct access (e.g., visualization)
    MultibandAGCSystem& getAGC() { return agc; }
    
private:
    MultibandAGCSystem agc;
};

#endif // MULTIBAND_AGC_NODE_H