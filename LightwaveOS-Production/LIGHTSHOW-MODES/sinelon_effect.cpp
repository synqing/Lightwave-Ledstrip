#include "sinelon_effect.h"
#include "../src/globals.h"
#include "../src/GDFT.h"

// Sinelon parameters
#define MAX_SINELONS 6

struct SinelonDot {
    uint8_t frequency;    // Oscillation frequency
    uint8_t phase_offset; // Phase offset for variety
    uint8_t hue_offset;   // Color offset
    uint8_t trail_length; // How long the trail is
};

static SinelonDot sinelons[MAX_SINELONS];
static bool initialized = false;

void light_mode_sinelon() {
    cache_frame_config();
    
    // Audio integration for sinelon dynamics
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
    
    // Beat detection for sinelon behavior changes
    static float last_rhythm_energy = 0;
    static float last_beat_time = 0;
    float current_rhythm_energy = mid_freq_energy + (low_freq_energy * 0.7f);
    float rhythm_delta = current_rhythm_energy - last_rhythm_energy;
    bool beat_detected = (rhythm_delta > 0.15f && current_rhythm_energy > 0.25f && 
                         (millis() - last_beat_time) > 120); // Min 120ms between beats
    
    if (beat_detected) {
        last_beat_time = millis();
    }
    
    last_rhythm_energy = current_rhythm_energy * 0.1f + last_rhythm_energy * 0.9f;
    
    // Initialize sinelons with different frequencies
    if (!initialized) {
        for (int i = 0; i < MAX_SINELONS; i++) {
            sinelons[i].frequency = 7 + i * 3;  // 7, 10, 13, 16, 19, 22
            sinelons[i].phase_offset = (i * 255) / MAX_SINELONS;
            sinelons[i].hue_offset = (i * 96) % 255;  // Spread across color wheel
            sinelons[i].trail_length = 20 - i * 2;    // Varying trail lengths
        }
        initialized = true;
    }
    
    // Audio-reactive fade rate
    uint8_t base_fade_rate = 255 - scale8(frame_config.DENSITY, 235);
    
    // Audio energy affects fade rate - higher energy = less fade (longer trails)
    float audio_fade_factor = 1.0f - (audio_vu_level * 0.3f) - (mid_freq_energy * 0.2f);
    audio_fade_factor = constrain(audio_fade_factor, 0.6f, 1.0f);
    
    uint8_t fade_rate = base_fade_rate * audio_fade_factor;
    fadeToBlackBy(leds_16, NATIVE_RESOLUTION, fade_rate);
    
    // Audio-reactive number of active sinelons
    int base_active_sinelons = 1 + (frame_config.SPEED / 51);  // 1-5 based on speed
    
    // Audio energy can increase active sinelons
    int audio_bonus_sinelons = 0;
    if (audio_vu_level > 0.4f) {
        audio_bonus_sinelons = 1;
    }
    if (high_freq_energy > 0.3f) {
        audio_bonus_sinelons += 1;
    }
    if (beat_detected) {
        audio_bonus_sinelons += 1;
    }
    
    int active_sinelons = base_active_sinelons + audio_bonus_sinelons;
    active_sinelons = min(active_sinelons, MAX_SINELONS);
    
    // Render each sinelon
    for (int s = 0; s < active_sinelons; s++) {
        // Audio-reactive position calculation
        uint16_t base_beat_freq = scale8(sinelons[s].frequency, frame_config.SPEED/4 + 32);
        
        // Audio affects frequency based on sinelon index
        float audio_freq_multiplier = 1.0f;
        if (s % 3 == 0) {
            // Low frequency reactive
            audio_freq_multiplier = 1.0f + (low_freq_energy * 0.8f);
        } else if (s % 3 == 1) {
            // Mid frequency reactive
            audio_freq_multiplier = 1.0f + (mid_freq_energy * 0.6f);
        } else {
            // High frequency reactive
            audio_freq_multiplier = 1.0f + (high_freq_energy * 1.2f);
        }
        
        // Beat detection creates phase jumps
        static uint8_t beat_phase_offsets[MAX_SINELONS] = {0};
        if (beat_detected) {
            beat_phase_offsets[s] += random8(64) + 32;
        }
        
        uint16_t beat_freq = base_beat_freq * audio_freq_multiplier;
        int pos = beatsin16(beat_freq, 0, NATIVE_RESOLUTION - 1, 
                           0, (sinelons[s].phase_offset + beat_phase_offsets[s]) * 256);
        
        // Audio-reactive color selection
        CRGB16 color;
        
        // Audio affects brightness
        uint8_t audio_brightness = 255;
        if (s % 3 == 0) {
            audio_brightness = 200 + (low_freq_energy * 55);
        } else if (s % 3 == 1) {
            audio_brightness = 200 + (mid_freq_energy * 55);
        } else {
            audio_brightness = 200 + (high_freq_energy * 55);
        }
        
        // Beat detection creates brightness boost
        if (beat_detected) {
            audio_brightness = 255;
        }
        
        if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
            uint8_t palette_index = beat8(beat_freq/2) + sinelons[s].hue_offset;
            
            // Audio affects palette progression
            palette_index += (audio_vu_level * 32) + (mid_freq_energy * 16);
            
            color = palette_to_crgb16(palette_arr[frame_config.PALETTE],
                                    palette_index, audio_brightness);
        } else if (frame_config.COLOR_MODE == COLOR_MODE_HYBRID) {
            // Hybrid - base hue with animated offset and audio modulation
            uint8_t hue = frame_config.HUE + sinelons[s].hue_offset + beat8(beat_freq/4);
            
            // Audio affects hue shifts
            if (s % 3 == 0) {
                hue += low_freq_energy * 24;
            } else if (s % 3 == 1) {
                hue += mid_freq_energy * 16;
            } else {
                hue += high_freq_energy * 32;
            }
            
            // Audio-reactive saturation
            uint8_t dynamic_saturation = frame_config.SATURATION;
            dynamic_saturation = scale8(dynamic_saturation, 200 + (audio_vu_level * 55));
            
            color = hsv_to_rgb_fast(hue, dynamic_saturation, audio_brightness);
        } else {
            // Pure HSV with audio modulation
            uint8_t hue = frame_config.HUE + sinelons[s].hue_offset;
            
            // Audio affects hue shifts
            if (s % 3 == 0) {
                hue += low_freq_energy * 20;
            } else if (s % 3 == 1) {
                hue += mid_freq_energy * 16;
            } else {
                hue += high_freq_energy * 28;
            }
            
            // Audio-reactive saturation
            uint8_t dynamic_saturation = frame_config.SATURATION;
            dynamic_saturation = scale8(dynamic_saturation, 200 + (audio_vu_level * 55));
            
            color = get_mode_color(hue, dynamic_saturation, audio_brightness);
        }
        
        // Audio-reactive dot rendering with anti-aliasing
        if (pos > 0 && pos < NATIVE_RESOLUTION - 1) {
            // Anti-aliased rendering for smooth motion
            uint8_t frac = (beatsin16(beat_freq, 0, 255, 0, (sinelons[s].phase_offset + beat_phase_offsets[s]) * 256));
            
            // Main pixel with audio intensity
            float dot_intensity = 1.0f + (audio_vu_level * 0.5f);
            if (beat_detected) {
                dot_intensity += 0.3f;
            }
            
            CRGB16 intensified_color = scale_color(color, SQ15x16(dot_intensity));
            leds_16[pos] = add_clipped(leds_16[pos], intensified_color);
            
            // Audio-reactive anti-aliasing strength
            float aa_strength = 0.3f + (audio_vu_level * 0.2f);
            CRGB16 dimmed_color = scale_color(color, SQ15x16(aa_strength));
            
            if (pos > 0) {
                leds_16[pos - 1] = add_clipped(leds_16[pos - 1], 
                                              scale_color(dimmed_color, SQ15x16(1) - SQ15x16(frac)/255));
            }
            if (pos < NATIVE_RESOLUTION - 1) {
                leds_16[pos + 1] = add_clipped(leds_16[pos + 1], 
                                              scale_color(dimmed_color, SQ15x16(frac)/255));
            }
        }
        
        // Audio-reactive motion blur trail
        bool enable_trail = (frame_config.DENSITY > 128) || (audio_vu_level > 0.3f) || beat_detected;
        
        if (enable_trail) {
            static int last_pos[MAX_SINELONS] = {0};
            
            // Draw trail between last and current position
            int start = min(last_pos[s], pos);
            int end = max(last_pos[s], pos);
            
            for (int i = start; i <= end; i++) {
                if (i >= 0 && i < NATIVE_RESOLUTION && i != pos) {
                    uint8_t base_trail_bright = scale8(255 - abs(i - pos) * 10, 
                                                      frame_config.DENSITY - 128);
                    
                    // Audio affects trail brightness
                    float audio_trail_factor = 1.0f + (audio_vu_level * 0.4f);
                    
                    // Beat detection intensifies trail
                    if (beat_detected) {
                        audio_trail_factor += 0.3f;
                    }
                    
                    uint8_t trail_bright = scale8(base_trail_bright, audio_trail_factor * 255);
                    CRGB16 trail_color = scale_color(color, SQ15x16(trail_bright) / 255);
                    leds_16[i] = add_clipped(leds_16[i], trail_color);
                }
            }
            
            last_pos[s] = pos;
        }
    }
    
    // Audio-reactive sparkle effect
    uint8_t sparkle_threshold = frame_config.DENSITY - 200;
    
    // Audio can trigger sparkles even with lower density
    if (audio_vu_level > 0.5f) {
        sparkle_threshold = max(sparkle_threshold, (uint8_t)(audio_vu_level * 100));
    }
    
    // High frequency energy creates more sparkles
    if (high_freq_energy > 0.4f) {
        sparkle_threshold = max(sparkle_threshold, (uint8_t)(high_freq_energy * 120));
    }
    
    // Beat detection creates burst of sparkles
    if (beat_detected) {
        sparkle_threshold = max(sparkle_threshold, (uint8_t)150);
    }
    
    if (sparkle_threshold > 0 && random8() < sparkle_threshold) {
        int sparkle_pos = random16(NATIVE_RESOLUTION);
        
        // Audio affects sparkle intensity
        uint16_t sparkle_intensity = 65535;
        if (high_freq_energy > 0.2f) {
            sparkle_intensity = 32768 + (high_freq_energy * 32767);
        }
        
        leds_16[sparkle_pos] = CRGB16(sparkle_intensity, sparkle_intensity, sparkle_intensity);
    }
    
    apply_global_brightness();
}