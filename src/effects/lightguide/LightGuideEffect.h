#ifndef LIGHT_GUIDE_EFFECT_H
#define LIGHT_GUIDE_EFFECT_H

#include <FastLED.h>
#include "../../config/hardware_config.h"
#include "../EffectBase.h"

// Physical constants for Light Guide Plate
namespace LightGuide {
    // Physical properties
    constexpr float PLATE_LENGTH_MM = 329.0f;
    constexpr uint16_t LEDS_PER_EDGE = 160;
    constexpr float LED_SPACING_MM = PLATE_LENGTH_MM / LEDS_PER_EDGE;
    
    // Optical properties
    constexpr float REFRACTIVE_INDEX = 1.49f;  // Acrylic
    constexpr float CRITICAL_ANGLE = 42.2f;    // For total internal reflection
    constexpr float PROPAGATION_LOSS_DB_M = 0.1f;  // Material loss
    
    // Simulation constants
    constexpr uint8_t INTERFERENCE_MAP_RESOLUTION = 32;  // Resolution for interference calculations
    constexpr uint8_t MAX_DEPTH_LAYERS = 5;
}

// Coordinate mapping for Light Guide Plate
struct LightGuideCoords {
    float edge1_position;    // Position along Edge 1 (0.0 - 1.0)
    float edge2_position;    // Position along Edge 2 (0.0 - 1.0)
    float center_distance;   // Distance from plate center (0.0 - 1.0)
    float interference_zone; // Calculated interference intensity
};

// Synchronization modes for Light Guide
enum class LightGuideSyncMode : uint8_t {
    INTERFERENCE,      // Full interference calculation
    INDEPENDENT,       // Edges operate independently
    MIRRORED,         // Edge 2 mirrors Edge 1
    PHASE_LOCKED,     // Edges locked with phase offset
    ALTERNATING,      // Edges alternate dominance
    COOPERATIVE       // Edges work together for combined effects
};

// Base class for Light Guide Plate effects
class LightGuideEffect : public EffectBase {
protected:
    // References to the LED strips
    CRGB* edge1;  // Strip 1 (edge injection)
    CRGB* edge2;  // Strip 2 (opposite edge)
    
    // Interference map for performance optimization
    uint8_t interferenceMap[LightGuide::INTERFERENCE_MAP_RESOLUTION];
    uint32_t interferenceMapTimestamp;
    
    // Common parameters
    float interference_strength = 1.0f;
    float phase_offset = 0.0f;
    float propagation_speed = 1.0f;
    LightGuideSyncMode sync_mode = LightGuideSyncMode::INTERFERENCE;
    
    // Helper methods
    
    // Set LED on edge 1
    void setEdge1LED(uint16_t index, CRGB color) {
        if (index < HardwareConfig::STRIP_LENGTH) {
            edge1[index] = color;
        }
    }
    
    // Set LED on edge 2
    void setEdge2LED(uint16_t index, CRGB color) {
        if (index < HardwareConfig::STRIP_LENGTH) {
            edge2[index] = color;
        }
    }
    
    // Calculate interference between two edge positions
    float calculateInterference(uint16_t edge1_pos, uint16_t edge2_pos, float phase = 0.0f) {
        // Simple interference model: cos(phase_difference)
        float dist1 = (float)edge1_pos / HardwareConfig::STRIP_LENGTH;
        float dist2 = (float)edge2_pos / HardwareConfig::STRIP_LENGTH;
        
        // Phase difference based on path length
        float phase_diff = (dist1 - dist2) * TWO_PI * interference_strength + phase;
        
        // Constructive/destructive interference
        return (1.0f + cos(phase_diff)) * 0.5f;
    }
    
    // Map edge positions to plate coordinates
    LightGuideCoords mapToPlate(uint16_t edge1_pos, uint16_t edge2_pos) {
        LightGuideCoords coords;
        coords.edge1_position = (float)edge1_pos / HardwareConfig::STRIP_LENGTH;
        coords.edge2_position = (float)edge2_pos / HardwareConfig::STRIP_LENGTH;
        
        // Calculate virtual center distance
        float x = (coords.edge1_position + coords.edge2_position) * 0.5f;
        float y = abs(coords.edge1_position - coords.edge2_position);
        coords.center_distance = sqrt(x * x + y * y);
        
        // Calculate interference
        coords.interference_zone = calculateInterference(edge1_pos, edge2_pos, phase_offset);
        
        return coords;
    }
    
    // Apply center-origin constraint (MANDATORY)
    void applyCenterOriginConstraint(uint16_t& pos, bool outward = true) {
        if (outward) {
            // Ensure effects move outward from center
            if (pos < HardwareConfig::STRIP_CENTER_POINT) {
                pos = HardwareConfig::STRIP_CENTER_POINT - (HardwareConfig::STRIP_CENTER_POINT - pos);
            } else {
                pos = HardwareConfig::STRIP_CENTER_POINT + (pos - HardwareConfig::STRIP_CENTER_POINT);
            }
        }
    }
    
    // Calculate depth layer for volumetric effects
    uint8_t calculateDepthLayer(float interference, uint8_t num_layers = 3) {
        return (uint8_t)(interference * (num_layers - 1));
    }
    
    // Apply optical propagation loss
    uint8_t applyPropagationLoss(uint8_t brightness, float distance) {
        // Exponential decay approximation
        float loss_factor = 1.0f - (distance * LightGuide::PROPAGATION_LOSS_DB_M * 0.1f);
        return (uint8_t)(brightness * max(0.0f, loss_factor));
    }
    
public:
    LightGuideEffect(const char* effectName, 
                     CRGB* strip1, 
                     CRGB* strip2,
                     uint8_t brightness = 128, 
                     uint8_t speed = 10, 
                     uint8_t fade = 20) :
        EffectBase(effectName, brightness, speed, fade),
        edge1(strip1),
        edge2(strip2),
        interferenceMapTimestamp(0) {
        
        // Initialize interference map
        memset(interferenceMap, 0, sizeof(interferenceMap));
    }
    
    virtual ~LightGuideEffect() {}
    
    // Set synchronization mode
    void setSyncMode(LightGuideSyncMode mode) {
        sync_mode = mode;
    }
    
    // Set interference parameters
    void setInterferenceParams(float strength, float phase) {
        interference_strength = strength;
        phase_offset = phase;
    }
    
    // Update interference map (for performance optimization)
    void updateInterferenceMap() {
        uint32_t now = millis();
        if (now - interferenceMapTimestamp > 100) {  // Update every 100ms
            for (uint8_t i = 0; i < LightGuide::INTERFERENCE_MAP_RESOLUTION; i++) {
                float pos = (float)i / LightGuide::INTERFERENCE_MAP_RESOLUTION;
                uint16_t led_pos = (uint16_t)(pos * HardwareConfig::STRIP_LENGTH);
                float interference = calculateInterference(led_pos, HardwareConfig::STRIP_LENGTH - led_pos - 1, phase_offset);
                interferenceMap[i] = (uint8_t)(interference * 255);
            }
            interferenceMapTimestamp = now;
        }
    }
    
    // Quick lookup for interference value
    uint8_t getInterferenceQuick(uint16_t position) {
        uint8_t map_index = (position * LightGuide::INTERFERENCE_MAP_RESOLUTION) / HardwareConfig::STRIP_LENGTH;
        return interferenceMap[min(map_index, (uint8_t)(LightGuide::INTERFERENCE_MAP_RESOLUTION - 1))];
    }
};

// Factory function type for creating Light Guide effects
typedef LightGuideEffect* (*LightGuideEffectFactory)(CRGB*, CRGB*);

#endif // LIGHT_GUIDE_EFFECT_H