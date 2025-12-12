/*
 * Pipeline Example - Demonstrates Pluggable Architecture
 * ======================================================
 * 
 * Shows how to create and configure an audio processing pipeline
 * using the new modular node system.
 * 
 * This example creates the standard SpectraSynq pipeline:
 * I2S → DC Offset → Goertzel → Multiband AGC → Output
 */

#include "audio/audio_pipeline.h"
#include "audio/nodes/i2s_input_node.h"
#include "audio/nodes/dc_offset_node.h"
#include "audio/nodes/goertzel_node.h"
#include "audio/nodes/multiband_agc_node.h"
#include <ArduinoJson.h>

// Create the audio processing pipeline
AudioPipeline* createStandardPipeline() {
    auto pipeline = new AudioPipeline("SpectraSynq_Standard");
    
    // Create nodes
    auto i2s_input = new I2SInputNode();
    auto dc_offset = new DCOffsetNode();
    auto goertzel = new GoertzelNode();
    auto multiband_agc = new MultibandAGCNode();
    
    // Initialize hardware nodes
    i2s_input->init();
    
    // Add nodes to pipeline in order
    pipeline->addNode(i2s_input);
    pipeline->addNode(dc_offset);
    pipeline->addNode(goertzel);
    pipeline->addNode(multiband_agc);
    
    // Print pipeline structure
    pipeline->printStructure();
    
    return pipeline;
}

// Configure pipeline from JSON
void configurePipelineFromJSON(AudioPipeline* pipeline, const char* json_config) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, json_config);
    
    if (error) {
        Serial.printf("Failed to parse JSON config: %s\n", error.c_str());
        return;
    }
    
    JsonObject root = doc.as<JsonObject>();
    pipeline->configure(root);
}

// Example JSON configuration
const char* EXAMPLE_CONFIG = R"({
    "name": "SpectraSynq_EDM",
    "nodes": [
        {
            "name": "I2SInput",
            "enabled": true,
            "sample_rate": 16000,
            "chunk_size": 128
        },
        {
            "name": "DCOffset",
            "enabled": true,
            "mode": 2,
            "high_pass_alpha": 0.999
        },
        {
            "name": "Goertzel",
            "enabled": true,
            "debug": false
        },
        {
            "name": "MultibandAGC",
            "enabled": true,
            "a_weighting": false
        }
    ]
})";

// Process audio through pipeline
void processAudioPipeline(AudioPipeline* pipeline) {
    // Input buffer (normally comes from I2S)
    float input_buffer[128];
    
    // Process through pipeline
    bool success = pipeline->process(input_buffer, 128);
    
    if (!success) {
        Serial.println("Pipeline processing failed!");
    }
}

// Get pipeline metrics
void printPipelineMetrics(AudioPipeline* pipeline) {
    StaticJsonDocument<1024> metrics_doc;
    JsonObject metrics = metrics_doc.to<JsonObject>();
    
    pipeline->getMetrics(metrics);
    
    Serial.println("\n=== Pipeline Metrics ===");
    serializeJsonPretty(metrics, Serial);
    Serial.println("\n=======================");
}

// Example of runtime reconfiguration
void reconfigurePipeline(AudioPipeline* pipeline) {
    // Disable DC offset for testing
    AudioNodePtr dc_node = pipeline->findNode("DCOffset");
    if (dc_node) {
        dc_node->setEnabled(false);
        Serial.println("DC Offset disabled");
    }
    
    // Enable A-weighting in AGC
    AudioNodePtr agc_node = pipeline->findNode("MultibandAGC");
    if (agc_node) {
        StaticJsonDocument<128> config;
        config["a_weighting"] = true;
        JsonObject obj = config.as<JsonObject>();
        agc_node->configure(obj);
        Serial.println("A-weighting enabled");
    }
}

// Main example usage
void setupPluggablePipeline() {
    Serial.println("\n=== Pluggable Pipeline Example ===\n");
    
    // Create standard pipeline
    AudioPipeline* pipeline = createStandardPipeline();
    
    // Configure from JSON
    configurePipelineFromJSON(pipeline, EXAMPLE_CONFIG);
    
    // Process some audio
    for (int i = 0; i < 10; i++) {
        processAudioPipeline(pipeline);
        delay(8);  // 8ms per frame (125 FPS)
    }
    
    // Print metrics
    printPipelineMetrics(pipeline);
    
    // Reconfigure at runtime
    reconfigurePipeline(pipeline);
    
    // Process more audio with new config
    for (int i = 0; i < 10; i++) {
        processAudioPipeline(pipeline);
        delay(8);
    }
    
    // Clean up
    delete pipeline;
}