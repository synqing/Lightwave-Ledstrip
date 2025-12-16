#ifndef WAVE_EFFECTS_H
#define WAVE_EFFECTS_H

#include "DualStripWaveEngine.h"

// ==================== CORE RENDERING FUNCTIONS ====================

/**
 * Mode 0: Independent wave operation on each strip
 * Each strip displays its own wave pattern with different frequencies
 */
void renderIndependentWaves(DualStripWaveEngine& engine) {
    for(uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float position = (float)i / HardwareConfig::STRIP_LENGTH;
        
        // Generate independent waves for each strip
        float wave1 = generateWave(position, engine.strip1_frequency, 
                                  engine.strip1_phase, engine.wave_type);
        float wave2 = generateWave(position, engine.strip2_frequency, 
                                  engine.strip2_phase, engine.wave_type);
        
        // Apply amplitude and beat enhancement
        wave1 *= engine.amplitude * engine.beat_enhancement;
        wave2 *= engine.amplitude * engine.beat_enhancement;
        
        // Convert to colors (different hue ranges for distinction)
        uint8_t brightness1 = constrain((wave1 + 1.0f) * 127.5f, 0, 255);
        uint8_t brightness2 = constrain((wave2 + 1.0f) * 127.5f, 0, 255);
        
        strip1[i] = CHSV(gHue, 255, brightness1);
        strip2[i] = CHSV(gHue + 60, 255, brightness2); // 60° hue offset
    }
}

/**
 * Mode 1: True interference pattern calculation
 * Demonstrates real wave physics with constructive/destructive interference
 */
void renderInterferencePattern(DualStripWaveEngine& engine) {
    for(uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float position = (float)i / HardwareConfig::STRIP_LENGTH;
        
        // Calculate interference at this position
        CRGB interference_color = calculateInterference(position, position, engine);
        
        // Apply beat enhancement to interference intensity
        if (engine.beat_enhancement != 1.0f) {
            interference_color.fadeToBlackBy(255 - (uint8_t)(engine.beat_enhancement * 255));
        }
        
        // Set same color on both strips for interference visualization
        strip1[i] = interference_color;
        strip2[i] = interference_color;
    }
}

/**
 * Mode 2: Wave chase - waves travel between strips
 * Creates the illusion of energy transfer from strip1 to strip2
 */
void renderWaveChase(DualStripWaveEngine& engine) {
    // Wave travels in 2-second cycles
    float cycle_time = fmod(engine.time_accumulator, 2.0f);
    float wave_position = cycle_time / 2.0f; // 0.0 to 1.0
    
    for(uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float led_position = (float)i / HardwareConfig::STRIP_LENGTH;
        
        if (cycle_time < 1.0f) {
            // First half: wave on strip1, moving toward center
            float distance_from_wave = abs(led_position - wave_position);
            if (distance_from_wave < 0.1f) { // Wave width
                float intensity = (1.0f - distance_from_wave / 0.1f) * engine.amplitude;
                uint8_t brightness = constrain(intensity * 255, 0, 255);
                strip1[i] = CHSV(gHue, 255, brightness);
            }
        } else {
            // Second half: wave on strip2, moving away from center
            float strip2_position = wave_position - 1.0f;
            float distance_from_wave = abs(led_position - strip2_position);
            if (distance_from_wave < 0.1f) {
                float intensity = (1.0f - distance_from_wave / 0.1f) * engine.amplitude;
                uint8_t brightness = constrain(intensity * 255, 0, 255);
                strip2[i] = CHSV(gHue + 80, 255, brightness);
            }
        }
    }
}

/**
 * Mode 3: Wave reflection from center point
 * Waves propagate from edges and reflect off the center (LEDs 79/80)
 */
void renderWaveReflection(DualStripWaveEngine& engine) {
    for(uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float dist_from_center = distanceFromCenter(i);
        
        // Generate outgoing wave (from center to edges)
        float outgoing_wave = generateWave(dist_from_center, engine.strip1_frequency,
                                         engine.strip1_phase, engine.wave_type);
        
        // Generate reflected wave (from edges back to center)
        float reflected_wave = generateWave(1.0f - dist_from_center, engine.strip2_frequency,
                                          engine.strip2_phase + PI, engine.wave_type);
        
        // Combine outgoing and reflected waves
        float combined_intensity = (outgoing_wave + reflected_wave * 0.7f) * engine.amplitude;
        uint8_t brightness = constrain((combined_intensity + 1.0f) * 127.5f, 0, 255);
        
        // Color varies with distance from center
        uint8_t hue = gHue + (uint8_t)(dist_from_center * 120);
        CRGB color = CHSV(hue, 255, brightness);
        
        strip1[i] = strip2[i] = color;
    }
}

/**
 * Mode 4: Spiral wave propagation
 * Phase rotates around center point creating spiral patterns
 */
void renderSpiralWaves(DualStripWaveEngine& engine) {
    for(uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float position = (float)i / HardwareConfig::STRIP_LENGTH;
        float dist_from_center = distanceFromCenter(i);
        
        // Calculate spiral phase - phase advances with distance and time
        float spiral_phase = engine.time_accumulator * 2.0f + dist_from_center * 8.0f * PI;
        
        // Generate wave with spiral phase modulation
        float wave_value = sin(2.0f * PI * engine.strip1_frequency * position + spiral_phase);
        wave_value *= engine.amplitude;
        
        // Intensity falls off with distance from center
        float intensity = wave_value * (1.0f - dist_from_center * 0.5f);
        uint8_t brightness = constrain((intensity + 1.0f) * 127.5f, 0, 255);
        
        // Hue rotates with spiral phase
        uint8_t hue = gHue + (uint8_t)(spiral_phase * 255.0f / (2.0f * PI));
        CRGB color = CHSV(hue, 255, brightness);
        
        strip1[i] = strip2[i] = color;
    }
}

/**
 * Mode 5: Synchronized pulse bursts
 * Coordinated pulses emanate from both edges toward center
 */
void renderPulseMode(DualStripWaveEngine& engine) {
    // Generate pulse every 1 second
    float pulse_cycle = fmod(engine.time_accumulator, 1.0f);
    float pulse_position = pulse_cycle; // 0.0 to 1.0
    
    for(uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float position = (float)i / HardwareConfig::STRIP_LENGTH;
        
        // Calculate distance from both edges
        float dist_from_left = position;
        float dist_from_right = 1.0f - position;
        
        // Pulse from left edge
        float left_pulse_distance = abs(dist_from_left - pulse_position);
        float left_intensity = 0.0f;
        if (left_pulse_distance < 0.15f) { // Pulse width
            left_intensity = (1.0f - left_pulse_distance / 0.15f) * engine.amplitude;
        }
        
        // Pulse from right edge  
        float right_pulse_distance = abs(dist_from_right - pulse_position);
        float right_intensity = 0.0f;
        if (right_pulse_distance < 0.15f) {
            right_intensity = (1.0f - right_pulse_distance / 0.15f) * engine.amplitude;
        }
        
        // Combine pulses (interference when they meet)
        float total_intensity = left_intensity + right_intensity;
        if (left_intensity > 0.1f && right_intensity > 0.1f) {
            // Interference enhancement when pulses overlap
            total_intensity *= 1.5f;
        }
        
        uint8_t brightness = constrain(total_intensity * 255, 0, 255);
        uint8_t hue = gHue + (uint8_t)(position * 90); // Color gradient across strip
        
        CRGB color = CHSV(hue, 255, brightness);
        strip1[i] = strip2[i] = color;
    }
}

/**
 * Main rendering function - dispatches to specific interaction modes
 * @param engine: Wave engine parameters
 */
void renderDualStripWaves(DualStripWaveEngine& engine) {
    uint32_t start_time = micros();
    
    // Update time accumulator
    uint32_t now = millis();
    float delta_time = (now - engine.last_update) * 0.001f; // Convert to seconds
    engine.last_update = now;
    engine.time_accumulator += delta_time * engine.wave_speed;
    
    // Update wave phases
    engine.strip1_phase += engine.strip1_frequency * delta_time * 2.0f * PI;
    engine.strip2_phase += engine.strip2_frequency * delta_time * 2.0f * PI;
    
    // Keep phases in valid range
    engine.strip1_phase = fmod(engine.strip1_phase, 2.0f * PI);
    engine.strip2_phase = fmod(engine.strip2_phase, 2.0f * PI);
    
    // Calculate beat frequency effects
    updateBeatFrequency(engine);
    
    // Clear strips with fading
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 20);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 20);
    
    // Dispatch to appropriate rendering mode
    switch(engine.interaction_mode) {
        case MODE_INDEPENDENT:
            renderIndependentWaves(engine);
            break;
            
        case MODE_INTERFERENCE:
            renderInterferencePattern(engine);
            break;
            
        case MODE_CHASE:
            renderWaveChase(engine);
            break;
            
        case MODE_REFLECTION:
            renderWaveReflection(engine);
            break;
            
        case MODE_SPIRAL:
            renderSpiralWaves(engine);
            break;
            
        case MODE_PULSE:
            renderPulseMode(engine);
            break;
            
        default:
            renderIndependentWaves(engine);
            break;
    }
    
    // Update performance monitoring
    engine.render_time_us = micros() - start_time;
    engine.frame_count++;
}

#endif // WAVE_EFFECTS_H