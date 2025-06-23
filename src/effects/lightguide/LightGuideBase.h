#ifndef LIGHT_GUIDE_BASE_H
#define LIGHT_GUIDE_BASE_H

#include <Arduino.h>
#include <FastLED.h>
#include "config/features.h"
#include "config/hardware_config.h"
#include "../EffectBase.h"

#if LED_STRIPS_MODE && FEATURE_LIGHT_GUIDE_MODE

// Light Guide Physical Constants
namespace LightGuide {
    // Physical measurements
    constexpr float PLATE_LENGTH_MM = 329.0f;
    constexpr uint16_t LEDS_PER_EDGE = HardwareConfig::STRIP_LENGTH;  // 160
    constexpr float LED_SPACING_MM = PLATE_LENGTH_MM / LEDS_PER_EDGE;  // ~2.06mm
    
    // Optical properties (typical acrylic)
    constexpr float REFRACTIVE_INDEX = 1.49f;
    constexpr float CRITICAL_ANGLE = 42.2f;    // Degrees, for total internal reflection
    constexpr float PROPAGATION_LOSS_DB_M = 0.1f;  // Material loss per meter
    
    // Computational constants
    constexpr uint16_t INTERFERENCE_MAP_WIDTH = 160;  // Match LED count
    constexpr uint16_t INTERFERENCE_MAP_HEIGHT = 80;  // Virtual height for 2D mapping
    constexpr float PI_F = 3.14159265f;
    constexpr float TWO_PI_F = 6.28318531f;
}

// Light Guide Coordinate System
struct LightGuideCoords {
    float edge1_position;    // Position along Edge 1 (0.0 - 1.0)
    float edge2_position;    // Position along Edge 2 (0.0 - 1.0)
    float center_distance;   // Distance from plate center (0.0 - 1.0)
    float interference_zone; // Calculated interference intensity (0.0 - 1.0)
    float propagation_distance; // Distance traveled through plate
};

// Light Guide Synchronization Modes
enum class LightGuideSyncMode : uint8_t {
    INTERFERENCE = 0,      // Full interference calculation between edges
    INDEPENDENT = 1,       // Edges operate independently
    MIRRORED = 2,         // Edge 2 mirrors Edge 1
    PHASE_LOCKED = 3,     // Edges locked with phase offset
    ALTERNATING = 4,      // Edges alternate dominance
    COOPERATIVE = 5       // Edges work together for combined effects
};

// Wave Parameters for Interference Calculations
struct WaveParameters {
    float frequency;       // Wave frequency (Hz equivalent)
    float amplitude;       // Wave amplitude (0.0 - 1.0)
    float phase;          // Phase offset (0.0 - 2π)
    float wavelength;     // Wavelength in plate units
    float decay_rate;     // Amplitude decay with distance
};

// Base class for all Light Guide effects
class LightGuideEffectBase : public EffectBase {
protected:
    // Current synchronization mode
    LightGuideSyncMode sync_mode = LightGuideSyncMode::INTERFERENCE;
    
    // Global parameters for light guide effects
    float interference_strength = 1.0f;    // Overall interference intensity
    float phase_offset = 0.0f;             // Global phase offset
    float propagation_speed = 1.0f;        // Speed of wave propagation
    float edge_balance = 0.5f;             // Balance between edges (0=Edge1, 1=Edge2)
    
    // Wave parameters for each edge
    WaveParameters edge1_wave;
    WaveParameters edge2_wave;
    
    // Buffers for interference calculations
    float interference_map[LightGuide::INTERFERENCE_MAP_WIDTH][LightGuide::INTERFERENCE_MAP_HEIGHT];
    uint32_t last_interference_calc = 0;   // Timestamp for calculation optimization
    
    // External references to LED strips
    extern CRGB strip1[HardwareConfig::STRIP1_LED_COUNT];
    extern CRGB strip2[HardwareConfig::STRIP2_LED_COUNT];
    extern CRGBPalette16 currentPalette;
    extern uint8_t gHue;
    
public:
    LightGuideEffectBase(const char* name, uint8_t default_brightness, 
                        uint8_t default_speed, uint8_t default_intensity)
        : EffectBase(name, default_brightness, default_speed, default_intensity) {
        
        // Initialize wave parameters
        edge1_wave = {1.0f, 1.0f, 0.0f, 10.0f, 0.1f};
        edge2_wave = {1.0f, 1.0f, LightGuide::PI_F, 10.0f, 0.1f};
        
        // Clear interference map
        memset(interference_map, 0, sizeof(interference_map));
    }
    
    // Virtual methods to be implemented by derived classes
    virtual void renderLightGuideEffect() = 0;
    
    // Final render method that handles light guide specifics
    void render() override final {
        // Update wave parameters based on global settings
        updateWaveParameters();
        
        // Calculate interference pattern if needed
        if (sync_mode == LightGuideSyncMode::INTERFERENCE) {
            calculateInterferencePattern();
        }
        
        // Clear strips
        fadeToBlackBy(strip1, HardwareConfig::STRIP1_LED_COUNT, fadeAmount);
        fadeToBlackBy(strip2, HardwareConfig::STRIP2_LED_COUNT, fadeAmount);
        
        // Render the specific effect
        renderLightGuideEffect();
        
        // Apply synchronization mode
        applySynchronization();
    }
    
protected:
    // Update wave parameters based on effect settings
    void updateWaveParameters() {
        float time_factor = millis() * 0.001f * propagation_speed;
        
        edge1_wave.phase += edge1_wave.frequency * time_factor * 0.01f;
        edge2_wave.phase += edge2_wave.frequency * time_factor * 0.01f;
        
        // Keep phases in range [0, 2π]
        edge1_wave.phase = fmod(edge1_wave.phase, LightGuide::TWO_PI_F);
        edge2_wave.phase = fmod(edge2_wave.phase, LightGuide::TWO_PI_F);
    }
    
    // Calculate interference pattern between edges
    void calculateInterferencePattern() {
        uint32_t now = millis();
        
        // Optimize: only recalculate every 16ms for 60Hz interference updates
        if (now - last_interference_calc < 16) return;
        last_interference_calc = now;
        
        for (uint16_t x = 0; x < LightGuide::INTERFERENCE_MAP_WIDTH; x++) {
            for (uint16_t y = 0; y < LightGuide::INTERFERENCE_MAP_HEIGHT; y++) {
                // Map to plate coordinates
                float plate_x = (float)x / LightGuide::INTERFERENCE_MAP_WIDTH;
                float plate_y = (float)y / LightGuide::INTERFERENCE_MAP_HEIGHT;
                
                // Calculate wave contributions from each edge
                float edge1_contrib = calculateWaveContribution(edge1_wave, plate_x, plate_y, 0.0f);
                float edge2_contrib = calculateWaveContribution(edge2_wave, plate_x, plate_y, 1.0f);
                
                // Calculate interference
                interference_map[x][y] = calculateInterference(edge1_contrib, edge2_contrib);
            }
        }
    }
    
    // Calculate wave contribution at a point in the plate
    float calculateWaveContribution(const WaveParameters& wave, float x, float y, float edge_pos) {
        // Distance from edge to point
        float distance = abs(y - edge_pos);
        
        // Wave amplitude with decay
        float amplitude = wave.amplitude * exp(-wave.decay_rate * distance);
        
        // Wave phase at this position
        float position_phase = wave.phase + (distance / wave.wavelength) * LightGuide::TWO_PI_F;
        
        // Calculate wave value
        return amplitude * sin(position_phase);
    }
    
    // Calculate interference between two wave contributions
    float calculateInterference(float wave1, float wave2) {
        // Simple interference: sum of waves
        float interference = wave1 + wave2;
        
        // Apply interference strength
        interference *= interference_strength;
        
        // Clamp to valid range
        return constrain(interference, -1.0f, 1.0f);
    }
    
    // Map plate coordinates to light guide coordinate system
    LightGuideCoords mapToLightGuide(float plate_x, float plate_y) {
        LightGuideCoords coords;
        
        coords.edge1_position = plate_x;
        coords.edge2_position = plate_x;
        coords.center_distance = abs(plate_y - 0.5f) * 2.0f;  // 0 at center, 1 at edges
        
        // Get interference value at this position
        uint16_t map_x = constrain(plate_x * LightGuide::INTERFERENCE_MAP_WIDTH, 0, LightGuide::INTERFERENCE_MAP_WIDTH - 1);
        uint16_t map_y = constrain(plate_y * LightGuide::INTERFERENCE_MAP_HEIGHT, 0, LightGuide::INTERFERENCE_MAP_HEIGHT - 1);
        coords.interference_zone = interference_map[map_x][map_y];
        
        // Calculate propagation distance (for attenuation)
        coords.propagation_distance = plate_y * LightGuide::PLATE_LENGTH_MM;
        
        return coords;
    }
    
    // Set LED color on Edge 1 (Strip 1)
    void setEdge1LED(uint16_t index, CRGB color) {
        if (index < HardwareConfig::STRIP1_LED_COUNT) {
            strip1[index] = color;
        }
    }
    
    // Set LED color on Edge 2 (Strip 2)  
    void setEdge2LED(uint16_t index, CRGB color) {
        if (index < HardwareConfig::STRIP2_LED_COUNT) {
            strip2[index] = color;
        }
    }
    
    // Get color from palette with light guide modifications
    CRGB getLightGuideColor(uint8_t palette_index, float intensity, float propagation_distance = 0.0f) {
        // Apply propagation loss
        float loss_factor = exp(-LightGuide::PROPAGATION_LOSS_DB_M * propagation_distance / 1000.0f);
        intensity *= loss_factor;
        
        // Clamp intensity
        intensity = constrain(intensity, 0.0f, 1.0f);
        
        // Get color from palette
        uint8_t brightness = (uint8_t)(intensity * 255);
        return ColorFromPalette(currentPalette, palette_index, brightness);
    }
    
    // Apply synchronization mode to strips
    void applySynchronization() {
        switch (sync_mode) {
            case LightGuideSyncMode::INTERFERENCE:
                // Already handled in interference calculation
                break;
                
            case LightGuideSyncMode::INDEPENDENT:
                // Strips operate independently - no action needed
                break;
                
            case LightGuideSyncMode::MIRRORED:
                // Mirror strip1 to strip2
                for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
                    uint16_t mirror_index = HardwareConfig::STRIP_LENGTH - 1 - i;
                    strip2[i] = strip1[mirror_index];
                }
                break;
                
            case LightGuideSyncMode::PHASE_LOCKED:
                // Copy with phase offset
                {
                    uint16_t offset = (uint16_t)(phase_offset * HardwareConfig::STRIP_LENGTH / LightGuide::TWO_PI_F);
                    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
                        uint16_t source_index = (i + offset) % HardwareConfig::STRIP_LENGTH;
                        strip2[i] = strip1[source_index];
                    }
                }
                break;
                
            case LightGuideSyncMode::ALTERNATING:
                // Alternate dominance based on time
                {
                    bool edge1_dominant = (millis() / 1000) % 2 == 0;
                    if (!edge1_dominant) {
                        // Copy strip1 to strip2 and dim strip1
                        for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
                            strip2[i] = strip1[i];
                            strip1[i].fadeToBlackBy(128);
                        }
                    } else {
                        // Dim strip2
                        fadeToBlackBy(strip2, HardwareConfig::STRIP2_LED_COUNT, 128);
                    }
                }
                break;
                
            case LightGuideSyncMode::COOPERATIVE:
                // Blend strips based on edge balance
                {
                    uint8_t blend_amount = (uint8_t)(edge_balance * 255);
                    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
                        strip2[i] = blend(strip1[i], strip2[i], blend_amount);
                    }
                }
                break;
        }
    }
    
public:
    // Parameter setters for external control (e.g., encoders)
    void setInterferenceStrength(float strength) { 
        interference_strength = constrain(strength, 0.0f, 2.0f); 
    }
    
    void setPhaseOffset(float offset) { 
        phase_offset = fmod(offset, LightGuide::TWO_PI_F); 
    }
    
    void setPropagationSpeed(float speed) { 
        propagation_speed = constrain(speed, 0.1f, 5.0f); 
    }
    
    void setEdgeBalance(float balance) { 
        edge_balance = constrain(balance, 0.0f, 1.0f); 
    }
    
    void setSyncMode(LightGuideSyncMode mode) { 
        sync_mode = mode; 
    }
    
    // Parameter getters
    float getInterferenceStrength() const { return interference_strength; }
    float getPhaseOffset() const { return phase_offset; }
    float getPropagationSpeed() const { return propagation_speed; }
    float getEdgeBalance() const { return edge_balance; }
    LightGuideSyncMode getSyncMode() const { return sync_mode; }
};

#endif // LED_STRIPS_MODE && FEATURE_LIGHT_GUIDE_MODE

#endif // LIGHT_GUIDE_BASE_H