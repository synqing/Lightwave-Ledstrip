/*
 * Dual-Path Pipeline Example - Critical Architecture Demonstration
 * ================================================================
 * 
 * This example shows the CRITICAL dual-path architecture:
 * 
 * PATH 1 (RAW): I2S → DC Offset → Goertzel → Beat Detection
 * PATH 2 (AGC): I2S → DC Offset → Goertzel → Multiband AGC → Zone Mapping
 * 
 * WHY DUAL PATHS?
 * - Beat detection requires dynamic range information (volume changes)
 * - AGC removes dynamic range to normalize visualization
 * - These are mutually exclusive requirements!
 * 
 * SOLUTION: Process the same Goertzel output through two paths
 */

#include "audio/audio_pipeline.h"
#include "audio/nodes/i2s_input_node.h"
#include "audio/nodes/dc_offset_node.h"
#include "audio/nodes/goertzel_node.h"
#include "audio/nodes/multiband_agc_node.h"
#include "audio/nodes/beat_detector_node.h"
#include "audio/nodes/zone_mapper_node.h"
#include <ArduinoJson.h>

// Shared nodes (used by both paths)
I2SInputNode* i2s_input = nullptr;
DCOffsetNode* dc_offset = nullptr;
GoertzelNode* goertzel = nullptr;

// Path 1: RAW for beat detection
BeatDetectorNode* beat_detector = nullptr;

// Path 2: AGC for visualization
MultibandAGCNode* multiband_agc = nullptr;
ZoneMapperNode* zone_mapper = nullptr;

// Buffers for dual-path processing
AudioBuffer audio_buffer_1;
AudioBuffer audio_buffer_2;
AudioBuffer freq_buffer_raw;
AudioBuffer freq_buffer_agc;
AudioBuffer zone_buffer;

// Buffer storage
float audio_data_1[512];
float audio_data_2[512];
float freq_data_raw[96];
float freq_data_agc[96];
float zone_data[256];

// Initialize dual-path architecture
bool initializeDualPathPipeline() {
    Serial.println("\n=== Initializing Dual-Path Pipeline ===");
    Serial.println("PATH 1: RAW for Beat Detection");
    Serial.println("PATH 2: AGC for Visualization\n");
    
    // Initialize buffers
    audio_buffer_1.data = audio_data_1;
    audio_buffer_2.data = audio_data_2;
    freq_buffer_raw.data = freq_data_raw;
    freq_buffer_agc.data = freq_data_agc;
    zone_buffer.data = zone_data;
    
    // Create shared nodes
    i2s_input = new I2SInputNode();
    dc_offset = new DCOffsetNode();
    goertzel = new GoertzelNode();
    
    // Create Path 1 nodes (RAW)
    beat_detector = new BeatDetectorNode();
    
    // Create Path 2 nodes (AGC)
    multiband_agc = new MultibandAGCNode();
    zone_mapper = new ZoneMapperNode();
    
    // Initialize hardware
    if (!i2s_input->init()) {
        Serial.println("ERROR: Failed to initialize I2S input!");
        return false;
    }
    
    Serial.println("✓ All nodes created successfully");
    return true;
}

// Process one frame through dual paths
void processDualPathFrame() {
    // STAGE 1: Common preprocessing
    // I2S Input → DC Offset → Goertzel
    
    // Read from I2S
    if (!i2s_input->process(audio_buffer_1, audio_buffer_1)) {
        return;
    }
    
    // Remove DC offset
    if (!dc_offset->process(audio_buffer_1, audio_buffer_2)) {
        return;
    }
    
    // Convert to frequency domain (Goertzel)
    if (!goertzel->process(audio_buffer_2, freq_buffer_raw)) {
        return;
    }
    
    // CRITICAL: freq_buffer_raw now contains RAW frequency magnitudes
    // This data has the dynamic range needed for beat detection!
    
    // PATH 1: Beat Detection on RAW data
    beat_detector->process(freq_buffer_raw, freq_buffer_raw);
    
    // PATH 2: AGC Processing for visualization
    // First copy RAW data to AGC buffer
    memcpy(freq_data_agc, freq_data_raw, 96 * sizeof(float));
    freq_buffer_agc.size = freq_buffer_raw.size;
    freq_buffer_agc.timestamp = freq_buffer_raw.timestamp;
    freq_buffer_agc.is_silence = freq_buffer_raw.is_silence;
    freq_buffer_agc.metadata = freq_buffer_raw.metadata;
    
    // Apply Multiband AGC
    multiband_agc->process(freq_buffer_agc, freq_buffer_agc);
    
    // Map to zones for LED visualization
    zone_mapper->process(freq_buffer_agc, zone_buffer);
    
    // Now we have:
    // - Beat detection results in beat_detector (from RAW path)
    // - Normalized zone energies in zone_buffer (from AGC path)
}

// Print pipeline status
void printPipelineStatus() {
    Serial.println("\n=== Dual-Path Pipeline Status ===");
    
    // Beat detection status (from RAW path)
    if (beat_detector->isBeatDetected()) {
        Serial.printf("BEAT DETECTED! BPM=%.1f, Confidence=%.2f\n",
                     beat_detector->getCurrentBPM(),
                     beat_detector->getBeatConfidence());
    }
    
    // Zone energy status (from AGC path)
    const float* zones = zone_mapper->getZoneEnergies();
    float avg_energy = 0.0f;
    for (size_t i = 0; i < zone_mapper->getNumZones(); i++) {
        avg_energy += zones[i];
    }
    avg_energy /= zone_mapper->getNumZones();
    
    Serial.printf("Zone Average Energy: %.3f (AGC normalized)\n", avg_energy);
    
    // Show first few zone values
    Serial.print("Zone Values: ");
    for (size_t i = 0; i < 8 && i < zone_mapper->getNumZones(); i++) {
        Serial.printf("%.2f ", zones[i]);
    }
    Serial.println("...");
}

// Configure pipeline from JSON
void configureDualPathPipeline(const char* json_config) {
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, json_config);
    
    if (error) {
        Serial.printf("Failed to parse config: %s\n", error.c_str());
        return;
    }
    
    JsonObject config = doc.as<JsonObject>();
    
    // Configure each node
    if (config.containsKey("i2s_input")) {
        JsonObject node_config = config["i2s_input"];
        i2s_input->configure(node_config);
    }
    
    if (config.containsKey("beat_detector")) {
        JsonObject node_config = config["beat_detector"];
        beat_detector->configure(node_config);
    }
    
    if (config.containsKey("zone_mapper")) {
        JsonObject node_config = config["zone_mapper"];
        zone_mapper->configure(node_config);
    }
}

// Example configuration for EDM music
const char* EDM_CONFIG = R"({
    "i2s_input": {
        "sample_rate": 16000,
        "chunk_size": 128
    },
    "beat_detector": {
        "onset_threshold": 0.25,
        "tempo_range": {
            "min": 120,
            "max": 140
        }
    },
    "zone_mapper": {
        "num_zones": 36,
        "mapping_mode": "logarithmic",
        "smoothing_factor": 0.8,
        "gamma": 1.8
    }
})";

// Main dual-path pipeline demo
void runDualPathPipelineDemo() {
    Serial.println("\n=== DUAL-PATH PIPELINE DEMO ===");
    Serial.println("Demonstrating critical AGC/Beat Detection separation\n");
    
    // Initialize
    if (!initializeDualPathPipeline()) {
        Serial.println("Failed to initialize pipeline!");
        return;
    }
    
    // Configure for EDM
    configureDualPathPipeline(EDM_CONFIG);
    
    // Process some frames
    Serial.println("Processing audio frames...\n");
    
    for (int frame = 0; frame < 1000; frame++) {
        processDualPathFrame();
        
        // Print status every second (125 frames at 8ms per frame)
        if (frame % 125 == 0) {
            printPipelineStatus();
        }
        
        delay(8);  // 8ms per frame = 125 FPS
    }
    
    // Print final metrics
    Serial.println("\n=== Final Metrics ===");
    
    StaticJsonDocument<1024> metrics;
    JsonObject beat_metrics = metrics.createNestedObject("beat_detector");
    beat_detector->getMetrics(beat_metrics);
    
    JsonObject agc_metrics = metrics.createNestedObject("multiband_agc");
    multiband_agc->getMetrics(agc_metrics);
    
    serializeJsonPretty(metrics, Serial);
    
    // Cleanup
    delete i2s_input;
    delete dc_offset;
    delete goertzel;
    delete beat_detector;
    delete multiband_agc;
    delete zone_mapper;
}