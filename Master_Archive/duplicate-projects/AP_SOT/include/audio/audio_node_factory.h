/*
 * AudioNodeFactory - Dynamic Node Creation from JSON Configuration
 * ================================================================
 * 
 * Creates AudioNode instances based on type strings from JSON configs.
 * Enables fully dynamic pipeline construction at runtime.
 * 
 * USAGE:
 * AudioNodePtr node = AudioNodeFactory::createNode("GoertzelNode", config);
 * 
 * SUPPORTED NODES:
 * - I2SInputNode
 * - DCOffsetNode
 * - GoertzelNode
 * - MultibandAGCNode
 * - BeatDetectorNode
 * - ZoneMapperNode
 */

#ifndef AUDIO_NODE_FACTORY_H
#define AUDIO_NODE_FACTORY_H

#include "audio_node.h"
#include "nodes/i2s_input_node.h"
#include "nodes/dc_offset_node.h"
#include "nodes/goertzel_node.h"
#include "nodes/multiband_agc_node.h"
#include "nodes/beat_detector_node.h"
#include "nodes/zone_mapper_node.h"
#include <ArduinoJson.h>

class AudioNodeFactory {
public:
    // Create node from type string and optional configuration
    static AudioNodePtr createNode(const char* type, JsonObject& config) {
        AudioNodePtr node = nullptr;
        
        // Create appropriate node based on type
        if (strcmp(type, "I2SInputNode") == 0) {
            node = std::make_shared<I2SInputNode>();
        }
        else if (strcmp(type, "DCOffsetNode") == 0) {
            node = std::make_shared<DCOffsetNode>();
        }
        else if (strcmp(type, "GoertzelNode") == 0) {
            node = std::make_shared<GoertzelNode>();
        }
        else if (strcmp(type, "MultibandAGCNode") == 0) {
            node = std::make_shared<MultibandAGCNode>();
        }
        else if (strcmp(type, "BeatDetectorNode") == 0) {
            node = std::make_shared<BeatDetectorNode>();
        }
        else if (strcmp(type, "ZoneMapperNode") == 0) {
            node = std::make_shared<ZoneMapperNode>();
        }
        else {
            Serial.printf("AudioNodeFactory: Unknown node type '%s'\n", type);
            return nullptr;
        }
        
        // Configure node if config provided
        if (node && !config.isNull()) {
            if (!node->configure(config)) {
                Serial.printf("AudioNodeFactory: Failed to configure '%s'\n", type);
            }
        }
        
        return node;
    }
    
    // Create node with default configuration
    static AudioNodePtr createNode(const char* type) {
        StaticJsonDocument<64> doc;
        JsonObject empty_config = doc.to<JsonObject>();
        return createNode(type, empty_config);
    }
    
    // Build entire pipeline from JSON configuration
    static bool buildPipeline(AudioPipeline& pipeline, JsonArray& nodes_config) {
        for (JsonObject node_config : nodes_config) {
            const char* type = node_config["type"];
            if (!type) {
                Serial.println("AudioNodeFactory: Node missing 'type' field");
                continue;
            }
            
            // Create and configure node
            AudioNodePtr node = createNode(type, node_config);
            if (!node) {
                Serial.printf("AudioNodeFactory: Failed to create '%s'\n", type);
                return false;
            }
            
            // Add to pipeline
            if (!pipeline.addNode(node)) {
                Serial.printf("AudioNodeFactory: Failed to add '%s' to pipeline\n", type);
                return false;
            }
        }
        
        return true;
    }
    
    // Load pipeline configuration from JSON string
    static bool loadPipelineFromJSON(AudioPipeline& pipeline, const char* json_config) {
        StaticJsonDocument<2048> doc;
        DeserializationError error = deserializeJson(doc, json_config);
        
        if (error) {
            Serial.printf("AudioNodeFactory: JSON parse error: %s\n", error.c_str());
            return false;
        }
        
        JsonObject config = doc.as<JsonObject>();
        
        // Check for nodes array
        if (!config.containsKey("nodes")) {
            Serial.println("AudioNodeFactory: JSON missing 'nodes' array");
            return false;
        }
        
        JsonArray nodes = config["nodes"];
        return buildPipeline(pipeline, nodes);
    }
};

// Example JSON configuration:
const char* EXAMPLE_PIPELINE_CONFIG = R"({
    "name": "Main Audio Pipeline",
    "nodes": [
        {
            "type": "I2SInputNode",
            "sample_rate": 16000,
            "chunk_size": 128
        },
        {
            "type": "DCOffsetNode",
            "mode": "calibrate",
            "high_pass_enabled": true
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
            "num_zones": 36,
            "mapping_mode": "logarithmic"
        }
    ]
})";

#endif // AUDIO_NODE_FACTORY_H