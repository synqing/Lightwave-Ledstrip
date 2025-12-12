/*
 * BeatDetectorNode - Pluggable Beat Detection Module
 * ===================================================
 * 
 * Wraps the Enhanced Beat Detector in AudioNode interface.
 * 
 * CRITICAL: This node MUST receive RAW frequency data!
 * DO NOT process beat detection on AGC-normalized data.
 * The AGC removes the dynamic range needed for beat detection.
 * 
 * ARCHITECTURE:
 * - Processes raw Goertzel magnitudes
 * - Detects onsets across multiple frequency bands
 * - Uses PLL for tempo tracking
 * - Includes genre classification
 * 
 * OUTPUT:
 * - Beat events with confidence scores
 * - Current BPM
 * - Genre classification
 * - Predicted next beat timing
 */

#ifndef BEAT_DETECTOR_NODE_H
#define BEAT_DETECTOR_NODE_H

#include "../audio_node.h"
#include "../legacy_beat_detector.h"

class BeatDetectorNode : public AudioNode {
public:
    BeatDetectorNode() : AudioNode("BeatDetector", AudioNodeType::ANALYZER) {
        // Beat detector operates on frequency bins, not time-domain samples
        expects_frequency_data = true;
    }
    
    // Process frequency bins for beat detection
    bool process(AudioBuffer& input, AudioBuffer& output) override {
        if (!enabled || input.size == 0) {
            // Pass through
            memcpy(output.data, input.data, input.size * sizeof(float));
            output.size = input.size;
            output.timestamp = input.timestamp;
            output.is_silence = input.is_silence;
            output.metadata = input.metadata;
            return true;
        }
        
        uint32_t start = micros();
        
        // CRITICAL: Ensure we're receiving RAW frequency data
        if (!input.metadata.is_raw_spectrum) {
            Serial.println("WARNING: BeatDetectorNode requires RAW frequency data!");
            Serial.println("Beat detection will not work on AGC-processed data!");
        }
        
        // Process spectrum through beat detector
        // detector.processSpectrum(input.data, input.size, input.timestamp);
        
        // Pass frequency data through unchanged
        memcpy(output.data, input.data, input.size * sizeof(float));
        output.size = input.size;
        output.timestamp = input.timestamp;
        output.is_silence = input.is_silence;
        output.metadata = input.metadata;
        
        // 1. Calculate total energy and detect transients
        float total_energy = 0;
        for (int i = 0; i < input.size; i++) {
            total_energy += input.data[i] * input.data[i];  // Square for energy
        }
        total_energy = sqrtf(total_energy / input.size);  // RMS energy

        float energy_transient = total_energy - last_energy;
        bool is_transient = energy_transient > (last_energy * 0.5f) && total_energy > 5000.0f; // 50% increase AND minimum energy
        
        // 2. Process with the legacy detector
        detector.process(total_energy, is_transient);
        
        // Update last_energy AFTER processing
        last_energy = total_energy;
        
        // Debug output to understand what's happening
        static int debug_counter = 0;
        if (++debug_counter % 50 == 0) {  // Every 50 frames
            Serial.printf("BEAT DEBUG: energy=%.1f, last=%.1f, trans=%s, threshold=8000.0\n", 
                         total_energy, last_energy, is_transient ? "YES" : "NO");
            
            // Also show some raw bin values
            Serial.printf("  Raw bins[0-4]: %.1f, %.1f, %.1f, %.1f, %.1f\n",
                         input.data[0], input.data[1], input.data[2], input.data[3], input.data[4]);
        }
        
        // Add beat detection results to metadata
        output.metadata.beat_detected = detector.isBeat();
        output.metadata.beat_confidence = detector.getConfidence();
        output.metadata.current_bpm = detector.getBPM();
        
        // Store for external access
        last_beat_detected = detector.isBeat();
        last_bpm = detector.getBPM();
        last_confidence = detector.getConfidence();
        
        // Debug output
        if (detector.isBeat()) {
            Serial.printf("BEAT! BPM=%.1f, Confidence=%.2f\n",
                         detector.getBPM(),
                         detector.getConfidence());
            debug_enabled = true;  // Force debug on to see what's happening
        }
        
        measureProcessTime(start);
        return true;
    }
    
    // Configure from JSON
    bool configure(JsonObject& config) override {
        if (config.containsKey("enabled")) {
            enabled = config["enabled"];
        }
        
        if (config.containsKey("onset_threshold")) {
            // No longer directly configurable in this simplified model
            // detector.setOnsetThreshold(config["onset_threshold"]);
        }
        
        if (config.containsKey("tempo_range")) {
            // This would require adding a setter to LegacyBeatDetector
            // JsonObject range = config["tempo_range"];
            // if (range.containsKey("min") && range.containsKey("max")) {
            //     detector.setTempoRange(range["min"], range["max"]);
            // }
        }
        
        if (config.containsKey("debug")) {
            debug_enabled = config["debug"];
        }
        
        return true;
    }
    
    // Get current configuration
    void getConfig(JsonObject& config) override {
        AudioNode::getConfig(config);
        config["current_bpm"] = detector.getBPM();
        config["beat_confidence"] = detector.getConfidence();
        // These are no longer available
        // config["genre"] = detector.getCurrentGenreName();
        // config["genre_confidence"] = detector.getGenreConfidence();
        // config["total_beats"] = detector.getTotalBeatsDetected();
        // config["average_tempo"] = detector.getAverageTempo();
    }
    
    // Get performance metrics
    void getMetrics(JsonObject& metrics) override {
        AudioNode::getMetrics(metrics);
        // These are no longer available
        // metrics["beats_detected"] = detector.getTotalBeatsDetected();
        // metrics["accuracy"] = detector.getAccuracy();
        metrics["current_bpm"] = detector.getBPM();
        
        // Add genre breakdown
        // JsonObject genre = metrics.createNestedObject("genre");
        // genre["current"] = detector.getCurrentGenreName();
        // genre["confidence"] = detector.getGenreConfidence();
    }
    
    // Direct access for visualization/effects
    bool isBeatDetected() const { return last_beat_detected; }
    float getCurrentBPM() const { return last_bpm; }
    float getBeatConfidence() const { return last_confidence; }
    LegacyBeatDetector& getDetector() { return detector; }
    
private:
    LegacyBeatDetector detector;
    float last_energy = 0.0f;
    bool expects_frequency_data = true;
    bool debug_enabled = false;
    
    // Cache last results for external access
    bool last_beat_detected = false;
    float last_bpm = 120.0f;
    float last_confidence = 0.0f;
};

#endif // BEAT_DETECTOR_NODE_H