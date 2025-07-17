#include "lgp_aurora_borealis_effect.h"
#include "../src/globals.h"
#include "../src/GDFT.h"

static uint16_t aurora_time = 0;

void light_mode_lgp_aurora_borealis() {
    cache_frame_config();
    
    // Audio integration for aurora dynamics
    get_smooth_spectrogram();
    get_smooth_chromagram();
    calculate_vu();
    
    // Calculate aurora activity based on audio energy
    float total_energy = 0;
    for(int i = 0; i < 96; i++) {
        total_energy += spectrogram_smooth[i];
    }
    
    // Low frequency energy affects aurora intensity
    float low_freq_energy = 0;
    for(int i = 0; i < 20; i++) {
        low_freq_energy += spectrogram_smooth[i];
    }
    
    // Mid frequency energy affects aurora movement
    float mid_freq_energy = 0;
    for(int i = 20; i < 50; i++) {
        mid_freq_energy += spectrogram_smooth[i];
    }
    
    // High frequency energy affects aurora color shifts
    float high_freq_energy = 0;
    for(int i = 50; i < 96; i++) {
        high_freq_energy += spectrogram_smooth[i];
    }
    
    // Time progression for aurora movement - audio reactive
    uint16_t base_speed = frame_config.SPEED / 16;
    // Increase speed with mid-frequency energy
    uint16_t audio_speed_boost = (uint16_t)(mid_freq_energy * 50);
    aurora_time += base_speed + audio_speed_boost;
    
    // Clear with very slow fade for persistence
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        leds_16[i] = scale_color(leds_16[i], SQ15x16(0.97));
    }
    
    // Create multiple aurora layers - audio reactive layer count
    int num_layers = 2 + (int)(total_energy * 3);  // 2-5 layers based on audio
    if(num_layers > 5) num_layers = 5;
    
    for (int layer = 0; layer < num_layers; layer++) {
        // Each layer has different characteristics
        uint16_t layer_offset = layer * 1000;
        SQ15x16 layer_speed = SQ15x16(1) + (SQ15x16(layer) * SQ15x16(0.3));
        
        // Audio reactive layer speed modulation
        layer_speed = layer_speed * (SQ15x16(1) + SQ15x16(low_freq_energy) * SQ15x16(0.5));
        
        for (int i = 0; i < NATIVE_RESOLUTION; i++) {
            // Multi-octave noise for natural movement
            uint16_t noise1 = inoise16(i * 100, aurora_time * layer_speed.getInteger() + layer_offset);
            uint16_t noise2 = inoise16(i * 200, aurora_time * 2 + layer_offset + 5000);
            uint16_t noise3 = inoise16(i * 50, aurora_time / 2 + layer_offset + 10000);
            
            // Combine noise octaves
            uint16_t combined_noise = (noise1 / 2) + (noise2 / 3) + (noise3 / 6);
            
            // Create vertical bands with varying intensity
            uint8_t band_intensity = combined_noise >> 8;
            
            // Aurora typically appears in bands - audio reactive threshold
            uint8_t threshold = 140 - (uint8_t)(total_energy * 60);  // Lower threshold with more audio
            if(threshold < 80) threshold = 80;
            
            if (band_intensity > threshold) {
                // Calculate brightness with height variation - audio reactive
                uint8_t brightness = map(band_intensity, threshold, 255, 0, 255);
                
                // Boost brightness with low frequency energy
                brightness = (uint8_t)(brightness * (1.0f + low_freq_energy * 0.8f));
                if(brightness > 255) brightness = 255;
                
                // Distance from center affects intensity
                SQ15x16 dist = abs(SQ15x16(i) - SQ15x16(NATIVE_RESOLUTION/2));
                SQ15x16 height_factor = SQ15x16(1) - (dist / (NATIVE_RESOLUTION/2)) * SQ15x16(0.5);
                brightness = (brightness * height_factor).getInteger();
                
                // Aurora colors - primarily greens and blues with occasional reds
                CRGB16 color;
                if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
                    // Use palette but bias toward aurora colors
                    uint8_t pal_index = 96 + (noise1 >> 10);  // Green range
                    color = palette_to_crgb16(palette_arr[frame_config.PALETTE],
                                            pal_index, brightness);
                } else {
                    // Realistic aurora colors - audio reactive color selection
                    uint8_t color_choice = (noise3 >> 14);  // 0-3
                    uint8_t hue;
                    uint8_t sat;
                    
                    // High frequency energy increases chance of rare colors
                    if(high_freq_energy > 0.3f && random8() < 100) {
                        color_choice = 3;  // Force rare red/pink
                    }
                    
                    switch (color_choice) {
                        case 0:  // Green (most common)
                            hue = 96 + (uint8_t)(high_freq_energy * 20);  // Shift with high freq
                            sat = 255;
                            break;
                        case 1:  // Blue-green
                            hue = 120 + (uint8_t)(high_freq_energy * 15);
                            sat = 200;
                            break;
                        case 2:  // Blue
                            hue = 160 + (uint8_t)(high_freq_energy * 10);
                            sat = 180;
                            break;
                        default:  // Rare red/pink - enhanced by audio
                            hue = 220 + (uint8_t)(high_freq_energy * 30);
                            sat = 150 + (uint8_t)(high_freq_energy * 80);
                            brightness = brightness * (2 + (uint8_t)(high_freq_energy * 2)) / 3;
                            break;
                    }
                    
                    color = hsv_to_rgb_fast(hue, sat, brightness);
                }
                
                // Add shimmer - audio reactive shimmer probability
                uint8_t shimmer_prob = 30 + (uint8_t)(high_freq_energy * 100);
                if (random8() < shimmer_prob) {
                    SQ15x16 shimmer_intensity = SQ15x16(1.2) + SQ15x16(high_freq_energy * 0.5);
                    color = scale_color(color, shimmer_intensity);
                }
                
                // Layer blending
                if (layer == 0) {
                    leds_16[i] = color;
                } else {
                    leds_16[i] = add_clipped(leds_16[i], scale_color(color, SQ15x16(0.5)));
                }
            }
        }
    }
    
    // Add occasional bright streaks - audio reactive streaks
    uint8_t streak_prob = 5 + (uint8_t)(total_energy * 20);
    if (frame_config.DENSITY > 128 && random8() < streak_prob) {
        int streak_pos = random16(NATIVE_RESOLUTION);
        int streak_length = 5 + random8(10);
        
        for (int i = 0; i < streak_length; i++) {
            int pos = streak_pos + i - streak_length/2;
            if (pos >= 0 && pos < NATIVE_RESOLUTION) {
                uint8_t streak_bright = 255 - (abs(i - streak_length/2) * 50);
                
                // Audio reactive streak brightness and color
                streak_bright = (uint8_t)(streak_bright * (1.0f + total_energy * 0.3f));
                if(streak_bright > 255) streak_bright = 255;
                
                uint8_t streak_hue = 96 + random8(40) + (uint8_t)(high_freq_energy * 60);
                uint8_t streak_sat = 200 + (uint8_t)(mid_freq_energy * 50);
                if(streak_sat > 255) streak_sat = 255;
                
                CRGB16 streak_color = hsv_to_rgb_fast(streak_hue, streak_sat, streak_bright);
                leds_16[pos] = add_clipped(leds_16[pos], streak_color);
            }
        }
    }
    
    apply_global_brightness();
}