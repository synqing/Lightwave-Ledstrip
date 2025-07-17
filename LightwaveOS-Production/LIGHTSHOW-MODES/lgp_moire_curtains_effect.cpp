#include "lgp_moire_curtains_effect.h"
#include "../src/globals.h"
#include "../src/GDFT.h"

static SQ15x16 curtain1_offset = 0;
static SQ15x16 curtain2_offset = 0;
static SQ15x16 curtain3_offset = 0;

void light_mode_lgp_moire_curtains() {
    cache_frame_config();
    
    // Audio integration for curtain dynamics
    get_smooth_spectrogram();
    get_smooth_chromagram();
    calculate_vu();
    
    // Calculate frequency band energies
    float low_freq_energy = 0;
    for(int i = 0; i < 20; i++) {
        low_freq_energy += spectrogram_smooth[i];
    }
    
    float mid_freq_energy = 0;
    for(int i = 20; i < 50; i++) {
        mid_freq_energy += spectrogram_smooth[i];
    }
    
    float high_freq_energy = 0;
    for(int i = 50; i < 96; i++) {
        high_freq_energy += spectrogram_smooth[i];
    }
    
    // Beat detection for curtain speed bursts
    static float last_bass_energy = 0;
    float current_bass_energy = chromagram_smooth[0] + chromagram_smooth[1] + chromagram_smooth[2];
    float bass_delta = current_bass_energy - last_bass_energy;
    bool beat_detected = (bass_delta > 0.3f && current_bass_energy > 0.5f);
    last_bass_energy = current_bass_energy * 0.9f + last_bass_energy * 0.1f;
    
    // Move curtains at different speeds - audio reactive
    SQ15x16 base_speed = SQ15x16(frame_config.SPEED) / 255;
    
    // Each curtain responds to different frequency bands
    SQ15x16 curtain1_speed = base_speed * (SQ15x16(1) + SQ15x16(low_freq_energy * 1.5));
    SQ15x16 curtain2_speed = base_speed * SQ15x16(0.7) * (SQ15x16(1) + SQ15x16(mid_freq_energy * 1.2));
    SQ15x16 curtain3_speed = base_speed * SQ15x16(0.4) * (SQ15x16(1) + SQ15x16(high_freq_energy * 2));
    
    // Beat detection creates speed bursts
    if(beat_detected) {
        curtain1_speed = curtain1_speed * SQ15x16(1.5);
        curtain2_speed = curtain2_speed * SQ15x16(1.8);
        curtain3_speed = curtain3_speed * SQ15x16(2);
    }
    
    curtain1_offset += curtain1_speed;
    curtain2_offset -= curtain2_speed;
    curtain3_offset += curtain3_speed;
    
    // Curtain frequencies based on density and audio
    SQ15x16 base_freq = SQ15x16(8) + (SQ15x16(frame_config.DENSITY) / 64);
    
    // Audio reactive frequency modulation
    SQ15x16 freq1 = base_freq * (SQ15x16(1) + SQ15x16(low_freq_energy * 0.3));
    SQ15x16 freq2 = freq1 * SQ15x16(1.1) * (SQ15x16(1) + SQ15x16(mid_freq_energy * 0.4));
    SQ15x16 freq3 = freq1 * SQ15x16(0.9) * (SQ15x16(1) + SQ15x16(high_freq_energy * 0.5));
    
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        SQ15x16 pos = SQ15x16(i);
        
        // Calculate three overlapping curtain patterns - audio reactive
        uint8_t curtain1 = 128 + (sin_lookup((pos * freq1 + curtain1_offset).getInteger() & 0xFF) * 127).getInteger();
        uint8_t curtain2 = 128 + (sin_lookup((pos * freq2 + curtain2_offset).getInteger() & 0xFF) * 127).getInteger();
        uint8_t curtain3 = 128 + (sin_lookup((pos * freq3 + curtain3_offset).getInteger() & 0xFF) * 127).getInteger();
        
        // Audio reactive curtain amplitude modulation
        curtain1 = scale8(curtain1, 255 - (uint8_t)(high_freq_energy * 100));
        curtain2 = scale8(curtain2, 255 - (uint8_t)(mid_freq_energy * 80));
        curtain3 = scale8(curtain3, 255 - (uint8_t)(low_freq_energy * 60));
        
        // Moiré pattern emerges from multiplication - audio reactive
        uint16_t moire = (curtain1 * curtain2) / 255;
        
        // Add third curtain for complexity if density is high or audio energy is high
        if (frame_config.DENSITY > 128 || high_freq_energy > 0.3f) {
            moire = (moire * curtain3) / 255;
        }
        
        // Beat detection creates additional interference patterns
        if(beat_detected) {
            uint8_t beat_pattern = sin8((pos * 30 + curtain1_offset * 2).getInteger() & 0xFF);
            moire = (moire * beat_pattern) / 255;
        }
        
        // Apply contrast enhancement - audio reactive
        if (moire > 128) {
            // High frequency energy increases contrast
            uint16_t contrast_factor = 150 + (uint16_t)(high_freq_energy * 100);
            moire = 128 + ((moire - 128) * contrast_factor / 100);
        } else {
            // Low frequency energy affects shadow detail
            uint16_t shadow_factor = 75 + (uint16_t)(low_freq_energy * 50);
            moire = moire * shadow_factor / 100;
        }
        uint8_t brightness = constrain(moire, 0, 255);
        
        // Audio reactive brightness boost
        brightness = (uint8_t)(brightness * (1.0f + mid_freq_energy * 0.4f));
        if(brightness > 255) brightness = 255;
        
        // Distance fade from center - audio reactive
        SQ15x16 dist = abs(pos - SQ15x16(NATIVE_RESOLUTION/2));
        // Low frequency energy reduces fade (more uniform lighting)
        SQ15x16 fade_amount = SQ15x16(0.3) - SQ15x16(low_freq_energy * 0.15);
        if(fade_amount < SQ15x16(0.1)) fade_amount = SQ15x16(0.1);
        SQ15x16 fade_factor = SQ15x16(1) - (dist / (NATIVE_RESOLUTION/2)) * fade_amount;
        brightness = (brightness * fade_factor).getInteger();
        
        // Color calculation
        CRGB16 color;
        if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
            // Use moiré value to index into palette
            uint8_t pal_index = moire + (curtain1_offset * 10).getInteger();
            color = palette_to_crgb16(palette_arr[frame_config.PALETTE],
                                    pal_index, brightness);
        } else if (frame_config.COLOR_MODE == COLOR_MODE_HYBRID) {
            // Hue shifts with moiré pattern - audio reactive
            uint8_t hue = frame_config.HUE + (moire / 4);
            // Mid frequency energy adds additional hue variation
            hue += (uint8_t)(mid_freq_energy * 50);
            color = hsv_to_rgb_fast(hue, frame_config.SATURATION, brightness);
        } else {
            // Pure HSV with subtle variation - audio reactive
            uint8_t hue = frame_config.HUE + sin8(moire) / 16;
            // High frequency energy creates color shifts
            hue += (uint8_t)(high_freq_energy * 40);
            color = hsv_to_rgb_fast(hue, frame_config.SATURATION, brightness);
        }
        
        // Add shimmer to enhance the curtain effect - audio reactive
        if (frame_config.DENSITY > 180 || high_freq_energy > 0.2f) {
            uint8_t shimmer = inoise8(i * 5, millis() / 30);
            // High frequency energy lowers shimmer threshold
            uint8_t shimmer_threshold = 200 - (uint8_t)(high_freq_energy * 100);
            if(shimmer_threshold < 150) shimmer_threshold = 150;
            
            if (shimmer > shimmer_threshold) {
                SQ15x16 shimmer_intensity = SQ15x16(1.3) + SQ15x16(high_freq_energy * 0.5);
                color = scale_color(color, shimmer_intensity);
            }
        }
        
        leds_16[i] = color;
    }
    
    // Audio reactive interference ripples
    if(beat_detected) {
        for(int i = 0; i < NATIVE_RESOLUTION; i++) {
            SQ15x16 ripple_wave = sin_lookup((i * 8 + curtain1_offset * 3).getInteger() & 0xFF);
            if(ripple_wave > SQ15x16(0.5)) {
                uint8_t ripple_intensity = ((ripple_wave - SQ15x16(0.5)) * 255 * 2).getInteger();
                CRGB16 ripple_color = CRGB16(ripple_intensity * 150, ripple_intensity * 100, ripple_intensity * 200);
                leds_16[i] = add_clipped(leds_16[i], ripple_color);
            }
        }
    }
    
    // High frequency creates curtain "tears" effect
    if(high_freq_energy > 0.5f) {
        for(int i = 0; i < NATIVE_RESOLUTION; i++) {
            if(random8() < (uint8_t)(high_freq_energy * 60)) {
                // Create a "tear" in the curtain
                leds_16[i] = scale_color(leds_16[i], SQ15x16(0.3));
            }
        }
    }
    
    apply_global_brightness();
}