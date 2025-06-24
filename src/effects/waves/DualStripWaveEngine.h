#ifndef DUAL_STRIP_WAVE_ENGINE_H
#define DUAL_STRIP_WAVE_ENGINE_H

#include <Arduino.h>
#include <FastLED.h>
#include "../../config/hardware_config.h"

// External references
extern CRGB strip1[HardwareConfig::STRIP1_LED_COUNT];
extern CRGB strip2[HardwareConfig::STRIP2_LED_COUNT];
extern uint8_t gHue;

// Wave type enumeration
enum WaveType : uint8_t {
    WAVE_SINE = 0,
    WAVE_TRIANGLE = 1,
    WAVE_SAWTOOTH = 2,
    WAVE_GAUSSIAN = 3,
    WAVE_DAMPED = 4
};

// Interaction mode enumeration
enum InteractionMode : uint8_t {
    MODE_INDEPENDENT = 0,
    MODE_INTERFERENCE = 1,
    MODE_CHASE = 2,
    MODE_REFLECTION = 3,
    MODE_SPIRAL = 4,
    MODE_PULSE = 5
};

/**
 * Dual-Strip Wave Engine - Stack-allocated, zero-malloc design
 * Total memory: ~200 bytes vs 153KB for LGP system
 */
struct DualStripWaveEngine {
    // ==================== WAVE PARAMETERS ====================
    
    // Frequency control (Hz equivalent)
    float strip1_frequency = 2.0f;     // Primary wave frequency
    float strip2_frequency = 2.1f;     // Secondary frequency (creates beats)
    
    // Phase control (radians)
    float strip1_phase = 0.0f;         // Current phase accumulator
    float strip2_phase = 0.0f;         // Secondary phase
    float manual_phase_offset = 0.0f;  // User-controlled phase offset
    
    // Wave characteristics
    float wave_speed = 1.0f;           // Propagation speed multiplier
    float amplitude = 1.0f;            // Wave amplitude (0.1 - 2.0)
    uint8_t wave_type = WAVE_SINE;     // Wave shape
    
    // ==================== INTERACTION MODES ====================
    
    uint8_t interaction_mode = MODE_INDEPENDENT;  // Wave interaction type
    
    // ==================== PROPAGATION CONTROL ====================
    
    bool bidirectional = false;       // Waves from both ends
    bool center_origin = true;        // Respect center-origin philosophy
    float beat_enhancement = 1.0f;    // Enhance beat frequency visualization
    
    // ==================== RUNTIME STATE ====================
    
    uint32_t last_update = 0;         // Last update timestamp
    float time_accumulator = 0.0f;    // Time accumulation for animations
    float beat_frequency = 0.0f;      // Calculated beat frequency
    
    // ==================== PERFORMANCE MONITORING ====================
    
    uint32_t render_time_us = 0;      // Last render time in microseconds
    uint16_t frame_count = 0;         // Frame counter for FPS calculation
};

// ==================== WAVE GENERATION FUNCTIONS ====================

/**
 * Generate wave value at given position with specified type
 * @param position: Normalized position (0.0 - 1.0)
 * @param frequency: Wave frequency (Hz equivalent)
 * @param phase: Current phase (radians)
 * @param type: Wave type (0-4)
 * @return: Wave value (-1.0 to +1.0)
 */
inline float generateWave(float position, float frequency, float phase, uint8_t type) {
    float arg = 2.0f * PI * frequency * position + phase;
    
    switch(type) {
        case WAVE_SINE: // Sine wave - smooth, classic
            return sin(arg);
            
        case WAVE_TRIANGLE: // Triangle wave - linear segments
            {
                float normalized = fmod(arg, 2.0f * PI) / (2.0f * PI);
                if (normalized < 0.25f) {
                    return 4.0f * normalized;
                } else if (normalized < 0.75f) {
                    return 2.0f - 4.0f * normalized;
                } else {
                    return 4.0f * normalized - 4.0f;
                }
            }
            
        case WAVE_SAWTOOTH: // Sawtooth wave - sharp transitions
            {
                float normalized = fmod(arg, 2.0f * PI) / (2.0f * PI);
                return 2.0f * normalized - 1.0f;
            }
            
        case WAVE_GAUSSIAN: // Gaussian pulse - smooth bell curve
            {
                float centered = fmod(arg, 2.0f * PI) - PI;
                return exp(-4.0f * centered * centered / (PI * PI));
            }
            
        case WAVE_DAMPED: // Damped sine - decaying oscillation
            {
                float envelope = exp(-0.1f * abs(arg));
                return sin(arg) * envelope;
            }
            
        default:
            return sin(arg);
    }
}

/**
 * Calculate distance from center point (for center-origin effects)
 * @param led_index: LED position (0 to STRIP_LENGTH-1)
 * @return: Normalized distance from center (0.0 at center, 1.0 at edges)
 */
inline float distanceFromCenter(uint16_t led_index) {
    return abs((float)led_index - HardwareConfig::STRIP_CENTER_POINT) / 
           HardwareConfig::STRIP_HALF_LENGTH;
}

/**
 * Calculate true wave interference using complex wave superposition
 * @param pos1: Position on strip1 (0.0 - 1.0)
 * @param pos2: Position on strip2 (0.0 - 1.0)  
 * @param engine: Wave engine parameters
 * @return: Color representing interference intensity and phase
 */
CRGB calculateInterference(float pos1, float pos2, DualStripWaveEngine& engine) {
    // Generate wave amplitudes at current positions
    float wave1 = generateWave(pos1, engine.strip1_frequency, 
                              engine.strip1_phase, engine.wave_type);
    float wave2 = generateWave(pos2, engine.strip2_frequency, 
                              engine.strip2_phase, engine.wave_type);
    
    // Calculate path difference for true interference
    float path_difference = abs(pos1 - pos2);
    float phase_difference = 2.0f * PI * path_difference / engine.wave_speed;
    phase_difference += engine.manual_phase_offset;
    
    // Complex wave superposition: |A₁e^(iφ₁) + A₂e^(iφ₂)|²
    float real_component = wave1 + wave2 * cos(phase_difference);
    float imaginary_component = wave2 * sin(phase_difference);
    float interference_intensity = sqrt(real_component * real_component + 
                                       imaginary_component * imaginary_component);
    
    // Apply amplitude scaling
    interference_intensity *= engine.amplitude;
    
    // Map interference intensity and phase to color
    uint8_t hue = (uint8_t)((phase_difference * 255.0f / (2.0f * PI)) + gHue);
    uint8_t brightness = constrain(interference_intensity * 128.0f, 0, 255);
    uint8_t saturation = 255; // Full saturation for vivid interference patterns
    
    return CHSV(hue, saturation, brightness);
}

/**
 * Calculate beat frequency and enhance visualization
 * @param engine: Wave engine parameters
 */
inline void updateBeatFrequency(DualStripWaveEngine& engine) {
    engine.beat_frequency = abs(engine.strip1_frequency - engine.strip2_frequency);
    
    // Enhance beat visualization when frequencies are close
    if (engine.beat_frequency < 0.5f && engine.beat_frequency > 0.01f) {
        float beat_phase = engine.time_accumulator * engine.beat_frequency * 2.0f * PI;
        engine.beat_enhancement = 1.0f + sin(beat_phase) * 0.4f;
    } else {
        engine.beat_enhancement = 1.0f;
    }
}

// ==================== HELPER FUNCTIONS ====================

/**
 * Get human-readable wave type name
 */
inline const char* getWaveTypeName(uint8_t type) {
    const char* names[] = {"Sine", "Triangle", "Sawtooth", "Gaussian", "Damped"};
    return (type < 5) ? names[type] : "Unknown";
}

/**
 * Get human-readable interaction mode name
 */
inline const char* getInteractionModeName(uint8_t mode) {
    const char* names[] = {"Independent", "Interference", "Chase", "Reflection", "Spiral", "Pulse"};
    return (mode < 6) ? names[mode] : "Unknown";
}

#endif // DUAL_STRIP_WAVE_ENGINE_H