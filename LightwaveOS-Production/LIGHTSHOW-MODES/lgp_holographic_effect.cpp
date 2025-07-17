#include "lgp_holographic_effect.h"
#include "../src/globals.h"
#include "../src/GDFT.h"

// Holographic state
static SQ15x16 phase1 = 0;
static SQ15x16 phase2 = 0;
static SQ15x16 phase3 = 0;

void light_mode_lgp_holographic() {
    cache_frame_config();
    
    // Audio integration for holographic interference
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
    
    // Beat detection for interference pattern changes
    static float last_bass_energy = 0;
    float current_bass_energy = chromagram_smooth[0] + chromagram_smooth[1] + chromagram_smooth[2];
    float bass_delta = current_bass_energy - last_bass_energy;
    bool beat_detected = (bass_delta > 0.3f && current_bass_energy > 0.5f);
    last_bass_energy = current_bass_energy * 0.9f + last_bass_energy * 0.1f;
    
    // Update phases at different rates - audio reactive
    SQ15x16 base_speed = SQ15x16(frame_config.SPEED) / 255;
    
    // Each phase responds to different frequency bands
    SQ15x16 phase1_speed = base_speed * SQ15x16(0.03) * (SQ15x16(1) + SQ15x16(low_freq_energy * 2));
    SQ15x16 phase2_speed = base_speed * SQ15x16(0.021) * (SQ15x16(1) + SQ15x16(mid_freq_energy * 1.5));
    SQ15x16 phase3_speed = base_speed * SQ15x16(0.017) * (SQ15x16(1) + SQ15x16(high_freq_energy * 3));
    
    phase1 += phase1_speed;
    phase2 -= phase2_speed;
    phase3 += phase3_speed;
    
    // Number of interference sources based on density and audio
    int base_sources = 2 + (frame_config.DENSITY / 85);  // 2-4 sources
    // High frequency energy can add extra sources
    int audio_sources = (int)(high_freq_energy * 2);
    int num_sources = base_sources + audio_sources;
    if(num_sources > 6) num_sources = 6;
    
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        SQ15x16 pos = SQ15x16(i) / NATIVE_RESOLUTION;
        
        // Calculate interference from multiple sources
        SQ15x16 interference = 0;
        
        // Source 1: Primary wave - audio reactive frequency
        SQ15x16 wave1_freq = SQ15x16(20) + SQ15x16(low_freq_energy * 10);
        SQ15x16 wave1 = sin_lookup((pos * wave1_freq + phase1).getInteger() & 0xFF);
        interference += wave1;
        
        // Source 2: Counter-propagating wave - audio reactive frequency
        SQ15x16 wave2_freq = SQ15x16(15) + SQ15x16(mid_freq_energy * 8);
        SQ15x16 wave2 = sin_lookup((pos * wave2_freq - phase2).getInteger() & 0xFF);
        SQ15x16 wave2_amplitude = SQ15x16(0.8) + SQ15x16(mid_freq_energy * 0.3);
        interference += wave2 * wave2_amplitude;
        
        if (num_sources > 2) {
            // Source 3: High frequency modulation - audio reactive
            SQ15x16 wave3_freq = SQ15x16(40) + SQ15x16(high_freq_energy * 20);
            SQ15x16 wave3 = sin_lookup((pos * wave3_freq + phase3).getInteger() & 0xFF);
            SQ15x16 wave3_amplitude = SQ15x16(0.5) + SQ15x16(high_freq_energy * 0.4);
            interference += wave3 * wave3_amplitude;
        }
        
        if (num_sources > 3) {
            // Source 4: Low frequency envelope - audio reactive
            SQ15x16 wave4_freq = SQ15x16(5) + SQ15x16(low_freq_energy * 3);
            SQ15x16 wave4 = sin_lookup((pos * wave4_freq + phase1 * SQ15x16(0.5)).getInteger() & 0xFF);
            SQ15x16 wave4_amplitude = SQ15x16(0.3) + SQ15x16(low_freq_energy * 0.2);
            interference += wave4 * wave4_amplitude;
        }
        
        // Additional audio-reactive sources
        if (num_sources > 4) {
            // Source 5: Beat-reactive wave
            SQ15x16 wave5 = sin_lookup((pos * 25 + phase2 * SQ15x16(1.3)).getInteger() & 0xFF);
            SQ15x16 wave5_amplitude = SQ15x16(0.4) + SQ15x16(mid_freq_energy * 0.3);
            interference += wave5 * wave5_amplitude;
        }
        
        if (num_sources > 5) {
            // Source 6: High-frequency shimmer
            SQ15x16 wave6 = sin_lookup((pos * 60 + phase3 * SQ15x16(1.5)).getInteger() & 0xFF);
            SQ15x16 wave6_amplitude = SQ15x16(0.2) + SQ15x16(high_freq_energy * 0.3);
            interference += wave6 * wave6_amplitude;
        }
        
        // Normalize interference pattern
        interference = interference / num_sources;
        
        // Add shimmer using noise - audio reactive
        uint8_t shimmer = inoise8(i * 10, millis() / 10);
        SQ15x16 shimmer_factor = SQ15x16(shimmer) / 255;
        
        // High frequency energy increases shimmer intensity
        shimmer_factor = shimmer_factor * (SQ15x16(1) + SQ15x16(high_freq_energy * 0.8));
        
        // Calculate brightness from interference
        uint8_t brightness = 128 + (interference * 127).getInteger();
        
        // Audio reactive brightness scaling
        uint8_t base_scale = 200 + (shimmer_factor * 55).getInteger();
        uint8_t audio_brightness_boost = (uint8_t)(low_freq_energy * 50);
        brightness = scale8(brightness, base_scale + audio_brightness_boost);
        
        // Beat detection creates brightness bursts
        if(beat_detected) {
            brightness = qadd8(brightness, 40);
        }
        
        // Color calculation with iridescent effect
        CRGB16 color;
        if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
            // Use palette with interference-based offset
            uint8_t pal_index = (interference * 255).getInteger() + shimmer / 4;
            color = palette_to_crgb16(palette_arr[frame_config.PALETTE],
                                    pal_index, brightness);
        } else {
            // Holographic color shift based on interference - audio reactive
            uint8_t hue_shift = (interference * 30).getInteger() + shimmer / 8;
            // Mid and high frequency energy affects color shifting
            hue_shift += (uint8_t)(mid_freq_energy * 40) + (uint8_t)(high_freq_energy * 30);
            uint8_t hue = frame_config.HUE + hue_shift;
            
            // Reduce saturation for holographic effect - audio reactive
            uint8_t sat = scale8(frame_config.SATURATION, 200);
            // Low frequency energy can increase saturation
            sat = qadd8(sat, (uint8_t)(low_freq_energy * 50));
            
            color = hsv_to_rgb_fast(hue, sat, brightness);
        }
        
        // Add spectral dispersion at edges - audio reactive
        SQ15x16 edge_distance = abs(pos - SQ15x16(0.5)) * 2;
        // High frequency energy increases dispersion range
        SQ15x16 dispersion_threshold = SQ15x16(0.7) - SQ15x16(high_freq_energy * 0.2);
        if(dispersion_threshold < SQ15x16(0.4)) dispersion_threshold = SQ15x16(0.4);
        
        if (edge_distance > dispersion_threshold) {
            // Add rainbow edges - audio reactive intensity
            uint8_t rainbow_amount = ((edge_distance - dispersion_threshold) * 255 * 3).getInteger();
            rainbow_amount = (uint8_t)(rainbow_amount * (1.0f + high_freq_energy * 0.5f));
            if(rainbow_amount > 255) rainbow_amount = 255;
            
            uint8_t rainbow_hue = (i * 2) + (uint8_t)(mid_freq_energy * 60);
            CRGB16 rainbow = hsv_to_rgb_fast(rainbow_hue, 255, rainbow_amount);
            color = add_clipped(color, rainbow);
        }
        
        leds_16[i] = color;
    }
    
    // Audio reactive interference pattern overlay
    if(beat_detected) {
        for(int i = 0; i < NATIVE_RESOLUTION; i++) {
            if(random8() < 50) {
                // Random interference flashes on beat
                leds_16[i] = add_clipped(leds_16[i], CRGB16(20000, 20000, 30000));
            }
        }
    }
    
    // High frequency creates holographic "glitches"
    if(high_freq_energy > 0.4f) {
        for(int i = 0; i < NATIVE_RESOLUTION; i++) {
            if(random8() < (uint8_t)(high_freq_energy * 80)) {
                // Color shift glitch
                CRGB16 glitch = leds_16[i];
                glitch.r = glitch.g;
                glitch.g = glitch.b;
                glitch.b = glitch.r;
                leds_16[i] = glitch;
            }
        }
    }
    
    apply_global_brightness();
}