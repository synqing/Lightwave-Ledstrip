#include "lgp_metamaterial_cloak_effect.h"

// Cloaked object state
struct CloakedObject {
    SQ15x16 position;
    SQ15x16 size;
    SQ15x16 cloak_efficiency;
    SQ15x16 phase_shift;
};

static CloakedObject cloak;
static SQ15x16 background_phase = 0;
static bool initialized = false;

void light_mode_lgp_metamaterial_cloak() {
    cache_frame_config();
    
    // Initialize cloaked object
    if (!initialized) {
        cloak.position = NATIVE_RESOLUTION / 2;
        cloak.size = SQ15x16(15);
        cloak.cloak_efficiency = SQ15x16(0.9);
        cloak.phase_shift = 0;
        initialized = true;
    }
    
    // Update cloak parameters
    SQ15x16 speed = SQ15x16(frame_config.SPEED) / 128;
    
    // Move cloaked object
    cloak.position = SQ15x16(NATIVE_RESOLUTION/2) + 
                    sin_lookup((millis() / 50) & 0xFF) * SQ15x16(NATIVE_RESOLUTION/4);
    
    // Cloak efficiency varies with density
    cloak.cloak_efficiency = SQ15x16(frame_config.DENSITY) / 255;
    
    // Size pulses slightly
    cloak.size = SQ15x16(10) + sin_lookup((millis() / 30) & 0xFF) * SQ15x16(5);
    
    // Update background and phase
    background_phase += speed;
    cloak.phase_shift += speed * SQ15x16(2);
    
    // Render effect
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        SQ15x16 pos = SQ15x16(i);
        
        // Distance from cloaked object center
        SQ15x16 dist = abs(pos - cloak.position);
        
        // Check if we're in the cloaking field
        if (dist < cloak.size * SQ15x16(2)) {
            // Calculate refraction angle based on metamaterial properties
            SQ15x16 cloak_factor = SQ15x16(1) - (dist / (cloak.size * SQ15x16(2)));
            cloak_factor = cloak_factor * cloak.cloak_efficiency;
            
            // Light bending - sample from opposite side
            SQ15x16 bend_distance = (cloak.size * SQ15x16(2) - dist) * cloak_factor;
            SQ15x16 source_pos;
            
            if (pos < cloak.position) {
                // Light bends around from right side
                source_pos = cloak.position + (cloak.position - pos) + bend_distance;
            } else {
                // Light bends around from left side
                source_pos = cloak.position - (pos - cloak.position) - bend_distance;
            }
            
            // Ensure source is in bounds
            while (source_pos < 0) source_pos += NATIVE_RESOLUTION;
            while (source_pos >= NATIVE_RESOLUTION) source_pos -= NATIVE_RESOLUTION;
            
            // Sample background pattern from bent position
            uint16_t background = inoise16(source_pos.getInteger() * 50, 
                                         background_phase.getInteger());
            uint8_t brightness = background >> 8;
            
            // Inside the cloak - mostly invisible
            if (dist < cloak.size) {
                // Imperfect cloaking - slight shimmer
                brightness = brightness * (SQ15x16(1) - cloak.cloak_efficiency).getInteger() / 255;
                
                // Add phase distortion at cloak boundary
                if (dist > cloak.size * SQ15x16(0.8)) {
                    uint8_t boundary_shimmer = sin8((i * 20 + cloak.phase_shift.getInteger()) & 0xFF);
                    brightness = qadd8(brightness, scale8(boundary_shimmer, 30));
                }
            }
            
            // Color with metamaterial interference
            CRGB16 color;
            if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
                uint8_t pal_index = brightness + (cloak_factor * 50).getInteger();
                color = palette_to_crgb16(palette_arr[frame_config.PALETTE],
                                        pal_index, brightness);
            } else {
                // Slight color shift due to metamaterial dispersion
                uint8_t hue = frame_config.HUE;
                if (cloak_factor > 0) {
                    hue += (cloak_factor * 20).getInteger();
                }
                color = hsv_to_rgb_fast(hue, frame_config.SATURATION, brightness);
            }
            
            leds_16[i] = color;
            
        } else {
            // Normal background pattern
            uint16_t background = inoise16(i * 50, background_phase.getInteger());
            uint8_t brightness = background >> 8;
            
            CRGB16 color;
            if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
                color = palette_to_crgb16(palette_arr[frame_config.PALETTE],
                                        brightness, brightness);
            } else {
                color = hsv_to_rgb_fast(frame_config.HUE, frame_config.SATURATION, brightness);
            }
            
            leds_16[i] = color;
        }
    }
    
    // Add edge detection shimmer for high density
    if (frame_config.DENSITY > 200) {
        for (int i = 1; i < NATIVE_RESOLUTION - 1; i++) {
            // Edge detection
            int16_t edge = (leds_16[i-1].r + leds_16[i-1].g + leds_16[i-1].b) -
                          (leds_16[i+1].r + leds_16[i+1].g + leds_16[i+1].b);
            
            if (abs(edge) > 10000) {
                // Add shimmer at edges
                CRGB16 shimmer = CRGB16(5000, 5000, 8000);
                leds_16[i] = add_clipped(leds_16[i], shimmer);
            }
        }
    }
    
    // Beat detection creates electromagnetic pulse effect
    if(beat_detected) {
        int cloak_pos = cloak.position.getInteger();
        for(int i = 0; i < NATIVE_RESOLUTION; i++) {
            SQ15x16 dist = abs(SQ15x16(i) - SQ15x16(cloak_pos));
            if(dist < SQ15x16(20)) {
                uint8_t pulse_intensity = 255 - (dist * 12).getInteger();
                CRGB16 pulse_color = CRGB16(pulse_intensity * 100, pulse_intensity * 150, pulse_intensity * 200);
                leds_16[i] = add_clipped(leds_16[i], pulse_color);
            }
        }
    }
    
    apply_global_brightness();
}