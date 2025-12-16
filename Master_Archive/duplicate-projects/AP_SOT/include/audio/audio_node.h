/*
 * AudioNode - Base Interface for Pluggable Audio Pipeline
 * ========================================================
 * 
 * This is the foundation of the SpectraSynq pluggable architecture.
 * All audio processing modules inherit from this interface.
 * 
 * DESIGN PRINCIPLES:
 * - Single Responsibility: Each node does ONE thing well
 * - Zero Copy: Nodes operate on shared buffers when possible
 * - Real-Time Safe: No dynamic allocation, no blocking
 * - Configuration: JSON-based for easy runtime changes
 * 
 * SIGNAL FLOW:
 * AudioNode → AudioNode → AudioNode → Output
 *     ↓           ↓           ↓
 *   Config     Config     Config
 */

#ifndef AUDIO_NODE_H
#define AUDIO_NODE_H

#include <Arduino.h>
#include <ArduinoJson.h>

// Node type identifiers for factory creation
enum class AudioNodeType {
    SOURCE,           // Input nodes (I2S, ADC, etc)
    ANALYZER,         // Analysis nodes (Goertzel, FFT, etc)
    PROCESSOR,        // Processing nodes (AGC, filters, etc)
    DETECTOR,         // Detection nodes (beat, onset, etc)
    SINK              // Output nodes (features, metrics, etc) - renamed from OUTPUT to avoid Arduino macro conflict
};

// Audio buffer structure for passing data between nodes
struct AudioBuffer {
    float* data;          // Pointer to audio data
    size_t size;          // Number of samples
    uint32_t timestamp;   // Timestamp in milliseconds
    bool is_silence;      // Silence flag for optimization
    
    // Metadata that flows through pipeline
    struct {
        float sample_rate;
        float dc_offset;
        float rms_level;
        
        // Frequency domain flags
        bool is_raw_spectrum;     // True if data is raw frequency bins
        bool is_agc_processed;    // True if data has been AGC processed
        
        // Beat detection results
        bool beat_detected;
        float beat_confidence;
        float current_bpm;
        float predicted_next_beat;
        int genre;
        float genre_confidence;
        
        // Zone mapping info
        size_t zone_count;
    } metadata;
};

// Base class for all audio pipeline nodes
class AudioNode {
    friend class AudioPipeline;  // Allow pipeline to access protected members
public:
    // Constructor/Destructor
    AudioNode(const char* name, AudioNodeType type) : 
        node_name(name), node_type(type), enabled(true) {}
    virtual ~AudioNode() = default;
    
    // Core interface - MUST be implemented by all nodes
    virtual bool process(AudioBuffer& input, AudioBuffer& output) = 0;
    
    // Configuration interface
    virtual bool configure(JsonObject& config) { return true; }
    virtual void getConfig(JsonObject& config) {
        config["name"] = node_name;
        config["type"] = (int)node_type;
        config["enabled"] = enabled;
    }
    
    // Initialization (called once at startup)
    virtual bool init() { return true; }
    
    // Enable/disable node (for runtime control)
    void setEnabled(bool enable) { enabled = enable; }
    bool isEnabled() const { return enabled; }
    
    // Node identification
    const char* getName() const { return node_name; }
    AudioNodeType getType() const { return node_type; }
    
    // Performance metrics (optional)
    virtual void getMetrics(JsonObject& metrics) {
        metrics["process_time_us"] = last_process_time_us;
    }
    
    // Get bypass permission (override in derived classes)
    virtual bool getAllowBypass() const { return false; }  // Default: critical node
    
protected:
    const char* node_name;
    AudioNodeType node_type;
    bool enabled;
    uint32_t last_process_time_us = 0;
    
    // Helper to measure processing time
    void measureProcessTime(uint32_t start_us) {
        last_process_time_us = micros() - start_us;
    }
};

// Convenience typedef for node pointers
typedef AudioNode* AudioNodePtr;

#endif // AUDIO_NODE_H