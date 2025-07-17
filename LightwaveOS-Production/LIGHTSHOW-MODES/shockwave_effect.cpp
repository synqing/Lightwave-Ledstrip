#include "shockwave_effect.h"
#include "../src/globals.h"
#include "../src/GDFT.h"

#define MAX_SHOCKWAVES 3

struct Shockwave {
    SQ15x16 radius;
    SQ15x16 velocity;
    SQ15x16 intensity;
    uint8_t hue;
    bool active;
    uint32_t birth_time;
    uint8_t type;  // 0: normal, 1: double ring, 2: reversed
};

static Shockwave shockwaves[MAX_SHOCKWAVES];
static uint32_t last_spawn_time = 0;

void light_mode_shockwave() {
    cache_frame_config();
    
    // Audio integration for shockwave dynamics
    get_smooth_spectrogram();
    get_smooth_chromagram();
    calculate_vu();
    
    // Calculate frequency band energies
    float low_freq_energy = 0;
    for(int i = 0; i < 20; i++) {
        low_freq_energy += spectrogram_smooth[i];
    }
    low_freq_energy /= 20.0f;
    
    float mid_freq_energy = 0;
    for(int i = 20; i < 50; i++) {
        mid_freq_energy += spectrogram_smooth[i];
    }
    mid_freq_energy /= 30.0f;
    
    float high_freq_energy = 0;
    for(int i = 50; i < min(96, NUM_FREQS); i++) {
        high_freq_energy += spectrogram_smooth[i];
    }
    high_freq_energy /= (min(96, NUM_FREQS) - 50);
    
    // Beat detection for shockwave triggers
    static float last_impact_energy = 0;
    static float last_beat_time = 0;
    float current_impact_energy = low_freq_energy * 2.0f + mid_freq_energy;
    float impact_delta = current_impact_energy - last_impact_energy;
    bool beat_detected = (impact_delta > 0.2f && current_impact_energy > 0.4f && 
                         (millis() - last_beat_time) > 100); // Min 100ms between beats
    
    if (beat_detected) {
        last_beat_time = millis();
    }
    
    last_impact_energy = current_impact_energy * 0.1f + last_impact_energy * 0.9f;
    
    // Audio-reactive fade - more aggressive fade with higher energy
    float audio_fade_factor = 0.85f - (audio_vu_level * 0.1f) - (high_freq_energy * 0.05f);
    audio_fade_factor = constrain(audio_fade_factor, 0.75f, 0.9f);
    
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        leds_16[i] = scale_color(leds_16[i], SQ15x16(audio_fade_factor));
    }
    
    // Audio-reactive shockwave spawning
    uint32_t base_spawn_interval = 2000 - (frame_config.SPEED * 7);  // 2000ms to 215ms
    
    // Audio reduces spawn interval (faster spawning)
    float audio_spawn_factor = 1.0f - (audio_vu_level * 0.4f) - (low_freq_energy * 0.3f);
    audio_spawn_factor = constrain(audio_spawn_factor, 0.3f, 1.0f);
    uint32_t spawn_interval = base_spawn_interval * audio_spawn_factor;
    
    uint32_t now = millis();
    
    // Beat detection forces immediate shockwave spawn
    bool force_spawn = beat_detected;
    
    if (force_spawn || (now - last_spawn_time > spawn_interval)) {
        // Find inactive shockwave slot
        for (int s = 0; s < MAX_SHOCKWAVES; s++) {
            if (!shockwaves[s].active) {
                shockwaves[s].radius = 0;
                
                // Audio-reactive velocity
                float base_velocity = 0.5f + (frame_config.SPEED / 128.0f);
                float audio_velocity_boost = 1.0f + (audio_vu_level * 0.8f);
                
                // Beat detection creates faster shockwaves
                if (beat_detected) {
                    audio_velocity_boost += 0.6f;
                }
                
                shockwaves[s].velocity = SQ15x16(base_velocity * audio_velocity_boost);
                
                // Audio-reactive intensity
                float audio_intensity = 1.0f + (audio_vu_level * 0.5f) + (low_freq_energy * 0.3f);
                shockwaves[s].intensity = SQ15x16(audio_intensity);
                
                shockwaves[s].birth_time = now;
                shockwaves[s].active = true;
                
                // Audio-reactive type selection
                if (high_freq_energy > 0.3f) {
                    shockwaves[s].type = 1; // Double ring for high frequency
                } else if (low_freq_energy > 0.4f) {
                    shockwaves[s].type = 2; // Reversed for low frequency
                } else {
                    shockwaves[s].type = 0; // Normal for mid frequency
                }
                
                // Audio-reactive color selection
                if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
                    // Use frequency content to influence palette selection
                    uint8_t freq_hue = (low_freq_energy * 85) + (mid_freq_energy * 85) + (high_freq_energy * 85);
                    shockwaves[s].hue = freq_hue + random8(32) - 16;
                } else {
                    // Audio-reactive hue shifts
                    uint8_t audio_hue_shift = (mid_freq_energy * 24) + (high_freq_energy * 16);
                    shockwaves[s].hue = frame_config.HUE + audio_hue_shift + random8(24) - 12;
                }
                
                last_spawn_time = now;
                break;
            }
        }
    }
    
    // Update and render shockwaves
    for (int s = 0; s < MAX_SHOCKWAVES; s++) {
        if (!shockwaves[s].active) continue;
        
        // Audio-reactive physics update
        SQ15x16 velocity_multiplier = SQ15x16(1.0f + (audio_vu_level * 0.2f));
        
        // Frequency content affects velocity
        if (shockwaves[s].type == 0) {
            velocity_multiplier *= SQ15x16(1.0f + mid_freq_energy * 0.3f);
        } else if (shockwaves[s].type == 1) {
            velocity_multiplier *= SQ15x16(1.0f + high_freq_energy * 0.4f);
        } else {
            velocity_multiplier *= SQ15x16(1.0f + low_freq_energy * 0.5f);
        }
        
        shockwaves[s].radius += shockwaves[s].velocity * velocity_multiplier;
        
        // Audio-reactive intensity decay
        float decay_rate = 0.98f - (audio_vu_level * 0.01f);
        shockwaves[s].intensity *= SQ15x16(decay_rate);
        
        // Deactivate if too large or too dim
        if (shockwaves[s].radius > NATIVE_RESOLUTION/2 + 20 || 
            shockwaves[s].intensity < SQ15x16(0.05)) {
            shockwaves[s].active = false;
            continue;
        }
        
        // Render shockwave
        for (int i = 0; i < NATIVE_RESOLUTION; i++) {
            SQ15x16 dist = abs(SQ15x16(i) - SQ15x16(NATIVE_RESOLUTION/2));
            
            // Different rendering based on type
            bool should_light = false;
            SQ15x16 brightness_factor = 0;
            
            switch (shockwaves[s].type) {
                case 0:  // Normal shockwave
                    {
                        SQ15x16 ring_dist = abs(dist - shockwaves[s].radius);
                        if (ring_dist < 4) {
                            should_light = true;
                            brightness_factor = SQ15x16(1) - (ring_dist / 4);
                        }
                    }
                    break;
                    
                case 1:  // Double ring
                    {
                        SQ15x16 ring_dist1 = abs(dist - shockwaves[s].radius);
                        SQ15x16 ring_dist2 = abs(dist - (shockwaves[s].radius * SQ15x16(0.7)));
                        
                        if (ring_dist1 < 3) {
                            should_light = true;
                            brightness_factor = SQ15x16(1) - (ring_dist1 / 3);
                        } else if (ring_dist2 < 3) {
                            should_light = true;
                            brightness_factor = (SQ15x16(1) - (ring_dist2 / 3)) * SQ15x16(0.6);
                        }
                    }
                    break;
                    
                case 2:  // Reversed (imploding)
                    {
                        SQ15x16 reverse_radius = SQ15x16(NATIVE_RESOLUTION/2) - shockwaves[s].radius;
                        if (reverse_radius > 0) {
                            SQ15x16 ring_dist = abs(dist - reverse_radius);
                            if (ring_dist < 4) {
                                should_light = true;
                                brightness_factor = SQ15x16(1) - (ring_dist / 4);
                            }
                        }
                    }
                    break;
            }
            
            if (should_light) {
                // Audio-reactive brightness calculation
                uint8_t brightness = (brightness_factor * shockwaves[s].intensity * 255).getInteger();
                
                // Audio affects brightness
                float audio_brightness_boost = 1.0f + (audio_vu_level * 0.4f);
                
                // Beat detection creates flash effect
                if (beat_detected && (now - shockwaves[s].birth_time) < 150) {
                    audio_brightness_boost += 0.6f;
                }
                
                brightness = scale8(brightness, audio_brightness_boost * 255);
                
                // Audio-reactive distortion
                if (frame_config.DENSITY > 128 || audio_vu_level > 0.3f) {
                    uint8_t noise = inoise8(i * 10 + (audio_vu_level * 1000), now / 10);
                    uint8_t distortion_amount = frame_config.DENSITY - 128 + (audio_vu_level * 64);
                    brightness = scale8(brightness, 200 + scale8(noise, distortion_amount));
                }
                
                // Audio-reactive color generation
                CRGB16 color;
                if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
                    uint8_t pal_index = shockwaves[s].hue + (dist / 2).getInteger();
                    
                    // Audio affects palette progression
                    pal_index += (audio_vu_level * 24) + (high_freq_energy * 16);
                    
                    color = palette_to_crgb16(palette_arr[frame_config.PALETTE],
                                            pal_index, brightness);
                } else {
                    // Audio-reactive chromatic aberration
                    uint8_t hue_shift = (dist / 8).getInteger();
                    hue_shift += (high_freq_energy * 16) + (mid_freq_energy * 8);
                    
                    // Audio-reactive saturation
                    uint8_t dynamic_saturation = frame_config.SATURATION;
                    dynamic_saturation = scale8(dynamic_saturation, 200 + (audio_vu_level * 55));
                    
                    color = hsv_to_rgb_fast(
                        shockwaves[s].hue + hue_shift,
                        dynamic_saturation,
                        brightness
                    );
                }
                
                // Audio-reactive white core for fresh shockwaves
                uint32_t age = now - shockwaves[s].birth_time;
                uint32_t core_duration = 100 + (audio_vu_level * 100); // Longer core with audio
                
                if (age < core_duration) {
                    uint8_t white_amount = 255 - (age * 255 / core_duration);
                    
                    // Beat detection intensifies white core
                    if (beat_detected && age < 50) {
                        white_amount = 255;
                    }
                    
                    // Audio energy affects white core intensity
                    white_amount = scale8(white_amount, 200 + (audio_vu_level * 55));
                    
                    CRGB16 white = CRGB16(white_amount * 256, white_amount * 256, white_amount * 256);
                    color = add_clipped(color, white);
                }
                
                leds_16[i] = add_clipped(leds_16[i], color);
            }
        }
    }
    
    apply_global_brightness();
}