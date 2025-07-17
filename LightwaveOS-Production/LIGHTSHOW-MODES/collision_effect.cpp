#include "collision_effect.h"
#include "../src/globals.h"
#include "../src/GDFT.h"

#define MAX_PARTICLES 2
#define MAX_DEBRIS 20

struct Particle {
    SQ15x16 position;
    SQ15x16 velocity;
    uint8_t hue;
    bool active;
    bool from_left;  // Direction
};

struct Debris {
    SQ15x16 position;
    SQ15x16 velocity;
    uint8_t hue;
    uint8_t brightness;
    bool active;
};

static Particle particles[MAX_PARTICLES];
static Debris debris[MAX_DEBRIS];
static bool collision_occurred = false;
static uint32_t collision_time = 0;
static uint32_t last_launch_time = 0;

void light_mode_collision() {
    cache_frame_config();
    
    // Audio integration - get current audio state
    get_smooth_spectrogram();
    get_smooth_chromagram();
    calculate_vu();
    
    // Beat detection for collision trigger
    static float last_bass_energy = 0;
    float current_bass_energy = chromagram_smooth[0] + chromagram_smooth[1] + chromagram_smooth[2];
    float bass_delta = current_bass_energy - last_bass_energy;
    bool beat_detected = (bass_delta > 0.3f && current_bass_energy > 0.5f);
    last_bass_energy = current_bass_energy * 0.9f + last_bass_energy * 0.1f;
    
    // Fade existing pixels
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        leds_16[i] = scale_color(leds_16[i], SQ15x16(0.9));
    }
    
    // Launch new particles - audio reactive timing
    uint32_t base_interval = 3000 - (frame_config.SPEED * 10);
    // Reduce interval on beats for more frequent collisions
    uint32_t launch_interval = beat_detected ? base_interval / 4 : base_interval;
    uint32_t now = millis();
    
    if (!collision_occurred && (now - last_launch_time > launch_interval || beat_detected)) {
        // Reset particles
        particles[0].position = 0;
        particles[0].velocity = SQ15x16(0.2);
        particles[0].active = true;
        particles[0].from_left = true;
        
        particles[1].position = NATIVE_RESOLUTION - 1;
        particles[1].velocity = SQ15x16(-0.2);
        particles[1].active = true;
        particles[1].from_left = false;
        
        // Colors based on mode
        if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
            particles[0].hue = 0;
            particles[1].hue = 128;
        } else {
            particles[0].hue = frame_config.HUE;
            particles[1].hue = frame_config.HUE + 128;
        }
        
        last_launch_time = now;
        collision_occurred = false;
    }
    
    // Update particles
    if (!collision_occurred) {
        for (int p = 0; p < MAX_PARTICLES; p++) {
            if (!particles[p].active) continue;
            
            // Accelerate particles (faster as they approach center)
            SQ15x16 center = SQ15x16(NATIVE_RESOLUTION / 2);
            SQ15x16 dist_to_center = abs(particles[p].position - center);
            SQ15x16 accel_factor = SQ15x16(1) + (SQ15x16(2) - (dist_to_center / center));
            
            // Audio reactive acceleration - higher frequencies increase speed
            float high_freq_energy = 0;
            for(int i = 60; i < 96; i++) {
                high_freq_energy += spectrogram_smooth[i];
            }
            SQ15x16 audio_accel = SQ15x16(1) + (SQ15x16(high_freq_energy) * SQ15x16(0.5));
            
            particles[p].velocity *= SQ15x16(1.02) * accel_factor * audio_accel;
            particles[p].position += particles[p].velocity;
            
            // Draw particle with trail
            int pos = particles[p].position.getInteger();
            if (pos >= 0 && pos < NATIVE_RESOLUTION) {
                // Main particle
                CRGB16 color;
                if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
                    color = palette_to_crgb16(palette_arr[frame_config.PALETTE],
                                            particles[p].hue, 255);
                } else {
                    color = hsv_to_rgb_fast(particles[p].hue, 
                                          frame_config.SATURATION, 255);
                }
                
                leds_16[pos] = add_clipped(leds_16[pos], color);
                
                // Trail effect
                int trail_length = 10 + (frame_config.DENSITY / 25);
                for (int t = 1; t < trail_length; t++) {
                    int trail_pos = pos + (particles[p].from_left ? -t : t);
                    if (trail_pos >= 0 && trail_pos < NATIVE_RESOLUTION) {
                        uint8_t trail_bright = 255 - (t * 255 / trail_length);
                        CRGB16 trail_color = scale_color(color, 
                                                        SQ15x16(trail_bright) / 255);
                        leds_16[trail_pos] = add_clipped(leds_16[trail_pos], trail_color);
                    }
                }
            }
        }
        
        // Check for collision
        SQ15x16 distance = abs(particles[0].position - particles[1].position);
        if (distance < 5) {
            collision_occurred = true;
            collision_time = now;
            
            // Create debris
            for (int d = 0; d < MAX_DEBRIS; d++) {
                debris[d].position = SQ15x16(NATIVE_RESOLUTION / 2);
                
                // Random velocity in all directions - audio reactive debris speed
                float mid_freq_energy = 0;
                for(int i = 30; i < 60; i++) {
                    mid_freq_energy += spectrogram_smooth[i];
                }
                SQ15x16 base_speed = SQ15x16(0.5) + (SQ15x16(mid_freq_energy) * SQ15x16(2));
                SQ15x16 speed = base_speed + (SQ15x16(random8()) / 128);
                debris[d].velocity = random8(2) ? speed : -speed;
                
                // Audio reactive colors based on frequency content
                if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
                    // Map frequency peaks to palette positions
                    uint8_t peak_freq = 0;
                    float max_energy = 0;
                    for(int i = 0; i < 12; i++) {
                        if(chromagram_smooth[i] > max_energy) {
                            max_energy = chromagram_smooth[i];
                            peak_freq = i;
                        }
                    }
                    debris[d].hue = peak_freq * 21 + random8(21);  // Map to palette sections
                } else if (frame_config.COLOR_MODE == COLOR_MODE_HYBRID) {
                    debris[d].hue = frame_config.HUE + random8(64) - 32;
                } else {
                    debris[d].hue = random8(2) ? particles[0].hue : particles[1].hue;
                }
                
                debris[d].brightness = 255;
                debris[d].active = true;
            }
            
            // Deactivate particles
            particles[0].active = false;
            particles[1].active = false;
        }
    }
    
    // Update and render debris
    if (collision_occurred) {
        uint32_t time_since_collision = now - collision_time;
        
        // Flash effect immediately after collision
        if (time_since_collision < 50) {
            uint8_t flash_bright = 255 - (time_since_collision * 5);
            for (int i = 0; i < NATIVE_RESOLUTION; i++) {
                SQ15x16 dist = abs(SQ15x16(i) - SQ15x16(NATIVE_RESOLUTION/2));
                if (dist < 20) {
                    uint8_t bright = scale8(flash_bright, 255 - (dist * 12).getInteger());
                    CRGB16 white = CRGB16(bright * 256, bright * 256, bright * 256);
                    leds_16[i] = add_clipped(leds_16[i], white);
                }
            }
        }
        
        // Update debris
        bool any_active = false;
        for (int d = 0; d < MAX_DEBRIS; d++) {
            if (!debris[d].active) continue;
            
            any_active = true;
            
            // Update position with gravity effect
            debris[d].position += debris[d].velocity;
            debris[d].velocity *= SQ15x16(0.98);  // Air resistance
            
            // Fade out
            debris[d].brightness = scale8(debris[d].brightness, 250);
            
            // Deactivate if out of bounds or too dim
            int pos = debris[d].position.getInteger();
            if (pos < 0 || pos >= NATIVE_RESOLUTION || debris[d].brightness < 10) {
                debris[d].active = false;
                continue;
            }
            
            // Render debris
            CRGB16 color;
            if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
                color = palette_to_crgb16(palette_arr[frame_config.PALETTE],
                                        debris[d].hue, debris[d].brightness);
            } else {
                color = hsv_to_rgb_fast(debris[d].hue, 
                                      frame_config.SATURATION, 
                                      debris[d].brightness);
            }
            
            leds_16[pos] = add_clipped(leds_16[pos], color);
        }
        
        // Reset for next collision
        if (!any_active) {
            collision_occurred = false;
        }
    }
    
    apply_global_brightness();
}