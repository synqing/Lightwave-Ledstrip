#include "lgp_radial_ripple_effect.h"
#include "../src/globals.h"
#include "../src/GDFT.h"

#define MAX_RADIAL_SOURCES 3

struct RadialSource {
    SQ15x16 phase;
    SQ15x16 frequency;
    uint8_t position;  // Position on strip
    uint8_t strength;
};

static RadialSource sources[MAX_RADIAL_SOURCES];
static bool sources_initialized = false;

void light_mode_lgp_radial_ripple() {
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
    
    float mid_freq_energy = 0;
    for(int i = 20; i < 50; i++) {
        mid_freq_energy += spectrogram_smooth[i];
    }
    
    float high_freq_energy = 0;
    for(int i = 50; i < 96; i++) {
        high_freq_energy += spectrogram_smooth[i];
    }
    
    // Beat detection for new ripple sources
    static float last_bass_energy = 0;
    float current_bass_energy = chromagram_smooth[0] + chromagram_smooth[1] + chromagram_smooth[2];
    float bass_delta = current_bass_energy - last_bass_energy;
    bool beat_detected = (bass_delta > 0.3f && current_bass_energy > 0.5f);
    last_bass_energy = current_bass_energy * 0.9f + last_bass_energy * 0.1f;
    
    // Initialize sources
    if (!sources_initialized) {
        sources[0].position = NATIVE_RESOLUTION / 4;
        sources[0].frequency = SQ15x16(1.0);
        sources[0].strength = 255;
        
        sources[1].position = NATIVE_RESOLUTION / 2;
        sources[1].frequency = SQ15x16(0.7);
        sources[1].strength = 200;
        
        sources[2].position = NATIVE_RESOLUTION * 3 / 4;
        sources[2].frequency = SQ15x16(1.3);
        sources[2].strength = 180;
        
        sources_initialized = true;
    }
    
    // Update source phases - audio reactive
    SQ15x16 base_speed = SQ15x16(frame_config.SPEED) / 64;
    
    for (int s = 0; s < MAX_RADIAL_SOURCES; s++) {
        // Each source responds to different frequency bands
        SQ15x16 audio_speed_factor;
        if(s == 0) {
            audio_speed_factor = SQ15x16(1) + SQ15x16(low_freq_energy * 1.5);
        } else if(s == 1) {
            audio_speed_factor = SQ15x16(1) + SQ15x16(mid_freq_energy * 1.2);
        } else {
            audio_speed_factor = SQ15x16(1) + SQ15x16(high_freq_energy * 2);
        }
        
        SQ15x16 speed = base_speed * audio_speed_factor;
        sources[s].phase += speed * sources[s].frequency;
        
        // Beat detection creates phase jumps
        if(beat_detected) {
            sources[s].phase += SQ15x16(random8(100));
        }
        
        // Slowly move sources - audio reactive movement
        if (frame_config.DENSITY > 128 || mid_freq_energy > 0.3f) {
            SQ15x16 movement_factor = SQ15x16(1) + SQ15x16(mid_freq_energy * 0.8);
            sources[s].position += sin_lookup((sources[s].phase / 10).getInteger() & 0xFF).getInteger() * movement_factor.getInteger() / 128;
            sources[s].position = constrain(sources[s].position, 10, NATIVE_RESOLUTION - 10);
        }
        
        // Audio reactive strength modulation
        if(s == 0) {
            sources[s].strength = 255 * (0.8f + low_freq_energy * 0.4f);
        } else if(s == 1) {
            sources[s].strength = 200 * (0.8f + mid_freq_energy * 0.4f);
        } else {
            sources[s].strength = 180 * (0.8f + high_freq_energy * 0.4f);
        }
        if(sources[s].strength > 255) sources[s].strength = 255;
    }
    
    // Clear strip
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        leds_16[i] = CRGB16(0, 0, 0);
    }
    
    // Calculate ripple pattern
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        SQ15x16 total_wave = 0;
        SQ15x16 total_strength = 0;
        
        // Sum waves from all sources
        for (int s = 0; s < MAX_RADIAL_SOURCES; s++) {
            // Distance from this source
            SQ15x16 dist = abs(SQ15x16(i) - SQ15x16(sources[s].position));
            
            // Radial wave with decay - audio reactive
            SQ15x16 decay = SQ15x16(1) - (dist / (NATIVE_RESOLUTION/2));
            if (decay > 0) {
                // Wave calculation - audio reactive wavelength
                SQ15x16 wavelength = SQ15x16(0.5);
                if(s == 0) {
                    wavelength = wavelength * (SQ15x16(1) + SQ15x16(low_freq_energy * 0.3));
                } else if(s == 1) {
                    wavelength = wavelength * (SQ15x16(1) + SQ15x16(mid_freq_energy * 0.4));
                } else {
                    wavelength = wavelength * (SQ15x16(1) + SQ15x16(high_freq_energy * 0.5));
                }
                
                SQ15x16 wave_arg = dist * wavelength - sources[s].phase;
                SQ15x16 wave = sin_lookup(wave_arg.getInteger() & 0xFF);
                
                // Apply decay and strength
                wave = wave * decay * (SQ15x16(sources[s].strength) / 255);
                
                total_wave += wave;
                total_strength += SQ15x16(sources[s].strength) / 255;
            }
        }
        
        // Normalize
        if (total_strength > 0) {
            total_wave = total_wave / total_strength;
        }
        
        // Convert to brightness - audio reactive
        uint8_t brightness = 128 + (total_wave * 127).getInteger();
        
        // Audio reactive brightness boost
        brightness = (uint8_t)(brightness * (1.0f + low_freq_energy * 0.3f));
        if(brightness > 255) brightness = 255;
        
        // Add interference pattern - audio reactive
        if (frame_config.DENSITY > 180 || high_freq_energy > 0.3f) {
            uint8_t interference = sin8((i * 8) + (sources[0].phase * 2).getInteger());
            // High frequency energy increases interference
            uint8_t interference_amount = 64 + (uint8_t)(high_freq_energy * 80);
            brightness = (brightness + scale8(interference, interference_amount)) / 2;
        }
        
        // Color mapping
        CRGB16 color;
        if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
            uint8_t pal_index = brightness + (i / 4);
            color = palette_to_crgb16(palette_arr[frame_config.PALETTE],
                                    pal_index, brightness);
        } else if (frame_config.COLOR_MODE == COLOR_MODE_HYBRID) {
            // Hue varies with wave amplitude - audio reactive
            uint8_t hue = frame_config.HUE + (total_wave * 30).getInteger();
            // Mid frequency energy adds color variation
            hue += (uint8_t)(mid_freq_energy * 50);
            color = hsv_to_rgb_fast(hue, frame_config.SATURATION, brightness);
        } else {
            // Pure HSV - audio reactive hue shift
            uint8_t hue = frame_config.HUE + (uint8_t)(high_freq_energy * 40);
            color = hsv_to_rgb_fast(hue, frame_config.SATURATION, brightness);
        }
        
        // Add source indicators - audio reactive
        for (int s = 0; s < MAX_RADIAL_SOURCES; s++) {
            if (abs(i - sources[s].position) < 2) {
                uint8_t source_hue = (s * 85) + (uint8_t)(high_freq_energy * 60);
                uint8_t source_brightness = 255;
                
                // Beat detection makes sources brighter
                if(beat_detected) {
                    source_brightness = 255;
                } else {
                    source_brightness = 150 + (uint8_t)(low_freq_energy * 100);
                }
                
                CRGB16 source_color = hsv_to_rgb_fast(source_hue, 255, source_brightness);
                SQ15x16 source_intensity = SQ15x16(0.5) + SQ15x16(mid_freq_energy * 0.4);
                color = add_clipped(color, scale_color(source_color, source_intensity));
            }
        }
        
        leds_16[i] = color;
    }
    
    // Beat detection creates new temporary ripple
    if(beat_detected) {
        static uint8_t beat_ripple_pos = NATIVE_RESOLUTION / 2;
        static SQ15x16 beat_ripple_phase = 0;
        
        // Random new position for beat ripple
        beat_ripple_pos = random8(NATIVE_RESOLUTION / 4) + NATIVE_RESOLUTION * 3/8;
        beat_ripple_phase = 0;
        
        for(int i = 0; i < NATIVE_RESOLUTION; i++) {
            SQ15x16 dist = abs(SQ15x16(i) - SQ15x16(beat_ripple_pos));
            if(dist < SQ15x16(30)) {
                SQ15x16 beat_wave = sin_lookup((dist * 2 - beat_ripple_phase).getInteger() & 0xFF);
                uint8_t beat_brightness = ((SQ15x16(1) - dist / 30) * beat_wave * 150).getInteger();
                
                CRGB16 beat_color = hsv_to_rgb_fast(frame_config.HUE + 60, 255, beat_brightness);
                leds_16[i] = add_clipped(leds_16[i], beat_color);
            }
        }
    }
    
    // High frequency creates ripple shimmer
    if(high_freq_energy > 0.4f) {
        for(int i = 0; i < NATIVE_RESOLUTION; i++) {
            if(random8() < (uint8_t)(high_freq_energy * 80)) {
                leds_16[i] = scale_color(leds_16[i], SQ15x16(1.4));
            }
        }
    }
    
    apply_global_brightness();
}