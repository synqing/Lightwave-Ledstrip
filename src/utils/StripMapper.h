#ifndef STRIP_MAPPER_H
#define STRIP_MAPPER_H

#include <Arduino.h>
#include "../config/hardware_config.h"

// Global arrays for strip mapping
extern uint8_t angles[HardwareConfig::NUM_LEDS];
extern uint8_t radii[HardwareConfig::NUM_LEDS];

class StripMapper {
private:
    // Mapping parameters
    static constexpr float PHI = 1.618033988749895; // Golden ratio
    static constexpr float ANGLE_SCALE = 137.5; // Golden angle in degrees
    
public:
    void initializeMapping() {
        Serial.println(F("[INFO] Initializing strip mapping..."));
        
        // For linear strip, create virtual circular/spiral mapping
        for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
            // Map position along strip to angle (0-255)
            angles[i] = map(i, 0, HardwareConfig::NUM_LEDS - 1, 0, 255);
            
            // Create radius based on Fibonacci spiral
            float normalizedPos = float(i) / float(HardwareConfig::NUM_LEDS - 1);
            
            // Create a spiral pattern
            float spiralRadius = normalizedPos * 127.0f + 
                                sin(normalizedPos * PI * 4) * 64.0f;
            
            radii[i] = constrain(spiralRadius, 0, 255);
        }
        
        #if FEATURE_DEBUG_OUTPUT
        Serial.println(F("[INFO] Strip mapping complete"));
        // Print sample mappings
        Serial.println(F("Sample mappings (LED: angle, radius):"));
        for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i += 10) {
            Serial.print(F("  LED "));
            Serial.print(i);
            Serial.print(F(": "));
            Serial.print(angles[i]);
            Serial.print(F(", "));
            Serial.println(radii[i]);
        }
        #endif
    }
    
    // Alternative mapping functions for different effects
    void setLinearMapping() {
        for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
            angles[i] = map(i, 0, HardwareConfig::NUM_LEDS - 1, 0, 255);
            radii[i] = 128; // Fixed radius for linear effects
        }
    }
    
    void setSpiralMapping() {
        for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
            float angle = fmod(i * ANGLE_SCALE, 360.0);
            angles[i] = (angle / 360.0) * 255;
            
            float r = sqrt(i) * 16.0;
            radii[i] = constrain(r, 0, 255);
        }
    }
    
    void setRadialMapping(uint8_t numSpokes = 8) {
        for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
            // Divide strip into spokes
            uint16_t ledsPerSpoke = HardwareConfig::NUM_LEDS / numSpokes;
            uint8_t spoke = i / ledsPerSpoke;
            uint16_t positionInSpoke = i % ledsPerSpoke;
            
            angles[i] = (spoke * 255) / numSpokes;
            radii[i] = map(positionInSpoke, 0, ledsPerSpoke - 1, 0, 255);
        }
    }
};

// Global mapper instance
extern StripMapper stripMapper;

#endif // STRIP_MAPPER_H