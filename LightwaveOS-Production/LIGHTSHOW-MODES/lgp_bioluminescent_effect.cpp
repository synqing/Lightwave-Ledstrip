#include "lgp_bioluminescent_effect.h"
#include "../src/globals.h"
#include "../src/GDFT.h"

#define MAX_ORGANISMS 8

struct Organism {
    SQ15x16 position;
    SQ15x16 phase;
    uint8_t size;
    uint8_t hue;
    uint8_t pulse_rate;
    bool active;
};

static Organism organisms[MAX_ORGANISMS];
static uint32_t last_spawn = 0;

void light_mode_lgp_bioluminescent() {
    cache_frame_config();
    
    // Audio integration for bioluminescent behavior
    get_smooth_spectrogram();
    get_smooth_chromagram();
    calculate_vu();
    
    // Calculate audio energy for organism responses
    float total_energy = 0;
    for(int i = 0; i < 96; i++) {
        total_energy += spectrogram_smooth[i];
    }
    
    // Low frequency energy triggers new organisms (like footsteps)
    float low_freq_energy = 0;
    for(int i = 0; i < 15; i++) {
        low_freq_energy += spectrogram_smooth[i];
    }
    
    // Mid frequency energy affects organism brightness
    float mid_freq_energy = 0;
    for(int i = 15; i < 40; i++) {
        mid_freq_energy += spectrogram_smooth[i];
    }
    
    // High frequency energy affects organism color shifts
    float high_freq_energy = 0;
    for(int i = 40; i < 96; i++) {
        high_freq_energy += spectrogram_smooth[i];
    }
    
    // Beat detection for sudden organism spawning
    static float last_total_energy = 0;
    float energy_delta = total_energy - last_total_energy;
    bool audio_trigger = (energy_delta > 0.2f && total_energy > 0.3f);
    last_total_energy = total_energy * 0.8f + last_total_energy * 0.2f;
    
    // Slow fade for organic trails
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        leds_16[i] = scale_color(leds_16[i], SQ15x16(0.93));
    }
    
    // Spawn new organisms - audio reactive spawning
    uint32_t now = millis();
    uint32_t base_interval = 2000 - (frame_config.SPEED * 7);
    
    // Reduce interval with low frequency energy (like footsteps)
    uint32_t spawn_interval = base_interval * (1.0f - low_freq_energy * 0.7f);
    if(spawn_interval < 500) spawn_interval = 500;
    
    // Increase spawn probability with audio activity
    uint8_t spawn_prob = 100 + (uint8_t)(total_energy * 155);
    
    if ((now - last_spawn > spawn_interval && random8() < spawn_prob) || audio_trigger) {
        for (int o = 0; o < MAX_ORGANISMS; o++) {
            if (!organisms[o].active) {
                organisms[o].position = random16(NATIVE_RESOLUTION);
                organisms[o].phase = 0;
                
                // Audio reactive organism properties
                organisms[o].size = 3 + random8(8) + (uint8_t)(total_energy * 10);
                if(organisms[o].size > 20) organisms[o].size = 20;
                
                organisms[o].pulse_rate = 20 + random8(40) + (uint8_t)(mid_freq_energy * 80);
                if(organisms[o].pulse_rate > 150) organisms[o].pulse_rate = 150;
                
                organisms[o].active = true;
                
                // Bioluminescent colors - audio reactive
                if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
                    // Map high frequency energy to palette position
                    uint8_t palette_shift = (uint8_t)(high_freq_energy * 127);
                    organisms[o].hue = random8() + palette_shift;
                } else {
                    uint8_t color_type = random8(4);
                    // High frequency energy shifts toward warmer colors
                    uint8_t freq_shift = (uint8_t)(high_freq_energy * 40);
                    
                    switch (color_type) {
                        case 0: organisms[o].hue = 120 + freq_shift; break;  // Cyan -> Green
                        case 1: organisms[o].hue = 96 + freq_shift; break;   // Green -> Yellow
                        case 2: organisms[o].hue = 160 + freq_shift; break;  // Blue -> Cyan
                        default: organisms[o].hue = 75 + freq_shift; break;  // Yellow-green -> Yellow
                    }
                }
                
                last_spawn = now;
                break;
            }
        }
    }
    
    // Update and render organisms
    for (int o = 0; o < MAX_ORGANISMS; o++) {
        if (!organisms[o].active) continue;
        
        // Update phase for pulsing
        organisms[o].phase += SQ15x16(organisms[o].pulse_rate) / 255;
        
        // Slow drift movement
        SQ15x16 drift = sin_lookup((organisms[o].phase / 4).getInteger() & 0xFF) * SQ15x16(0.3);
        organisms[o].position += drift;
        
        // Wrap around
        if (organisms[o].position < 0) organisms[o].position += NATIVE_RESOLUTION;
        if (organisms[o].position >= NATIVE_RESOLUTION) organisms[o].position -= NATIVE_RESOLUTION;
        
        // Calculate pulse brightness - audio reactive
        uint8_t base_pulse = 100 + (sin_lookup(organisms[o].phase.getInteger() & 0xFF) * 155).getInteger();
        
        // Boost pulse with mid frequency energy
        uint8_t pulse = base_pulse + (uint8_t)(mid_freq_energy * 100);
        if(pulse > 255) pulse = 255;
        
        // Occasional bright flash - audio reactive probability
        uint8_t flash_prob = 2 + (uint8_t)(total_energy * 20);
        if (random8() < flash_prob || audio_trigger) {
            pulse = 255;
        }
        
        // Deactivate old organisms - slower aging with audio activity
        float age_threshold = 100 + (total_energy * 50);  // Live longer with audio
        if (organisms[o].phase > age_threshold) {
            organisms[o].active = false;
            continue;
        }
        
        // Render organism with gaussian-like glow
        for (int i = 0; i < NATIVE_RESOLUTION; i++) {
            SQ15x16 dist = abs(SQ15x16(i) - organisms[o].position);
            
            // Wrap distance for edge cases
            if (dist > NATIVE_RESOLUTION/2) {
                dist = NATIVE_RESOLUTION - dist;
            }
            
            if (dist < organisms[o].size * 2) {
                // Gaussian falloff
                SQ15x16 norm_dist = dist / organisms[o].size;
                SQ15x16 gaussian = SQ15x16(1) - (norm_dist * norm_dist);
                
                if (gaussian > 0) {
                    uint8_t brightness = (gaussian * pulse).getInteger();
                    
                    // Add slight color variation within organism - audio reactive
                    uint8_t hue_var = sin8((i * 10) + organisms[o].phase.getInteger()) / 32;
                    
                    // High frequency energy increases color variation
                    hue_var += (uint8_t)(high_freq_energy * 20);
                    
                    CRGB16 color;
                    if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
                        color = palette_to_crgb16(palette_arr[frame_config.PALETTE],
                                                organisms[o].hue + hue_var, brightness);
                    } else {
                        // Reduce saturation for more realistic bioluminescence
                        uint8_t sat = scale8(frame_config.SATURATION, 200);
                        color = hsv_to_rgb_fast(organisms[o].hue + hue_var, sat, brightness);
                    }
                    
                    // Core is brighter and whiter
                    if (dist < organisms[o].size / 2) {
                        uint8_t core_white = 128 - (dist * 256 / organisms[o].size).getInteger();
                        CRGB16 white = CRGB16(core_white * 256, core_white * 256, core_white * 256);
                        color = add_clipped(color, white);
                    }
                    
                    leds_16[i] = add_clipped(leds_16[i], color);
                }
            }
        }
    }
    
    // Add random sparkles for plankton effect - audio reactive
    if (frame_config.DENSITY > 128) {
        for (int i = 0; i < NATIVE_RESOLUTION; i++) {
            uint8_t sparkle_prob = (frame_config.DENSITY - 128) / 32;
            
            // Increase sparkle probability with high frequency energy
            sparkle_prob += (uint8_t)(high_freq_energy * 50);
            
            if (random8() < sparkle_prob) {
                uint8_t sparkle = random8(100, 200) + (uint8_t)(total_energy * 55);
                if(sparkle > 255) sparkle = 255;
                
                // Audio reactive sparkle colors
                uint8_t sparkle_hue = 120 + random8(40) + (uint8_t)(high_freq_energy * 60);
                uint8_t sparkle_sat = 150 + (uint8_t)(mid_freq_energy * 80);
                if(sparkle_sat > 255) sparkle_sat = 255;
                
                CRGB16 sparkle_color = hsv_to_rgb_fast(sparkle_hue, sparkle_sat, sparkle);
                leds_16[i] = add_clipped(leds_16[i], sparkle_color);
            }
        }
    }
    
    apply_global_brightness();
}