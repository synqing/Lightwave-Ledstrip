#include "ripple_effect.h"
#include "../src/globals.h"
#include "../src/GDFT.h"

// Maximum number of concurrent ripples
#define MAX_RIPPLES 5

// Ripple state structure
struct Ripple {
    SQ15x16 radius;      // Current radius from center
    SQ15x16 speed;       // Expansion speed
    uint8_t hue;         // Base hue for this ripple
    uint8_t saturation;  // Saturation level
    bool active;         // Is this ripple active?
    uint16_t birth_time; // When ripple was created (for fade calculation)
};

// Static ripple storage
static Ripple ripples[MAX_RIPPLES];
static bool initialized = false;

void light_mode_ripple() {
    // Cache configuration once per frame
    cache_frame_config();
    
    // Audio integration for ripple dynamics
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
    
    // Beat detection for new ripple sources
    static float last_bass_energy = 0;
    static float last_beat_time = 0;
    float current_bass_energy = low_freq_energy + (chromagram_smooth[0] + chromagram_smooth[1] + chromagram_smooth[2]) / 3.0f;
    float bass_delta = current_bass_energy - last_bass_energy;
    bool beat_detected = (bass_delta > 0.15f && current_bass_energy > 0.3f && 
                         (millis() - last_beat_time) > 150); // Min 150ms between beats
    
    if (beat_detected) {
        last_beat_time = millis();
    }
    
    last_bass_energy = current_bass_energy * 0.1f + last_bass_energy * 0.9f;
    
    // Initialize ripples on first run
    if (!initialized) {
        for (int i = 0; i < MAX_RIPPLES; i++) {
            ripples[i].active = false;
        }
        initialized = true;
    }
    
    // Audio-reactive fade - stronger fade with higher audio energy
    float audio_fade_factor = 0.92f - (audio_vu_level * 0.08f);
    audio_fade_factor = constrain(audio_fade_factor, 0.85f, 0.95f);
    
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        leds_16[i] = scale_color(leds_16[i], SQ15x16(audio_fade_factor));
    }
    
    // Audio-reactive ripple spawning
    uint8_t base_spawn_chance = scale8(frame_config.SPEED, 30) + 5;
    
    // Beat detection triggers immediate ripple spawn
    bool force_spawn = beat_detected;
    
    // Additional audio-reactive spawn chance based on frequency content
    float audio_spawn_factor = 1.0f + (low_freq_energy * 2.0f) + (mid_freq_energy * 1.5f);
    uint8_t audio_spawn_chance = base_spawn_chance * audio_spawn_factor;
    
    if (force_spawn || random8() < audio_spawn_chance) {
        for (int r = 0; r < MAX_RIPPLES; r++) {
            if (!ripples[r].active) {
                ripples[r].radius = 0;
                
                // Audio-reactive speed - faster ripples on beats and high energy
                float speed_base = 0.3f + (audio_vu_level * 0.4f);
                if (beat_detected) {
                    speed_base += 0.3f; // Beat boost
                }
                ripples[r].speed = SQ15x16(speed_base) + (SQ15x16(random8()) / 200);
                
                // Audio-reactive color selection
                if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
                    // Use frequency content to influence palette selection
                    uint8_t freq_hue = (low_freq_energy * 85) + (mid_freq_energy * 85) + (high_freq_energy * 85);
                    ripples[r].hue = freq_hue + random8(64) - 32;
                    ripples[r].saturation = 255;
                } else {
                    // Audio-reactive hue shifts
                    uint8_t audio_hue_shift = (mid_freq_energy * 32) + (high_freq_energy * 16);
                    ripples[r].hue = frame_config.HUE + audio_hue_shift + random8(32) - 16;
                    ripples[r].saturation = frame_config.SATURATION;
                }
                
                ripples[r].active = true;
                ripples[r].birth_time = millis();
                break;
            }
        }
    }
    
    // Update and render ripples
    for (int r = 0; r < MAX_RIPPLES; r++) {
        if (!ripples[r].active) continue;
        
        // Audio-reactive radius expansion
        SQ15x16 speed_multiplier = SQ15x16(frame_config.SPEED) / 128;
        
        // Frequency content affects expansion speed
        if (r % 3 == 0) {
            // Low frequency reactive
            speed_multiplier *= SQ15x16(1.0f + low_freq_energy * 0.8f);
        } else if (r % 3 == 1) {
            // Mid frequency reactive
            speed_multiplier *= SQ15x16(1.0f + mid_freq_energy * 0.6f);
        } else {
            // High frequency reactive
            speed_multiplier *= SQ15x16(1.0f + high_freq_energy * 1.2f);
        }
        
        ripples[r].radius += ripples[r].speed * speed_multiplier;
        
        // Check if ripple has expanded beyond strip
        if (ripples[r].radius > NATIVE_RESOLUTION/2 + 10) {
            ripples[r].active = false;
            continue;
        }
        
        // Calculate age-based fade
        uint32_t age = millis() - ripples[r].birth_time;
        uint8_t age_brightness = 255;
        if (age > 1000) {
            age_brightness = 255 - min(255, (age - 1000) / 8);
        }
        
        // Draw ripple
        for (int i = 0; i < NATIVE_RESOLUTION; i++) {
            // Calculate distance from center
            SQ15x16 dist = abs(SQ15x16(i) - SQ15x16(NATIVE_RESOLUTION/2));
            
            // Calculate distance from ripple edge
            SQ15x16 wave_dist = abs(dist - ripples[r].radius);
            
            // Audio-reactive ripple thickness
            SQ15x16 base_thickness = SQ15x16(2) + (SQ15x16(frame_config.DENSITY) / 64);
            
            // Audio energy affects thickness
            float audio_thickness_factor = 1.0f + (audio_vu_level * 0.5f);
            
            // Frequency content creates thickness variation
            if (r % 3 == 0) {
                audio_thickness_factor += low_freq_energy * 0.3f;
            } else if (r % 3 == 1) {
                audio_thickness_factor += mid_freq_energy * 0.2f;
            } else {
                audio_thickness_factor += high_freq_energy * 0.4f;
            }
            
            SQ15x16 thickness = base_thickness * SQ15x16(audio_thickness_factor);
            
            if (wave_dist < thickness) {
                // Calculate brightness based on distance from ripple center
                SQ15x16 normalized_dist = wave_dist / thickness;
                uint8_t wave_brightness = 255 - (normalized_dist * 255).getInteger();
                
                // Apply age fade and distance fade with audio enhancement
                SQ15x16 distance_fade = SQ15x16(1) - (ripples[r].radius / (NATIVE_RESOLUTION/2));
                wave_brightness = scale8(wave_brightness, distance_fade.getInteger() * 255);
                wave_brightness = scale8(wave_brightness, age_brightness);
                
                // Audio-reactive brightness boost
                float audio_brightness_boost = 1.0f + (audio_vu_level * 0.3f);
                
                // Beat detection creates flash effect
                if (beat_detected && (millis() - ripples[r].birth_time) < 200) {
                    audio_brightness_boost += 0.5f;
                }
                
                wave_brightness = scale8(wave_brightness, audio_brightness_boost * 255);
                
                // Audio-reactive color generation
                CRGB16 color;
                if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
                    // Use palette with ripple-specific offset and audio modulation
                    uint8_t palette_index = ripples[r].hue + (dist / 4).getInteger();
                    
                    // Audio affects palette progression
                    palette_index += (audio_vu_level * 32) + (mid_freq_energy * 16);
                    
                    color = palette_to_crgb16(palette_arr[frame_config.PALETTE], 
                                            palette_index, wave_brightness);
                } else {
                    // Audio-reactive hue shifting
                    uint8_t dynamic_hue = ripples[r].hue;
                    
                    // Frequency content creates hue shifts
                    if (r % 3 == 0) {
                        dynamic_hue += low_freq_energy * 24;
                    } else if (r % 3 == 1) {
                        dynamic_hue += mid_freq_energy * 16;
                    } else {
                        dynamic_hue += high_freq_energy * 32;
                    }
                    
                    // Audio-reactive saturation
                    uint8_t dynamic_saturation = ripples[r].saturation;
                    dynamic_saturation = scale8(dynamic_saturation, 200 + (audio_vu_level * 55));
                    
                    color = hsv_to_rgb_fast(
                        dynamic_hue,
                        dynamic_saturation,
                        wave_brightness
                    );
                }
                
                // Add to existing color (additive blending)
                leds_16[i] = add_clipped(leds_16[i], color);
            }
        }
    }
    
    // Apply global brightness
    apply_global_brightness();
}