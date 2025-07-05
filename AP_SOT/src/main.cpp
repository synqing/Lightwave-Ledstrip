/*
 * Main - Pluggable Pipeline Architecture
 * ======================================
 * 
 * This is the new main.cpp using the pluggable pipeline architecture.
 * It maintains backward compatibility with the audio_state global.
 * 
 * ARCHITECTURE:
 * - Dual-path processing (RAW for beat, AGC for visuals)
 * - Configurable pipeline via JSON
 * - Real-time metrics and monitoring
 * 
 * To use this instead of main.cpp:
 * 1. Rename main.cpp to main_legacy.cpp
 * 2. Rename this file to main.cpp
 */

#include <Arduino.h>
#include <math.h>
#include "audio/audio_state.h"
#include "audio/audio_pipeline.h"
#include "audio/nodes/i2s_input_node.h"
#include "audio/nodes/dc_offset_node.h"
#include "audio/nodes/goertzel_node.h"
#include "audio/nodes/multiband_agc_node.h"
#include "audio/nodes/beat_detector_node.h"
#include "audio/nodes/zone_mapper_node.h"
#include "audio/nodes/vog_node.h"

// Global audio state (required for backward compatibility)
AudioState audio_state = {0};

// Pipeline components
AudioPipeline* main_pipeline = nullptr;
AudioPipeline* beat_pipeline = nullptr;

// Node references for direct access
I2SInputNode* i2s_node = nullptr;
GoertzelNode* goertzel_node = nullptr;
BeatDetectorNode* beat_node = nullptr;
MultibandAGCNode* agc_node = nullptr;
ZoneMapperNode* zone_node = nullptr;
VoGNode* vog_node = nullptr;

// Audio buffers
float pipeline_buffer_1[512];
float pipeline_buffer_2[512];
float beat_buffer[96];

// Initialize the pluggable pipeline
bool initializePipeline() {
    Serial.println("\n=== Initializing Pluggable Pipeline ===");
    
    // Create main visualization pipeline
    main_pipeline = new AudioPipeline("SpectraSynq_Main");
    
    // Create nodes
    i2s_node = new I2SInputNode();
    auto dc_offset = new DCOffsetNode();
    goertzel_node = new GoertzelNode();
    agc_node = new MultibandAGCNode();
    zone_node = new ZoneMapperNode();
    
    // Initialize I2S hardware
    if (!i2s_node->init()) {
        Serial.println("ERROR: Failed to initialize I2S!");
        return false;
    }
    
    // Build main pipeline: I2S → DC → Goertzel → AGC → Zones
    main_pipeline->addNode(i2s_node);
    main_pipeline->addNode(dc_offset);
    main_pipeline->addNode(goertzel_node);
    main_pipeline->addNode(agc_node);
    main_pipeline->addNode(zone_node);
    
    // Create separate beat detection pipeline (shares Goertzel output)
    beat_pipeline = new AudioPipeline("SpectraSynq_Beat");
    beat_node = new BeatDetectorNode();
    beat_pipeline->addNode(beat_node);
    
    // Configure zone mapper for 36 zones (standard Emotiscope)
    StaticJsonDocument<256> zone_config;
    zone_config["num_zones"] = 36;
    zone_config["mapping_mode"] = "logarithmic";
    zone_config["smoothing_factor"] = 0.8;
    zone_config["gamma"] = 1.5;
    zone_config["debug"] = false;  // Disable debug output
    JsonObject zone_obj = zone_config.as<JsonObject>();
    zone_node->configure(zone_obj);
    
    // Create Voice of God confidence engine
    vog_node = new VoGNode();
    Serial.println("Voice of God (VoG) confidence engine initialized");
    
    // Print pipeline structure
    Serial.println("\nMain Pipeline Structure:");
    main_pipeline->printStructure();
    
    Serial.println("\nBeat Pipeline Structure:");
    beat_pipeline->printStructure();
    
    Serial.println("\nVoG Engine: Runs asynchronously at 10-12Hz");
    
    return true;
}

// Update global audio_state from pipeline results
void updateAudioState() {
    // Get zone energies from zone mapper (already normalized 0-1)
    const float* zones = zone_node->getZoneEnergies();
    size_t num_zones = zone_node->getNumZones();
    
    // Map to legacy 8-zone format (aggregate 36 zones to 8)
    int zones_per_legacy = num_zones / 8;
    
    // Reset global energy
    audio_state.core.global_energy = 0.0f;
    
    for (int i = 0; i < 8; i++) {
        float sum = 0.0f;
        int start = i * zones_per_legacy;
        int end = (i + 1) * zones_per_legacy;
        if (i == 7) end = num_zones; // Handle remainder
        
        for (int j = start; j < end; j++) {
            sum += zones[j];
        }
        // Zone mapper already outputs normalized values (0-1) with gamma applied
        audio_state.core.zone_energies[i] = sum / (end - start);
        audio_state.core.zone_energies[i] = constrain(audio_state.core.zone_energies[i], 0.0f, 1.0f);
        audio_state.core.global_energy += audio_state.core.zone_energies[i];
    }
    audio_state.core.global_energy /= 8.0f;
    
    // Update beat detection state
    if (beat_node->isBeatDetected()) {
        // When beat is detected, show immediate confidence spike
        float calculated_confidence = beat_node->getBeatConfidence();
        // If confidence is 0 (not enough history), use 1.0 for immediate feedback
        audio_state.ext.beat.beat_confidence = (calculated_confidence > 0.0f) ? calculated_confidence : 1.0f;
        audio_state.ext.beat.tempo_bpm = beat_node->getCurrentBPM();
        audio_state.ext.beat.last_beat_ms = millis();
        audio_state.ext.beat.beat_band = 0; // TODO: Get from beat detector
    } else {
        // Decay confidence
        audio_state.ext.beat.beat_confidence *= 0.95f;
    }
    
    // Update beat phase based on tempo
    if (audio_state.ext.beat.tempo_bpm > 0) {
        float beat_period_ms = 60000.0f / audio_state.ext.beat.tempo_bpm;
        float ms_since_beat = millis() - audio_state.ext.beat.last_beat_ms;
        audio_state.ext.beat.beat_phase = fmodf(ms_since_beat / beat_period_ms, 1.0f);
    }
    
    // Update timestamps
    audio_state.last_update_ms = millis();
    audio_state.update_counter++;
    
    // Debug zone normalization
    static int zone_debug_counter = 0;
    if (++zone_debug_counter % 250 == 0) {  // Every 2 seconds at 125Hz
        // Calculate max zone for debug display
        float debug_max_zone = 0.0f;
        for (int i = 0; i < 8; i++) {
            if (audio_state.core.zone_energies[i] > debug_max_zone) {
                debug_max_zone = audio_state.core.zone_energies[i];
            }
        }
        Serial.printf("ZONE DEBUG: max=%.2f | ", debug_max_zone);
        Serial.printf("Z[0-3]=%.2f,%.2f,%.2f,%.2f | Z[4-7]=%.2f,%.2f,%.2f,%.2f\n",
                     audio_state.core.zone_energies[0], audio_state.core.zone_energies[1], 
                     audio_state.core.zone_energies[2], audio_state.core.zone_energies[3],
                     audio_state.core.zone_energies[4], audio_state.core.zone_energies[5], 
                     audio_state.core.zone_energies[6], audio_state.core.zone_energies[7]);
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n\n=== AP_SOT: Pluggable Audio Pipeline ===");
    Serial.println("Architecture: Dual-path AGC/Beat separation");
    Serial.println("Starting initialization...");
    Serial.flush();
    
    // Initialize the pluggable pipeline
    if (!initializePipeline()) {
        Serial.println("FATAL: Pipeline initialization failed!");
        while (1) { delay(100); }
    }
    
    Serial.println("\n✓ Pipeline initialization complete!");
    Serial.println("Starting audio processing loop...\n");
}

void loop() {
    static uint32_t frame_count = 0;
    static uint32_t last_metrics_time = 0;
    static uint32_t last_vog_time = 0;
    
    // Process main pipeline (includes AGC and zone mapping)
    // The I2S input node will generate its own data, so we pass a dummy buffer
    PipelineError error = main_pipeline->process(pipeline_buffer_1, 128);
    
    if (error == PipelineError::NONE) {
        // Get Goertzel output for beat detection (RAW data)
        AudioBuffer* goertzel_output = main_pipeline->getNodeOutput("Goertzel");
        
        if (goertzel_output && goertzel_output->metadata.is_raw_spectrum) {
            // Debug Goertzel output values
            static int goertzel_debug_counter = 0;
            if (++goertzel_debug_counter % 125 == 0) {  // Once per second
                float min_val = 999999.0f, max_val = 0.0f, avg_val = 0.0f;
                for (int i = 0; i < 96; i++) {
                    if (goertzel_output->data[i] < min_val) min_val = goertzel_output->data[i];
                    if (goertzel_output->data[i] > max_val) max_val = goertzel_output->data[i];
                    avg_val += goertzel_output->data[i];
                }
                avg_val /= 96.0f;
                Serial.printf("GOERTZEL RAW: min=%.1f, max=%.1f, avg=%.1f\n", min_val, max_val, avg_val);
            }
            
            // Process beat detection on RAW frequency data
            memcpy(beat_buffer, goertzel_output->data, 96 * sizeof(float));
            AudioBuffer beat_input = { 
                beat_buffer, 
                96, 
                goertzel_output->timestamp, 
                goertzel_output->is_silence 
            };
            beat_input.metadata = goertzel_output->metadata;
            
            // Create output buffer for beat detection
            AudioBuffer beat_output = { 
                beat_buffer,  // Can reuse same buffer since beat detector passes through
                96, 
                goertzel_output->timestamp, 
                goertzel_output->is_silence 
            };
            
            // Process beat detection with proper AudioBuffer structures
            beat_node->process(beat_input, beat_output);
        }
        
        // Update global audio state
        updateAudioState();
        
        // Process VoG confidence engine at ~12Hz
        if (millis() - last_vog_time > 83) {  // 83ms = ~12Hz
            // Get raw and AGC spectrum pointers
            AudioBuffer* raw_spectrum = main_pipeline->getNodeOutput("Goertzel");
            AudioBuffer* agc_spectrum = main_pipeline->getNodeOutput("MultibandAGC");
            
            if (raw_spectrum && agc_spectrum) {
                // Set spectrum pointers for VoG
                vog_node->setSpectrumPointers(raw_spectrum, agc_spectrum);
                
                // Process VoG (it updates audio_state internally)
                float dummy_buffer[1];
                AudioBuffer dummy_in = { dummy_buffer, 1, millis(), false };
                AudioBuffer dummy_out = { dummy_buffer, 1, millis(), false };
                vog_node->process(dummy_in, dummy_out);
            }
            
            last_vog_time = millis();
        }
        
        frame_count++;
    } else {
        // Handle pipeline error
        Serial.printf("Pipeline error: %d\n", (int)error);
        
        // Check health status
        const PipelineHealth& health = main_pipeline->getHealth();
        if (!health.is_healthy) {
            Serial.printf("Pipeline unhealthy! Failures: %d consecutive, %d total\n",
                         health.consecutive_failures, health.total_failures);
            
            // Could trigger emergency recovery here
            if (health.consecutive_failures > 10) {
                Serial.println("CRITICAL: Too many failures, resetting pipeline...");
                main_pipeline->resetHealth();
                // Could reinitialize nodes here
            }
        }
    }
    
    // Print metrics at 10Hz
    if (millis() - last_metrics_time > 100) {
        last_metrics_time = millis();
        
        // Legacy format output for compatibility (averaging band groups)
        Serial.printf("Energy: %.1f | Bass: %.1f | Mid: %.1f | High: %.1f | Beat: %.1f (%.0f BPM) | VoG: %.2f\n",
                     audio_state.core.global_energy,
                     (audio_state.core.zone_energies[0] + audio_state.core.zone_energies[1]) / 2.0f,
                     (audio_state.core.zone_energies[2] + audio_state.core.zone_energies[3] + 
                      audio_state.core.zone_energies[4] + audio_state.core.zone_energies[5]) / 4.0f,
                     (audio_state.core.zone_energies[6] + audio_state.core.zone_energies[7]) / 2.0f,
                     audio_state.ext.beat.beat_confidence,
                     audio_state.ext.beat.tempo_bpm,
                     audio_state.ext.beat.vog_confidence);
        
        // Extended metrics every 5 seconds
        static int metric_counter = 0;
        if (++metric_counter % 50 == 0) {
            Serial.println("\n=== Pipeline Metrics ===");
            
            // Get pipeline performance metrics
            StaticJsonDocument<1024> metrics;
            JsonObject root = metrics.to<JsonObject>();
            main_pipeline->getMetrics(root);
            
            Serial.printf("Frames processed: %lu\n", frame_count);
            Serial.printf("Pipeline latency: %.2f ms\n", 
                         root["total_latency_ms"].as<float>());
            
            // Node-specific metrics
            JsonArray nodes = root["nodes"];
            for (JsonObject node : nodes) {
                Serial.printf("  %s: %.1f µs\n", 
                             node["name"].as<const char*>(),
                             node["process_time_us"].as<float>());
            }
            
            // Beat detector specific info
            Serial.printf("\nBeat Detector:\n");
            Serial.printf("  BPM: %.1f (confidence: %.2f)\n",
                         beat_node->getDetector().getBPM(),
                         beat_node->getDetector().getConfidence());
            // Genre and total beats no longer available in legacy detector
            // Serial.printf("  Genre: %s (confidence: %.2f)\n",
            //              beat_node->getDetector().getCurrentGenreName(),
            //              beat_node->getDetector().getGenreConfidence());
            // Serial.printf("  Total beats: %lu\n", 
            //              beat_node->getDetector().getTotalBeatsDetected());
            
            Serial.println("=======================\n");
        }
    }
}