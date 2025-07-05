/*
 * Dual-Path Architecture Verification
 * ===================================
 * 
 * Verifies that the RAW and AGC paths are properly separated
 * and that beat detection works correctly on RAW data only
 */

#include <Arduino.h>
#include "audio/audio_pipeline.h"
#include "audio/audio_node_factory.h"
#include "audio/nodes/all_nodes.h"  // Include all node headers

// Test signal parameters
const float BEAT_FREQUENCY = 2.0f;  // 120 BPM
const float SIGNAL_FREQUENCY = 100.0f;  // Bass frequency

// Generate signal with dynamic envelope
void generateBeatSignal(float* buffer, int size, float time_sec) {
    // Create a signal with clear beats
    float beat_phase = fmodf(time_sec * BEAT_FREQUENCY, 1.0f);
    
    // Sharp attack, exponential decay
    float envelope;
    if (beat_phase < 0.05f) {
        // Attack phase (5% of beat period)
        envelope = beat_phase / 0.05f;
    } else {
        // Decay phase
        envelope = expf(-(beat_phase - 0.05f) * 5.0f);
    }
    
    // Generate signal with envelope
    for (int i = 0; i < size; i++) {
        float t = time_sec + (float)i / 16000.0f;
        float signal = envelope * sinf(2.0f * M_PI * SIGNAL_FREQUENCY * t);
        buffer[i] = signal * 16384.0f;  // Scale to int16 range
    }
}

void verifyDualPath() {
    Serial.println("\n=== DUAL-PATH VERIFICATION TEST ===");
    Serial.println("Testing RAW vs AGC path separation\n");
    
    // Create pipelines
    AudioPipeline main_pipeline("Main");
    AudioPipeline beat_pipeline("Beat");
    
    // Build main pipeline
    main_pipeline.addNode(std::make_shared<DCOffsetNode>());
    main_pipeline.addNode(std::make_shared<GoertzelNode>());
    main_pipeline.addNode(std::make_shared<MultibandAGCNode>());
    main_pipeline.addNode(std::make_shared<ZoneMapperNode>());
    
    // Build beat pipeline
    beat_pipeline.addNode(std::make_shared<BeatDetectorNode>());
    
    // Configure nodes
    StaticJsonDocument<512> config;
    JsonObject dc_config = config.to<JsonObject>();
    dc_config["mode"] = "fixed";
    dc_config["fixed_offset"] = 0.0f;
    main_pipeline.findNode("DCOffset")->configure(dc_config);
    
    // Test buffers
    float audio_buffer[128];
    float raw_magnitudes[96];
    float agc_magnitudes[96];
    
    // Process several seconds of audio
    Serial.println("Processing beat signal...");
    Serial.println("Time  | RAW Energy | AGC Energy | Beat? | Confidence");
    Serial.println("------|------------|------------|-------|------------");
    
    float time_sec = 0.0f;
    int beat_count = 0;
    int expected_beats = 0;
    
    for (int frame = 0; frame < 250; frame++) {  // 2 seconds
        // Generate test signal
        generateBeatSignal(audio_buffer, 128, time_sec);
        
        // Process main pipeline
        PipelineError error = main_pipeline.process(audio_buffer, 128);
        
        if (error == PipelineError::NONE) {
            // Get RAW Goertzel output
            AudioBuffer* raw_output = main_pipeline.getNodeOutput("Goertzel");
            if (raw_output) {
                memcpy(raw_magnitudes, raw_output->data, 96 * sizeof(float));
            }
            
            // Get AGC output
            AudioBuffer* agc_output = main_pipeline.getNodeOutput("MultibandAGC");
            if (agc_output) {
                memcpy(agc_magnitudes, agc_output->data, 96 * sizeof(float));
            }
            
            // Calculate total energies
            float raw_energy = 0.0f;
            float agc_energy = 0.0f;
            for (int i = 0; i < 96; i++) {
                raw_energy += raw_magnitudes[i];
                agc_energy += agc_magnitudes[i];
            }
            raw_energy /= 96.0f;
            agc_energy /= 96.0f;
            
            // Process beat detection on RAW data
            beat_pipeline.process(raw_magnitudes, 96);
            
            // Get beat detection results
            BeatDetectorNode* beat_node = 
                static_cast<BeatDetectorNode*>(beat_pipeline.findNode("BeatDetector").get());
            
            bool beat_detected = beat_node->isBeatDetected();
            float confidence = beat_node->getBeatConfidence();
            
            if (beat_detected) {
                beat_count++;
            }
            
            // Check if we expect a beat (at the start of each beat period)
            float beat_phase = fmodf(time_sec * BEAT_FREQUENCY, 1.0f);
            float prev_phase = fmodf((time_sec - 0.008f) * BEAT_FREQUENCY, 1.0f);
            if (prev_phase > beat_phase) {  // Phase wrapped around
                expected_beats++;
            }
            
            // Print status every 10 frames
            if (frame % 10 == 0) {
                Serial.printf("%5.2f | %10.1f | %10.3f | %5s | %10.2f\n",
                             time_sec,
                             raw_energy,
                             agc_energy,
                             beat_detected ? "BEAT" : "-",
                             confidence);
            }
        }
        
        time_sec += 0.008f;  // 8ms per frame
    }
    
    // Print summary
    Serial.println("\n=== DUAL-PATH VERIFICATION RESULTS ===");
    
    // Verify RAW path preserves dynamics
    Serial.println("\n1. Dynamic Range Preservation:");
    Serial.println("   RAW path should show large variations");
    Serial.println("   AGC path should be normalized");
    Serial.println("   ✓ Visual inspection required from output above");
    
    // Verify beat detection accuracy
    Serial.println("\n2. Beat Detection Accuracy:");
    Serial.printf("   Expected beats: ~%d\n", expected_beats);
    Serial.printf("   Detected beats: %d\n", beat_count);
    
    float accuracy = (float)beat_count / expected_beats;
    if (accuracy > 0.8f && accuracy < 1.2f) {
        Serial.println("   ✓ Beat detection working correctly");
    } else {
        Serial.println("   ✗ Beat detection needs tuning");
    }
    
    // Test AGC beat detection (should fail)
    Serial.println("\n3. AGC Beat Detection Test:");
    Serial.println("   Testing beat detection on AGC data (should fail)...");
    
    beat_count = 0;
    time_sec = 0.0f;
    
    for (int frame = 0; frame < 250; frame++) {
        generateBeatSignal(audio_buffer, 128, time_sec);
        main_pipeline.process(audio_buffer, 128);
        
        // Get AGC output and try beat detection
        AudioBuffer* agc_output = main_pipeline.getNodeOutput("MultibandAGC");
        if (agc_output) {
            beat_pipeline.process(agc_output->data, agc_output->size);
            BeatDetectorNode* beat_node = 
                static_cast<BeatDetectorNode*>(beat_pipeline.findNode("BeatDetector").get());
            if (beat_node->isBeatDetected()) {
                beat_count++;
            }
        }
        
        time_sec += 0.008f;
    }
    
    Serial.printf("   Beats detected on AGC data: %d\n", beat_count);
    if (beat_count < expected_beats / 2) {
        Serial.println("   ✓ Correctly fails on AGC data (dynamics removed)");
    } else {
        Serial.println("   ✗ Incorrectly detecting beats on AGC data");
    }
    
    Serial.println("\n=== DUAL-PATH VERIFICATION COMPLETE ===");
}

// Test metadata flow
void testMetadataFlow() {
    Serial.println("\n=== METADATA FLOW TEST ===");
    
    AudioPipeline pipeline("Metadata Test");
    
    // Add nodes
    pipeline.addNode(std::make_shared<GoertzelNode>());
    pipeline.addNode(std::make_shared<MultibandAGCNode>());
    
    // Process dummy data
    float dummy[128] = {0};
    pipeline.process(dummy, 128);
    
    // Check metadata at each stage
    AudioBuffer* goertzel_out = pipeline.getNodeOutput("Goertzel");
    AudioBuffer* agc_out = pipeline.getNodeOutput("MultibandAGC");
    
    Serial.println("Metadata propagation:");
    Serial.printf("Goertzel output: is_raw_spectrum=%d, is_agc_processed=%d\n",
                  goertzel_out->metadata.is_raw_spectrum,
                  goertzel_out->metadata.is_agc_processed);
    
    Serial.printf("AGC output: is_raw_spectrum=%d, is_agc_processed=%d\n",
                  agc_out->metadata.is_raw_spectrum,
                  agc_out->metadata.is_agc_processed);
    
    if (goertzel_out->metadata.is_raw_spectrum && 
        !goertzel_out->metadata.is_agc_processed &&
        !agc_out->metadata.is_raw_spectrum && 
        agc_out->metadata.is_agc_processed) {
        Serial.println("✓ Metadata flow correct");
    } else {
        Serial.println("✗ Metadata flow incorrect");
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n==========================================");
    Serial.println("    DUAL-PATH ARCHITECTURE VERIFICATION");
    Serial.println("==========================================");
    
    // Run verification tests
    verifyDualPath();
    delay(1000);
    
    testMetadataFlow();
    
    Serial.println("\n=== ALL TESTS COMPLETE ===");
}

void loop() {
    delay(1000);
}