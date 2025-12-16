/*
 * ZoneMapperNode - Frequency to Zone Energy Mapping Module
 * =========================================================
 * 
 * Maps frequency bins to spatial zones for LED visualization.
 * 
 * ARCHITECTURE:
 * - Accepts AGC-processed frequency data (for visualization)
 * - Maps 96 frequency bins to configurable number of zones
 * - Supports both linear and logarithmic mapping
 * - Includes smoothing for visual stability
 * 
 * CRITICAL: This node is for VISUALIZATION ONLY!
 * It should receive AGC-processed data, not raw data.
 * Beat detection happens on a separate RAW data path.
 */

#ifndef ZONE_MAPPER_NODE_H
#define ZONE_MAPPER_NODE_H

#include "../audio_node.h"
#include <algorithm>

class ZoneMapperNode : public AudioNode {
public:
    static constexpr size_t MAX_ZONES = 256;
    static constexpr size_t GOERTZEL_BINS = 96;
    
    ZoneMapperNode() : AudioNode("ZoneMapper", AudioNodeType::SINK) {
        // Initialize zone mapping
        initializeMapping();
    }
    
    // Map frequency bins to zone energies
    bool process(AudioBuffer& input, AudioBuffer& output) override {
        if (!enabled || input.size == 0) {
            // Clear output if disabled
            memset(output.data, 0, num_zones * sizeof(float));
            output.size = num_zones;
            output.timestamp = input.timestamp;
            output.is_silence = input.is_silence;
            output.metadata = input.metadata;
            return true;
        }
        
        uint32_t start = micros();
        
        // Ensure we have the expected number of frequency bins
        if (input.size != GOERTZEL_BINS) {
            Serial.printf("ZoneMapperNode: Expected %d bins, got %d\n", 
                         GOERTZEL_BINS, input.size);
            return false;
        }
        
        // Silence detection already done in Goertzel node on raw data
        bool is_silence = input.is_silence;
        
        // Clear zone accumulator
        memset(zone_accumulator, 0, num_zones * sizeof(float));
        memset(zone_counts, 0, num_zones * sizeof(int));
        
        // Map frequency bins to zones based on mapping mode
        if (mapping_mode == LOGARITHMIC) {
            mapLogarithmic(input.data);
        } else {
            mapLinear(input.data);
        }
        
        // Calculate average energy per zone
        float raw_energies[MAX_ZONES];
        float max_energy = 0.0f;
        
        for (size_t i = 0; i < num_zones; i++) {
            if (zone_counts[i] > 0) {
                raw_energies[i] = zone_accumulator[i] / zone_counts[i];
            } else {
                raw_energies[i] = 0.0f;
            }
            
            // Track maximum for normalization
            if (raw_energies[i] > max_energy) {
                max_energy = raw_energies[i];
            }
        }
        
        // Normalize to 0-1 range (similar to legacy system)
        float normalization_factor = (max_energy > 0.01f) ? (0.95f / max_energy) : 1.0f;
        
        // Apply silence handling (legacy parity)
        if (is_silence) {
            // During silence, output zero (legacy behavior)
            for (size_t i = 0; i < num_zones; i++) {
                zone_energies[i] = 0.0f;
                output.data[i] = 0.0f;
            }
        } else {
            // Apply normalization, smoothing, and gamma
            for (size_t i = 0; i < num_zones; i++) {
                // Normalize the raw energy
                float normalized_energy = raw_energies[i] * normalization_factor;
                
                // Apply smoothing filter
                zone_energies[i] = (smoothing_factor * zone_energies[i]) + 
                                  ((1.0f - smoothing_factor) * normalized_energy);
                
                // Apply gamma curve for better visual response
                output.data[i] = powf(zone_energies[i], gamma);
                
                // Clamp to valid range
                output.data[i] = constrain(output.data[i], 0.0f, 1.0f);
            }
        }
        
        // Set output metadata
        output.size = num_zones;
        output.timestamp = input.timestamp;
        output.is_silence = is_silence;  // Propagate silence flag from Goertzel node
        output.metadata = input.metadata;
        output.metadata.zone_count = num_zones;
        
        // Debug output
        if (debug_counter++ % 500 == 0 && debug_enabled) {
            Serial.printf("ZoneMapper: zones=%d, max_raw=%.1f, norm_factor=%.4f, avg_output=%.3f\n",
                         num_zones, max_energy, normalization_factor, calculateAverageEnergy(output.data));
        }
        
        measureProcessTime(start);
        return true;
    }
    
    // Configure from JSON
    bool configure(JsonObject& config) override {
        if (config.containsKey("enabled")) {
            enabled = config["enabled"];
        }
        
        if (config.containsKey("num_zones")) {
            size_t new_zones = config["num_zones"];
            if (new_zones > 0 && new_zones <= MAX_ZONES) {
                num_zones = new_zones;
                initializeMapping();
            }
        }
        
        if (config.containsKey("mapping_mode")) {
            const char* mode = config["mapping_mode"];
            if (strcmp(mode, "linear") == 0) {
                mapping_mode = LINEAR;
            } else if (strcmp(mode, "logarithmic") == 0) {
                mapping_mode = LOGARITHMIC;
            }
            initializeMapping();
        }
        
        if (config.containsKey("smoothing_factor")) {
            smoothing_factor = config["smoothing_factor"];
            smoothing_factor = constrain(smoothing_factor, 0.0f, 0.99f);
        }
        
        if (config.containsKey("gamma")) {
            gamma = config["gamma"];
            gamma = constrain(gamma, 0.1f, 3.0f);
        }
        
        if (config.containsKey("debug")) {
            debug_enabled = config["debug"];
        }
        
        return true;
    }
    
    // Get current configuration
    void getConfig(JsonObject& config) override {
        AudioNode::getConfig(config);
        config["num_zones"] = num_zones;
        config["mapping_mode"] = (mapping_mode == LINEAR) ? "linear" : "logarithmic";
        config["smoothing_factor"] = smoothing_factor;
        config["gamma"] = gamma;
        config["input_bins"] = GOERTZEL_BINS;
    }
    
    // Get zone energies for external access
    size_t getNumZones() const { return num_zones; }
    const float* getZoneEnergies() const { return zone_energies; }
    
private:
    enum MappingMode {
        LINEAR,
        LOGARITHMIC
    };
    
    // Configuration
    size_t num_zones = 36;  // Default for 36-LED configuration
    MappingMode mapping_mode = LOGARITHMIC;
    float smoothing_factor = 0.7f;
    float gamma = 1.5f;  // Gamma curve for visual response
    bool debug_enabled = false;
    
    // Working buffers
    float zone_energies[MAX_ZONES] = {0};
    float zone_accumulator[MAX_ZONES] = {0};
    int zone_counts[MAX_ZONES] = {0};
    int bin_to_zone_map[GOERTZEL_BINS] = {0};
    int debug_counter = 0;
    
    // Initialize bin-to-zone mapping
    void initializeMapping() {
        memset(zone_energies, 0, sizeof(zone_energies));
        
        if (mapping_mode == LINEAR) {
            // Linear mapping: equal bins per zone
            float bins_per_zone = (float)GOERTZEL_BINS / num_zones;
            for (int bin = 0; bin < GOERTZEL_BINS; bin++) {
                int zone = (int)(bin / bins_per_zone);
                zone = constrain(zone, 0, (int)num_zones - 1);
                bin_to_zone_map[bin] = zone;
            }
        } else {
            // Logarithmic mapping: more zones for lower frequencies
            for (int bin = 0; bin < GOERTZEL_BINS; bin++) {
                // Use log scale mapping
                float normalized_bin = (float)bin / (GOERTZEL_BINS - 1);
                float log_position = log10f(1.0f + 9.0f * normalized_bin) / log10f(10.0f);
                int zone = (int)(log_position * num_zones);
                zone = constrain(zone, 0, (int)num_zones - 1);
                bin_to_zone_map[bin] = zone;
            }
        }
    }
    
    // Linear mapping
    void mapLinear(const float* freq_bins) {
        for (int bin = 0; bin < GOERTZEL_BINS; bin++) {
            int zone = bin_to_zone_map[bin];
            zone_accumulator[zone] += freq_bins[bin];
            zone_counts[zone]++;
        }
    }
    
    // Logarithmic mapping
    void mapLogarithmic(const float* freq_bins) {
        // Same as linear but with pre-computed logarithmic mapping
        mapLinear(freq_bins);
    }
    
    // Calculate average energy across all zones
    float calculateAverageEnergy(const float* zones) const {
        float sum = 0.0f;
        for (size_t i = 0; i < num_zones; i++) {
            sum += zones[i];
        }
        return sum / num_zones;
    }
};

#endif // ZONE_MAPPER_NODE_H