/*
 * Test Program for Pluggable Pipeline Architecture
 * ================================================
 * 
 * Tests the complete audio pipeline with simulated audio input
 * Verifies dual-path processing and performance metrics
 */

#include <Arduino.h>
#include "audio/audio_pipeline.h"
#include "audio/audio_node_factory.h"
#include "audio/nodes/i2s_input_node.h"
#include "audio/nodes/dc_offset_node.h"
#include "audio/nodes/goertzel_node.h"
#include "audio/nodes/multiband_agc_node.h"
#include "audio/nodes/beat_detector_node.h"
#include "audio/nodes/zone_mapper_node.h"
#include "audio/audio_state.h"
#include <esp_timer.h>

// Global state (for compatibility)
AudioState audio_state;

// Test buffers
float test_audio_buffer[128];
float test_freq_buffer[96];

// Pipelines
AudioPipeline* main_pipeline = nullptr;
AudioPipeline* beat_pipeline = nullptr;

// Generate test signal with multiple frequency components
void generateTestSignal(float* buffer, int size, float time_sec) {
    // Mix of frequencies: 100Hz (bass), 500Hz (mid), 2000Hz (high)
    for (int i = 0; i < size; i++) {
        float t = time_sec + (float)i / 16000.0f;  // 16kHz sample rate
        
        // Bass component with envelope
        float bass_env = 0.5f + 0.5f * sinf(2.0f * M_PI * 2.0f * t);  // 2Hz envelope
        float bass = bass_env * sinf(2.0f * M_PI * 100.0f * t);
        
        // Mid component
        float mid = 0.3f * sinf(2.0f * M_PI * 500.0f * t);
        
        // High component with tremolo
        float high_env = 0.5f + 0.5f * sinf(2.0f * M_PI * 8.0f * t);  // 8Hz tremolo
        float high = high_env * 0.2f * sinf(2.0f * M_PI * 2000.0f * t);
        
        // Mix and scale to int16 range
        buffer[i] = (bass + mid + high) * 8192.0f;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== PLUGGABLE PIPELINE TEST ===");
    Serial.println("Testing dual-path architecture with simulated audio\n");
    
    // Create main pipeline
    main_pipeline = new AudioPipeline("Main");
    
    // Build main pipeline manually (could use factory)
    main_pipeline->addNode(std::make_shared<DCOffsetNode>());
    main_pipeline->addNode(std::make_shared<GoertzelNode>());
    main_pipeline->addNode(std::make_shared<MultibandAGCNode>());
    main_pipeline->addNode(std::make_shared<ZoneMapperNode>());
    
    // Create beat detection pipeline
    beat_pipeline = new AudioPipeline("Beat");
    beat_pipeline->addNode(std::make_shared<BeatDetectorNode>());
    
    // Configure nodes
    StaticJsonDocument<512> config;
    JsonObject dc_config = config.to<JsonObject>();
    dc_config["mode"] = "fixed";
    dc_config["fixed_offset"] = 0.0f;  // No DC offset in test signal
    main_pipeline->findNode("DCOffset")->configure(dc_config);
    
    JsonObject zone_config = config.to<JsonObject>();
    zone_config["num_zones"] = 8;
    zone_config["mapping_mode"] = "logarithmic";
    main_pipeline->findNode("ZoneMapper")->configure(zone_config);
    
    Serial.println("Pipeline configuration complete");
    main_pipeline->printStructure();
    beat_pipeline->printStructure();
}

void loop() {
    static float time_sec = 0.0f;
    static uint32_t frame_count = 0;
    static uint32_t last_report_time = 0;
    static uint64_t total_process_time = 0;
    static uint64_t max_process_time = 0;
    
    // Generate test signal
    generateTestSignal(test_audio_buffer, 128, time_sec);
    time_sec += 128.0f / 16000.0f;  // Advance time
    
    // Measure processing time
    uint64_t start_time = esp_timer_get_time();
    
    // Process main pipeline
    PipelineError error = main_pipeline->process(test_audio_buffer, 128);
    
    if (error == PipelineError::NONE) {
        // Get Goertzel output for beat detection
        AudioBuffer* goertzel_output = main_pipeline->getNodeOutput("Goertzel");
        
        if (goertzel_output && goertzel_output->metadata.is_raw_spectrum) {
            // Process beat detection on RAW data
            beat_pipeline->process(goertzel_output->data, goertzel_output->size);
            
            // Get zone mapper output
            AudioBuffer* zone_output = main_pipeline->getNodeOutput("ZoneMapper");
            
            if (zone_output) {
                // Update global audio state
                memcpy(audio_state.core.zone_energies, zone_output->data, 
                       zone_output->size * sizeof(float));
                
                // Get beat detection results
                BeatDetectorNode* beat_node = 
                    static_cast<BeatDetectorNode*>(beat_pipeline->findNode("BeatDetector").get());
                
                if (beat_node->isBeatDetected()) {
                    Serial.printf("BEAT! BPM=%.1f, Confidence=%.2f\n",
                                 beat_node->getCurrentBPM(),
                                 beat_node->getBeatConfidence());
                }
            }
        }
    } else {
        Serial.printf("Pipeline error: %d\n", (int)error);
    }
    
    // Measure processing time
    uint64_t process_time = esp_timer_get_time() - start_time;
    total_process_time += process_time;
    if (process_time > max_process_time) {
        max_process_time = process_time;
    }
    
    frame_count++;
    
    // Report metrics every second
    if (millis() - last_report_time >= 1000) {
        // Calculate averages
        float avg_process_time_ms = (float)total_process_time / frame_count / 1000.0f;
        float max_process_time_ms = (float)max_process_time / 1000.0f;
        
        Serial.println("\n=== PERFORMANCE METRICS ===");
        Serial.printf("Frames processed: %d\n", frame_count);
        Serial.printf("Average process time: %.3f ms\n", avg_process_time_ms);
        Serial.printf("Max process time: %.3f ms\n", max_process_time_ms);
        Serial.printf("Frame rate: %.1f FPS\n", (float)frame_count);
        
        // Check if we meet the <8ms target
        if (avg_process_time_ms < 8.0f) {
            Serial.println("✓ Performance target MET (<8ms)");
        } else {
            Serial.println("✗ Performance target MISSED (>8ms)");
        }
        
        // Print zone energies
        Serial.print("Zone energies: ");
        for (int i = 0; i < 8; i++) {
            Serial.printf("%.2f ", audio_state.core.zone_energies[i]);
        }
        Serial.println();
        
        // Print pipeline health
        const PipelineHealth& health = main_pipeline->getHealth();
        Serial.printf("Pipeline health: %s, failures: %d\n",
                     health.is_healthy ? "HEALTHY" : "UNHEALTHY",
                     health.total_failures);
        
        // Get detailed metrics
        StaticJsonDocument<1024> metrics;
        JsonObject pipeline_metrics = metrics.to<JsonObject>();
        main_pipeline->getMetrics(pipeline_metrics);
        
        Serial.println("\nNode timings:");
        JsonArray nodes = pipeline_metrics["nodes"];
        for (JsonObject node : nodes) {
            const char* name = node["name"];
            uint32_t time_us = node["process_time_us"];
            Serial.printf("  %s: %d us\n", name, time_us);
        }
        
        // Reset counters
        frame_count = 0;
        total_process_time = 0;
        max_process_time = 0;
        last_report_time = millis();
    }
    
    // Simulate 125 FPS (8ms per frame)
    delay(8);
}

// Test with node factory
void testNodeFactory() {
    Serial.println("\n=== TESTING NODE FACTORY ===");
    
    const char* pipeline_config = R"({
        "name": "Factory Test Pipeline",
        "nodes": [
            {
                "type": "DCOffsetNode",
                "mode": "fixed",
                "fixed_offset": 0.0
            },
            {
                "type": "GoertzelNode",
                "enabled": true
            },
            {
                "type": "MultibandAGCNode",
                "enabled": true
            },
            {
                "type": "ZoneMapperNode",
                "num_zones": 8,
                "mapping_mode": "logarithmic"
            }
        ]
    })";
    
    AudioPipeline test_pipeline("Factory Test");
    if (AudioNodeFactory::loadPipelineFromJSON(test_pipeline, pipeline_config)) {
        Serial.println("✓ Pipeline created from JSON successfully!");
        test_pipeline.printStructure();
    } else {
        Serial.println("✗ Failed to create pipeline from JSON");
    }
}