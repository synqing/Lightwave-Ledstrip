#include "lgp_quantum_tunneling_effect.h"

// Quantum particle state
struct QuantumParticle {
    SQ15x16 position;
    SQ15x16 momentum;
    SQ15x16 wave_packet_width;
    uint8_t energy_level;
    bool tunneling;
};

static QuantumParticle particle;
static SQ15x16 probability_phase = 0;
static bool initialized = false;

// Potential barrier parameters
static const int BARRIER_START = NATIVE_RESOLUTION * 2 / 5;
static const int BARRIER_END = NATIVE_RESOLUTION * 3 / 5;
static const int BARRIER_HEIGHT = 200;  // Energy units

void light_mode_lgp_quantum_tunneling() {
    cache_frame_config();
    
    // Initialize particle
    if (!initialized) {
        particle.position = 10;
        particle.momentum = SQ15x16(0.5);
        particle.wave_packet_width = SQ15x16(20);
        particle.energy_level = 100;
        particle.tunneling = false;
        initialized = true;
    }
    
    // Update quantum state
    SQ15x16 speed = SQ15x16(frame_config.SPEED) / 128;
    particle.position += particle.momentum * speed;
    probability_phase += speed * SQ15x16(3);
    
    // Energy level based on density
    particle.energy_level = 50 + (frame_config.DENSITY / 2);
    
    // Check for barrier interaction
    int pos = particle.position.getInteger();
    if (pos >= BARRIER_START && pos <= BARRIER_END) {
        if (!particle.tunneling) {
            // Quantum tunneling probability
            float tunneling_prob = (float)particle.energy_level / BARRIER_HEIGHT;
            tunneling_prob = tunneling_prob * tunneling_prob;  // Square for quantum mechanics
            
            if (random8() < tunneling_prob * 255) {
                // Successful tunneling!
                particle.tunneling = true;
            } else {
                // Reflection
                particle.momentum = -particle.momentum * SQ15x16(0.8);
                particle.tunneling = false;
            }
        }
    } else {
        particle.tunneling = false;
    }
    
    // Reset particle at edges
    if (particle.position < 0 || particle.position > NATIVE_RESOLUTION) {
        particle.position = 10;
        particle.momentum = SQ15x16(0.5);
        particle.tunneling = false;
    }
    
    // Clear strip
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        leds_16[i] = CRGB16(0, 0, 0);
    }
    
    // Render quantum wave function
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        // Wave function amplitude (Gaussian wave packet)
        SQ15x16 distance = abs(SQ15x16(i) - particle.position);
        SQ15x16 gaussian = exp_approx(-(distance * distance) / (particle.wave_packet_width * particle.wave_packet_width));
        
        // Add wave nature (de Broglie waves)
        SQ15x16 wave_component = sin_lookup((i * 10 + probability_phase.getInteger()) & 0xFF);
        SQ15x16 amplitude = gaussian * (SQ15x16(0.5) + wave_component * SQ15x16(0.5));
        
        // Probability density (amplitude squared)
        SQ15x16 probability = amplitude * amplitude;
        uint8_t brightness = (probability * 255).getInteger();
        
        // Inside barrier - reduced probability unless tunneling
        if (i >= BARRIER_START && i <= BARRIER_END) {
            if (!particle.tunneling) {
                // Exponential decay inside barrier
                float barrier_depth = (float)(i - BARRIER_START) / (BARRIER_END - BARRIER_START);
                float decay = exp(-barrier_depth * 3);
                brightness = brightness * decay;
            }
        }
        
        // Color based on quantum state
        CRGB16 color;
        if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
            uint8_t pal_index = (probability_phase + i).getInteger() & 0xFF;
            color = palette_to_crgb16(palette_arr[frame_config.PALETTE],
                                    pal_index, brightness);
        } else {
            // Phase-based color
            uint8_t phase_hue = (wave_component * 30 + SQ15x16(frame_config.HUE)).getInteger() & 0xFF;
            color = hsv_to_rgb_fast(phase_hue, frame_config.SATURATION, brightness);
        }
        
        leds_16[i] = color;
    }
    
    // Draw potential barrier
    for (int i = BARRIER_START; i <= BARRIER_END; i++) {
        // Barrier visualization
        uint8_t barrier_bright = 80 - abs(i - (BARRIER_START + BARRIER_END)/2) * 2;
        CRGB16 barrier_color;
        
        if (particle.energy_level < BARRIER_HEIGHT) {
            // Red barrier (classically forbidden)
            barrier_color = CRGB16(barrier_bright * 256, 0, 0);
        } else {
            // Green barrier (classically allowed)
            barrier_color = CRGB16(0, barrier_bright * 256, 0);
        }
        
        leds_16[i] = add_clipped(leds_16[i], barrier_color);
    }
    
    // Tunneling flash effect
    if (particle.tunneling && pos >= BARRIER_START && pos <= BARRIER_END) {
        for (int i = max(0, pos - 5); i < min(NATIVE_RESOLUTION, pos + 5); i++) {
            CRGB16 flash = CRGB16(30000, 30000, 50000);  // Bright blue-white
            leds_16[i] = add_clipped(leds_16[i], flash);
        }
    }
    
    apply_global_brightness();
}