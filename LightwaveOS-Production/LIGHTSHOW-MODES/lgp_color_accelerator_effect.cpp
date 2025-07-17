#include "lgp_color_accelerator_effect.h"
#include "../src/globals.h"
#include "../src/GDFT.h"

#define MAX_COLOR_PARTICLES 6

struct ColorParticle {
    SQ15x16 position;
    SQ15x16 velocity;
    uint8_t hue;
    uint8_t saturation;
    uint8_t brightness;
    uint8_t size;
    bool active;
    bool from_left;
};

struct ColorExplosion {
    SQ15x16 position;
    SQ15x16 radius;
    uint8_t hue;
    uint8_t brightness;
    uint8_t age;
    bool active;
};

static ColorParticle particles[MAX_COLOR_PARTICLES];
static ColorExplosion explosions[MAX_COLOR_PARTICLES];
static uint32_t last_launch_time = 0;

void light_mode_lgp_color_accelerator() {
    cache_frame_config();
    
    // Audio integration for color accelerator dynamics
    get_smooth_spectrogram();
    get_smooth_chromagram();
    calculate_vu();
    
    // Calculate audio energy for particle behavior
    float total_energy = 0;
    for(int i = 0; i < 96; i++) {
        total_energy += spectrogram_smooth[i];
    }
    
    // Low frequency energy affects particle launch rate
    float low_freq_energy = 0;
    for(int i = 0; i < 20; i++) {
        low_freq_energy += spectrogram_smooth[i];
    }
    
    // Mid frequency energy affects particle speed
    float mid_freq_energy = 0;
    for(int i = 20; i < 50; i++) {
        mid_freq_energy += spectrogram_smooth[i];
    }
    
    // High frequency energy affects collision probability
    float high_freq_energy = 0;
    for(int i = 50; i < 96; i++) {
        high_freq_energy += spectrogram_smooth[i];
    }
    
    // Beat detection for particle injection
    static float last_total_energy = 0;
    float energy_delta = total_energy - last_total_energy;
    bool beat_detected = (energy_delta > 0.2f && total_energy > 0.3f);
    last_total_energy = total_energy * 0.9f + last_total_energy * 0.1f;
    
    // Fade existing colors
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        leds_16[i] = scale_color(leds_16[i], SQ15x16(0.92));
    }
    
    // Launch new color particles - audio reactive
    uint32_t launch_interval = 1500 - (frame_config.SPEED * 5);
    // Reduce interval with low frequency energy
    launch_interval = launch_interval * (1.0f - low_freq_energy * 0.6f);
    if(launch_interval < 300) launch_interval = 300;
    
    uint32_t now = millis();
    
    if ((now - last_launch_time > launch_interval) || beat_detected) {
        // Launch particles from both sides
        for (int p = 0; p < MAX_COLOR_PARTICLES; p += 2) {
            if (!particles[p].active) {
                // Left side particle
                particles[p].position = SQ15x16(random8(20));
                particles[p].velocity = SQ15x16(0.3f + mid_freq_energy * 0.8f);
                particles[p].from_left = true;
                particles[p].active = true;
                particles[p].size = 8 + random8(12) + (uint8_t)(total_energy * 15);
                if(particles[p].size > 30) particles[p].size = 30;
                
                // Audio reactive color selection
                if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
                    particles[p].hue = random8();
                } else {
                    // Primary colors for particle physics
                    uint8_t color_choice = random8(3);
                    switch(color_choice) {
                        case 0: particles[p].hue = 0; break;   // Red
                        case 1: particles[p].hue = 85; break;  // Green
                        case 2: particles[p].hue = 170; break; // Blue
                    }
                    // High frequency shifts toward secondary colors
                    particles[p].hue += (uint8_t)(high_freq_energy * 42);
                }
                
                particles[p].saturation = 255;
                particles[p].brightness = 200 + (uint8_t)(total_energy * 55);
                if(particles[p].brightness > 255) particles[p].brightness = 255;
                
                last_launch_time = now;
                break;
            }
            
            if (p + 1 < MAX_COLOR_PARTICLES && !particles[p + 1].active) {
                // Right side particle
                particles[p + 1].position = SQ15x16(NATIVE_RESOLUTION - 1 - random8(20));
                particles[p + 1].velocity = SQ15x16(-0.3f - mid_freq_energy * 0.8f);
                particles[p + 1].from_left = false;
                particles[p + 1].active = true;
                particles[p + 1].size = 8 + random8(12) + (uint8_t)(total_energy * 15);
                if(particles[p + 1].size > 30) particles[p + 1].size = 30;
                
                // Audio reactive color selection
                if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
                    particles[p + 1].hue = random8();
                } else {
                    // Primary colors for particle physics
                    uint8_t color_choice = random8(3);
                    switch(color_choice) {
                        case 0: particles[p + 1].hue = 0; break;   // Red
                        case 1: particles[p + 1].hue = 85; break;  // Green
                        case 2: particles[p + 1].hue = 170; break; // Blue
                    }
                    // High frequency shifts toward secondary colors
                    particles[p + 1].hue += (uint8_t)(high_freq_energy * 42);
                }
                
                particles[p + 1].saturation = 255;
                particles[p + 1].brightness = 200 + (uint8_t)(total_energy * 55);
                if(particles[p + 1].brightness > 255) particles[p + 1].brightness = 255;
                
                break;
            }
        }
    }
    
    // Update particles and check for collisions
    for (int p = 0; p < MAX_COLOR_PARTICLES; p++) {
        if (!particles[p].active) continue;
        
        // Audio reactive acceleration
        SQ15x16 accel_factor = SQ15x16(1) + SQ15x16(mid_freq_energy * 0.5f);
        particles[p].velocity = particles[p].velocity * accel_factor;
        
        // Update position
        particles[p].position += particles[p].velocity;
        
        // Check bounds
        if (particles[p].position < 0 || particles[p].position >= NATIVE_RESOLUTION) {
            particles[p].active = false;
            continue;
        }
        
        // Check for collisions with other particles
        for (int other = p + 1; other < MAX_COLOR_PARTICLES; other++) {
            if (!particles[other].active) continue;
            
            SQ15x16 distance = abs(particles[p].position - particles[other].position);
            float collision_threshold = 8.0f + high_freq_energy * 15.0f;
            
            if (distance < collision_threshold) {
                // Color collision! Create explosion
                for (int e = 0; e < MAX_COLOR_PARTICLES; e++) {
                    if (!explosions[e].active) {
                        explosions[e].position = (particles[p].position + particles[other].position) / 2;
                        explosions[e].radius = 0;
                        explosions[e].age = 0;
                        explosions[e].active = true;
                        
                        // Color mixing - additive color model
                        uint8_t mixed_hue;
                        if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
                            mixed_hue = (particles[p].hue + particles[other].hue) / 2;
                        } else {
                            // Physics-based color mixing
                            int16_t hue_diff = particles[other].hue - particles[p].hue;
                            if (hue_diff > 128) hue_diff -= 256;
                            if (hue_diff < -128) hue_diff += 256;
                            mixed_hue = particles[p].hue + hue_diff / 2;
                        }
                        
                        explosions[e].hue = mixed_hue;
                        explosions[e].brightness = 255;
                        
                        // Deactivate collided particles
                        particles[p].active = false;
                        particles[other].active = false;
                        
                        break;
                    }
                }
            }
        }
        
        // Render particle
        if (particles[p].active) {
            int pos = particles[p].position.getInteger();
            if (pos >= 0 && pos < NATIVE_RESOLUTION) {
                // Particle core
                CRGB16 particle_color;
                if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
                    particle_color = palette_to_crgb16(palette_arr[frame_config.PALETTE],
                                                     particles[p].hue, particles[p].brightness);
                } else {
                    particle_color = hsv_to_rgb_fast(particles[p].hue, 
                                                   particles[p].saturation, 
                                                   particles[p].brightness);
                }
                
                leds_16[pos] = add_clipped(leds_16[pos], particle_color);
                
                // Particle trail
                int trail_length = particles[p].size / 4;
                for (int t = 1; t <= trail_length; t++) {
                    int trail_pos = pos + (particles[p].from_left ? -t : t);
                    if (trail_pos >= 0 && trail_pos < NATIVE_RESOLUTION) {
                        uint8_t trail_bright = particles[p].brightness - (t * particles[p].brightness / trail_length);
                        CRGB16 trail_color = scale_color(particle_color, SQ15x16(trail_bright) / 255);
                        leds_16[trail_pos] = add_clipped(leds_16[trail_pos], trail_color);
                    }
                }
            }
        }
    }
    
    // Update and render explosions
    for (int e = 0; e < MAX_COLOR_PARTICLES; e++) {
        if (!explosions[e].active) continue;
        
        // Audio reactive explosion growth
        explosions[e].radius += SQ15x16(1.5f + total_energy * 2.0f);
        explosions[e].age++;
        
        // Fade explosion
        explosions[e].brightness = scale8(explosions[e].brightness, 220);
        
        // Deactivate old explosions
        if (explosions[e].age > 30 || explosions[e].brightness < 20) {
            explosions[e].active = false;
            continue;
        }
        
        // Render explosion
        int center = explosions[e].position.getInteger();
        int radius = explosions[e].radius.getInteger();
        
        for (int i = max(0, center - radius); i <= min(NATIVE_RESOLUTION - 1, center + radius); i++) {
            float distance = abs(i - center);
            if (distance <= radius) {
                // Explosion falloff
                float falloff = 1.0f - (distance / radius);
                uint8_t explosion_bright = explosions[e].brightness * falloff;
                
                // Audio reactive explosion colors
                uint8_t explosion_hue = explosions[e].hue + (uint8_t)(high_freq_energy * 60);
                
                CRGB16 explosion_color;
                if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
                    explosion_color = palette_to_crgb16(palette_arr[frame_config.PALETTE],
                                                      explosion_hue, explosion_bright);
                } else {
                    // White flash at explosion center
                    uint8_t explosion_sat = 255;
                    if (distance < radius * 0.3f) {
                        explosion_sat = 255 - (uint8_t)(200 * (1.0f - distance / (radius * 0.3f)));
                    }
                    
                    explosion_color = hsv_to_rgb_fast(explosion_hue, explosion_sat, explosion_bright);
                }
                
                leds_16[i] = add_clipped(leds_16[i], explosion_color);
            }
        }
    }
    
    // Add quantum fluctuations - audio reactive background sparkles
    if (frame_config.DENSITY > 100) {
        for (int i = 0; i < NATIVE_RESOLUTION; i++) {
            uint8_t sparkle_prob = (frame_config.DENSITY - 100) / 20;
            sparkle_prob += (uint8_t)(high_freq_energy * 30);
            
            if (random8() < sparkle_prob) {
                uint8_t sparkle_bright = 50 + random8(100) + (uint8_t)(total_energy * 80);
                if(sparkle_bright > 255) sparkle_bright = 255;
                
                // Random quantum colors
                uint8_t quantum_hue = random8();
                CRGB16 quantum_color = hsv_to_rgb_fast(quantum_hue, 200, sparkle_bright);
                leds_16[i] = add_clipped(leds_16[i], quantum_color);
            }
        }
    }
    
    apply_global_brightness();
}