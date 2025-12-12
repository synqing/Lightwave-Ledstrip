/*
 * AudioPipeline - Dynamic Audio Processing Chain Manager
 * ======================================================
 * 
 * Manages a chain of AudioNode modules for real-time audio processing.
 * Supports runtime reconfiguration without audio glitches.
 * 
 * FEATURES:
 * - Dynamic node insertion/removal
 * - Parallel processing branches
 * - Performance monitoring
 * - JSON configuration
 * 
 * EXAMPLE PIPELINE:
 * I2S Input → DC Offset → Goertzel → [Beat Detector]
 *                                  → [Multiband AGC] → Zones → Output
 */

#ifndef AUDIO_PIPELINE_H
#define AUDIO_PIPELINE_H

#include "audio_node.h"
#include <vector>

// Maximum nodes in pipeline (to avoid dynamic allocation)
#define MAX_PIPELINE_NODES 16
#define MAX_AUDIO_BUFFER_SIZE 512

// Pipeline processing error codes
enum class PipelineError {
    NONE = 0,
    NODE_FAILED = 1,
    BUFFER_OVERFLOW = 2,
    INVALID_INPUT = 3,
    TIMEOUT = 4,
    CRITICAL_FAILURE = 5
};

// Pipeline health status
struct PipelineHealth {
    bool is_healthy = true;
    uint32_t total_failures = 0;
    uint32_t consecutive_failures = 0;
    uint32_t last_failure_time = 0;
    const char* last_failed_node = nullptr;
    PipelineError last_error = PipelineError::NONE;
};

class AudioPipeline {
public:
    AudioPipeline(const char* name = "Main") : pipeline_name(name) {
        // Pre-allocate buffers for zero-copy operation
        for (int i = 0; i < 2; i++) {
            buffers[i].data = buffer_memory[i];
            buffers[i].size = MAX_AUDIO_BUFFER_SIZE;
        }
        
        // Pre-allocate node output buffers for pipeline tapping
        for (int i = 0; i < MAX_PIPELINE_NODES; i++) {
            node_output_buffers[i].data = node_output_memory[i];
            node_output_buffers[i].size = MAX_AUDIO_BUFFER_SIZE;
        }
    }
    
    // Add a node to the pipeline
    bool addNode(AudioNodePtr node) {
        if (node_count >= MAX_PIPELINE_NODES) {
            Serial.println("AudioPipeline: Maximum nodes reached!");
            return false;
        }
        
        nodes[node_count++] = node;
        Serial.printf("AudioPipeline: Added node '%s' at position %d\n", 
                     node->getName(), node_count - 1);
        return true;
    }
    
    // Insert a node at specific position
    bool insertNode(AudioNodePtr node, int position) {
        if (node_count >= MAX_PIPELINE_NODES || position > node_count) {
            return false;
        }
        
        // Shift nodes to make room
        for (int i = node_count; i > position; i--) {
            nodes[i] = nodes[i-1];
        }
        
        nodes[position] = node;
        node_count++;
        return true;
    }
    
    // Remove a node by name
    bool removeNode(const char* name) {
        for (int i = 0; i < node_count; i++) {
            if (strcmp(nodes[i]->getName(), name) == 0) {
                // Shift remaining nodes
                for (int j = i; j < node_count - 1; j++) {
                    nodes[j] = nodes[j+1];
                }
                node_count--;
                return true;
            }
        }
        return false;
    }
    
    // Process audio through the pipeline with error reporting
    PipelineError process(float* input_data, size_t input_size) {
        if (node_count == 0) return PipelineError::INVALID_INPUT;
        
        // Check input validity
        if (input_size > MAX_AUDIO_BUFFER_SIZE) {
            updateHealth(PipelineError::BUFFER_OVERFLOW, nullptr);
            return PipelineError::BUFFER_OVERFLOW;
        }
        
        // Setup initial buffer
        buffers[0].data = input_data;
        buffers[0].size = input_size;
        buffers[0].timestamp = millis();
        
        // Process through each node
        int current_buffer = 0;
        for (int i = 0; i < node_count; i++) {
            if (!nodes[i]->isEnabled()) continue;
            
            int next_buffer = 1 - current_buffer;
            uint32_t start_time = micros();
            
            bool success = nodes[i]->process(buffers[current_buffer], 
                                            buffers[next_buffer]);
            
            if (!success) {
                const char* node_name = nodes[i]->getName();
                Serial.printf("AudioPipeline: Node '%s' failed!\n", node_name);
                
                // Update health status
                updateHealth(PipelineError::NODE_FAILED, node_name);
                
                // Check if node allows bypass
                if (nodes[i]->getAllowBypass()) {
                    Serial.printf("AudioPipeline: Bypassing failed node '%s'\n", node_name);
                    // Copy input to output and continue
                    memcpy(buffers[next_buffer].data, buffers[current_buffer].data,
                           buffers[current_buffer].size * sizeof(float));
                    buffers[next_buffer].size = buffers[current_buffer].size;
                    buffers[next_buffer].timestamp = buffers[current_buffer].timestamp;
                    buffers[next_buffer].is_silence = buffers[current_buffer].is_silence;
                    buffers[next_buffer].metadata = buffers[current_buffer].metadata;
                } else {
                    // Critical node failure, stop pipeline
                    return PipelineError::NODE_FAILED;
                }
            }
            
            // Save node output for pipeline tapping
            memcpy(node_output_buffers[i].data, buffers[next_buffer].data, 
                   buffers[next_buffer].size * sizeof(float));
            node_output_buffers[i].size = buffers[next_buffer].size;
            node_output_buffers[i].timestamp = buffers[next_buffer].timestamp;
            node_output_buffers[i].is_silence = buffers[next_buffer].is_silence;
            node_output_buffers[i].metadata = buffers[next_buffer].metadata;
            
            // Measure node performance
            nodes[i]->measureProcessTime(start_time);
            
            current_buffer = next_buffer;
        }
        
        // Update pipeline metrics
        last_process_time_ms = millis() - buffers[0].timestamp;
        frames_processed++;
        
        // Success - reset consecutive failures
        if (health.consecutive_failures > 0) {
            health.consecutive_failures = 0;
            health.is_healthy = true;
        }
        
        return PipelineError::NONE;
    }
    
    // Get pipeline health status
    const PipelineHealth& getHealth() const { return health; }
    
    // Reset health status
    void resetHealth() {
        health = PipelineHealth();
    }
    
    // Configure pipeline from JSON
    bool configure(JsonObject& config) {
        if (config.containsKey("nodes")) {
            JsonArray nodes_config = config["nodes"];
            for (JsonObject node_config : nodes_config) {
                const char* name = node_config["name"];
                AudioNodePtr node = findNode(name);
                if (node) {
                    node->configure(node_config);
                }
            }
        }
        return true;
    }
    
    // Get pipeline configuration
    void getConfig(JsonObject& config) {
        config["name"] = pipeline_name;
        config["node_count"] = node_count;
        
        JsonArray nodes_array = config.createNestedArray("nodes");
        for (int i = 0; i < node_count; i++) {
            JsonObject node_config = nodes_array.createNestedObject();
            nodes[i]->getConfig(node_config);
        }
    }
    
    // Get performance metrics
    void getMetrics(JsonObject& metrics) {
        metrics["pipeline_name"] = pipeline_name;
        metrics["frames_processed"] = frames_processed;
        metrics["last_process_ms"] = last_process_time_ms;
        
        JsonArray nodes_metrics = metrics.createNestedArray("nodes");
        for (int i = 0; i < node_count; i++) {
            JsonObject node_metric = nodes_metrics.createNestedObject();
            node_metric["name"] = nodes[i]->getName();
            nodes[i]->getMetrics(node_metric);
        }
    }
    
    // Find node by name
    AudioNodePtr findNode(const char* name) {
        for (int i = 0; i < node_count; i++) {
            if (strcmp(nodes[i]->getName(), name) == 0) {
                return nodes[i];
            }
        }
        return nullptr;
    }
    
    // Get output buffer from a specific node (for pipeline tapping)
    AudioBuffer* getNodeOutput(const char* node_name) {
        for (int i = 0; i < node_count; i++) {
            if (strcmp(nodes[i]->getName(), node_name) == 0) {
                // Return the output buffer for this node
                // The node's output is in the "next" buffer after processing
                return &node_output_buffers[i];
            }
        }
        return nullptr;
    }
    
    // Print pipeline structure
    void printStructure() {
        Serial.printf("\n=== AudioPipeline: %s ===\n", pipeline_name);
        for (int i = 0; i < node_count; i++) {
            Serial.printf("%d. %s (%s)\n", i, nodes[i]->getName(),
                         nodes[i]->isEnabled() ? "enabled" : "disabled");
        }
        Serial.println("========================\n");
    }
    
private:
    const char* pipeline_name;
    AudioNodePtr nodes[MAX_PIPELINE_NODES];
    int node_count = 0;
    
    // Double buffering for zero-copy operation
    AudioBuffer buffers[2];
    float buffer_memory[2][MAX_AUDIO_BUFFER_SIZE];
    
    // Node output buffers for pipeline tapping
    AudioBuffer node_output_buffers[MAX_PIPELINE_NODES];
    float node_output_memory[MAX_PIPELINE_NODES][MAX_AUDIO_BUFFER_SIZE];
    
    // Metrics
    uint32_t frames_processed = 0;
    uint32_t last_process_time_ms = 0;
    
    // Health tracking
    PipelineHealth health;
    
    // Update health status on error
    void updateHealth(PipelineError error, const char* failed_node) {
        health.last_error = error;
        health.last_failed_node = failed_node;
        health.last_failure_time = millis();
        health.total_failures++;
        health.consecutive_failures++;
        
        // Mark unhealthy after 3 consecutive failures
        if (health.consecutive_failures >= 3) {
            health.is_healthy = false;
        }
    }
};

#endif // AUDIO_PIPELINE_H