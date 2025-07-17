#include "lgp_concentric_rings_effect.h"
#include "../src/globals.h"
#include "../src/GDFT.h"

static SQ15x16 ring_phase = 0;
static SQ15x16 modulation_phase = 0;

void light_mode_lgp_concentric_rings() {
    cache_frame_config();
    
    // Audio integration for ring dynamics
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
    
    // Beat detection for ring expansion bursts
    static float last_bass_energy = 0;
    float current_bass_energy = chromagram_smooth[0] + chromagram_smooth[1] + chromagram_smooth[2];
    float bass_delta = current_bass_energy - last_bass_energy;
    bool beat_detected = (bass_delta > 0.3f && current_bass_energy > 0.5f);
    last_bass_energy = current_bass_energy * 0.9f + last_bass_energy * 0.1f;
    
    // Ring expansion speed - audio reactive
    SQ15x16 base_speed = SQ15x16(frame_config.SPEED) / 64;
    // Mid-frequency energy controls speed
    SQ15x16 audio_speed_multiplier = SQ15x16(1) + SQ15x16(mid_freq_energy * 2);
    SQ15x16 speed = base_speed * audio_speed_multiplier;
    
    // Beat detection creates speed bursts
    if(beat_detected) {
        speed = speed * SQ15x16(2);
    }
    
    ring_phase += speed;
    
    // Modulation for ring distortion
    modulation_phase += speed * SQ15x16(0.3);
    
    // Ring spacing based on density and audio
    SQ15x16 base_spacing = SQ15x16(5) + (SQ15x16(frame_config.DENSITY) / 32);
    // Low frequency energy affects ring spacing
    SQ15x16 audio_spacing_factor = SQ15x16(1) + SQ15x16(low_freq_energy * 0.5);
    SQ15x16 ring_spacing = base_spacing * audio_spacing_factor;
    
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        // Distance from center
        SQ15x16 dist = abs(SQ15x16(i) - SQ15x16(NATIVE_RESOLUTION/2));
        
        // Apply modulation for wavy rings - audio reactive
        SQ15x16 modulation = sin_lookup((i + modulation_phase.getInteger()) & 0xFF) * SQ15x16(0.2);
        // High frequency energy increases modulation
        SQ15x16 audio_modulation = SQ15x16(1) + SQ15x16(high_freq_energy * 0.8);
        dist = dist + modulation * (SQ15x16(frame_config.DENSITY) / 255) * audio_modulation;
        
        // Calculate which ring we're in
        SQ15x16 ring_position = (dist + ring_phase) / ring_spacing;
        SQ15x16 ring_frac = ring_position - SQ15x16(ring_position.getInteger());
        
        // Create sharp or soft rings based on density
        uint8_t brightness;
        if (frame_config.DENSITY < 128) {
            // Sharp rings
            brightness = ring_frac < SQ15x16(0.5) ? 255 : 0;
        } else {
            // Smooth gradient rings
            brightness = sin8((ring_frac * 255).getInteger());
        }
        
        // Add pulsing effect - audio reactive
        uint8_t pulse = sin8((ring_phase * 4).getInteger() & 0xFF);
        // Low frequency energy affects pulse intensity
        uint8_t pulse_intensity = 55 + (uint8_t)(low_freq_energy * 100);
        if(pulse_intensity > 100) pulse_intensity = 100;
        brightness = scale8(brightness, 200 + scale8(pulse, pulse_intensity));
        
        // Beat detection creates brightness bursts
        if(beat_detected) {
            brightness = qadd8(brightness, 50);
        }
        
        // Color based on ring number and mode
        CRGB16 color;
        if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
            uint8_t pal_index = (ring_position * 32).getInteger();
            color = palette_to_crgb16(palette_arr[frame_config.PALETTE],
                                    pal_index, brightness);
        } else if (frame_config.COLOR_MODE == COLOR_MODE_HYBRID) {
            // Hybrid: hue changes with radius - audio reactive
            uint8_t hue = frame_config.HUE + (dist / 2).getInteger();
            // High frequency energy shifts hue
            hue += (uint8_t)(high_freq_energy * 40);
            color = hsv_to_rgb_fast(hue, frame_config.SATURATION, brightness);
        } else {
            // Pure HSV with slight variation - audio reactive
            uint8_t hue = frame_config.HUE + sin8((ring_position * 16).getInteger() & 0xFF) / 8;
            // Mid frequency energy adds color variation
            hue += (uint8_t)(mid_freq_energy * 30);
            color = hsv_to_rgb_fast(hue, frame_config.SATURATION, brightness);
        }
        
        // Add interference between rings - audio reactive
        if (frame_config.DENSITY > 180) {
            SQ15x16 ring2_position = (dist - ring_phase * SQ15x16(0.7)) / (ring_spacing * SQ15x16(1.5));
            SQ15x16 ring2_frac = ring2_position - SQ15x16(ring2_position.getInteger());
            uint8_t interference = sin8((ring2_frac * 255).getInteger());
            
            // High frequency energy increases interference
            SQ15x16 interference_strength = SQ15x16(0.3) + SQ15x16(high_freq_energy * 0.4);
            if(interference_strength > SQ15x16(0.8)) interference_strength = SQ15x16(0.8);
            
            CRGB16 interference_color = scale_color(color, SQ15x16(interference) / 255);
            color = add_clipped(color, scale_color(interference_color, interference_strength));
        }
        
        leds_16[i] = color;
    }
    
    // Audio reactive shimmer effect
    if(high_freq_energy > 0.3f) {
        for(int i = 0; i < NATIVE_RESOLUTION; i++) {
            if(random8() < (uint8_t)(high_freq_energy * 100)) {
                leds_16[i] = scale_color(leds_16[i], SQ15x16(1.3));
            }
        }
    }
    
    apply_global_brightness();
}