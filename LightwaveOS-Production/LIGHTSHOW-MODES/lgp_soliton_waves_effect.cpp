#include "lgp_soliton_waves_effect.h"
#include "../src/globals.h"
#include "../src/GDFT.h"

#define MAX_SOLITONS 4

struct Soliton {
    SQ15x16 position;
    SQ15x16 velocity;
    SQ15x16 amplitude;
    SQ15x16 width;
    uint8_t hue;
    bool active;
    bool forward;  // Direction
};

static Soliton solitons[MAX_SOLITONS];
static uint32_t last_spawn = 0;

// Sech squared function for soliton shape
SQ15x16 sech_squared(SQ15x16 x) {
    // Approximation: sech²(x) ≈ 1 / (1 + x²)
    SQ15x16 x_squared = x * x;
    return SQ15x16(1) / (SQ15x16(1) + x_squared);
}

void light_mode_lgp_soliton_waves() {
    cache_frame_config();
    
    // Audio integration for soliton dynamics
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
    
    // Beat detection for soliton spawning
    static float last_bass_energy = 0;
    float current_bass_energy = chromagram_smooth[0] + chromagram_smooth[1] + chromagram_smooth[2];
    float bass_delta = current_bass_energy - last_bass_energy;
    bool beat_detected = (bass_delta > 0.3f && current_bass_energy > 0.5f);
    last_bass_energy = current_bass_energy * 0.9f + last_bass_energy * 0.1f;
    
    // Spawn new solitons - audio reactive
    uint32_t now = millis();
    uint32_t base_interval = 2000 - (frame_config.SPEED * 7);
    // High frequency energy reduces spawn interval
    uint32_t spawn_interval = base_interval - (uint32_t)(high_freq_energy * 1000);
    if(spawn_interval < 300) spawn_interval = 300;
    
    // Beat detection forces soliton spawn
    if (beat_detected || now - last_spawn > spawn_interval) {
        for (int s = 0; s < MAX_SOLITONS; s++) {
            if (!solitons[s].active) {
                // Initialize soliton
                solitons[s].active = true;
                solitons[s].forward = random8(2);
                solitons[s].position = solitons[s].forward ? 0 : NATIVE_RESOLUTION - 1;
                
                // Soliton properties (amplitude and width are related) - audio reactive
                SQ15x16 base_amplitude = SQ15x16(0.5) + (SQ15x16(random8()) / 255) * SQ15x16(0.5);
                // Low frequency energy increases amplitude
                solitons[s].amplitude = base_amplitude * (SQ15x16(1) + SQ15x16(low_freq_energy * 0.6));
                if(solitons[s].amplitude > SQ15x16(1.5)) solitons[s].amplitude = SQ15x16(1.5);
                
                solitons[s].width = SQ15x16(10) / solitons[s].amplitude;  // Inverse relationship
                solitons[s].velocity = solitons[s].amplitude * SQ15x16(0.3);  // Velocity proportional to amplitude
                
                // Mid frequency energy affects velocity
                solitons[s].velocity = solitons[s].velocity * (SQ15x16(1) + SQ15x16(mid_freq_energy * 0.4));
                
                if (!solitons[s].forward) {
                    solitons[s].velocity = -solitons[s].velocity;
                }
                
                // Color - audio reactive
                if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
                    solitons[s].hue = random8() + (uint8_t)(high_freq_energy * 100);
                } else {
                    solitons[s].hue = frame_config.HUE + (s * 60) + (uint8_t)(high_freq_energy * 60);
                }
                
                last_spawn = now;
                break;
            }
        }
    }
    
    // Very slow fade for wave persistence - audio reactive
    SQ15x16 fade_factor = SQ15x16(0.95);
    // High frequency energy increases fade rate (less persistence)
    fade_factor = fade_factor - SQ15x16(high_freq_energy * 0.05);
    if(fade_factor < SQ15x16(0.85)) fade_factor = SQ15x16(0.85);
    
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        leds_16[i] = scale_color(leds_16[i], fade_factor);
    }
    
    // Update and render solitons
    for (int s = 0; s < MAX_SOLITONS; s++) {
        if (!solitons[s].active) continue;
        
        // Update position - audio reactive
        SQ15x16 speed_factor = SQ15x16(frame_config.SPEED) / 64;
        // Mid frequency energy affects global speed
        speed_factor = speed_factor * (SQ15x16(1) + SQ15x16(mid_freq_energy * 0.5));
        solitons[s].position += solitons[s].velocity * speed_factor;
        
        // Check bounds
        if (solitons[s].position < -20 || solitons[s].position > NATIVE_RESOLUTION + 20) {
            solitons[s].active = false;
            continue;
        }
        
        // Render soliton
        for (int i = 0; i < NATIVE_RESOLUTION; i++) {
            // Distance from soliton center
            SQ15x16 x = (SQ15x16(i) - solitons[s].position) / solitons[s].width;
            
            // Soliton wave shape (sech² profile) - audio reactive
            SQ15x16 wave_amplitude = solitons[s].amplitude * sech_squared(x);
            
            if (wave_amplitude > SQ15x16(0.01)) {
                uint8_t brightness = (wave_amplitude * 255).getInteger();
                
                // Audio reactive brightness boost
                brightness = (uint8_t)(brightness * (1.0f + low_freq_energy * 0.3f));
                if(brightness > 255) brightness = 255;
                
                // Add nonlinear effects for high density or high frequency energy
                if (frame_config.DENSITY > 180 || high_freq_energy > 0.3f) {
                    // Self-focusing effect - enhanced by audio
                    uint8_t nonlinear_boost = brightness / 4 + (uint8_t)(high_freq_energy * 50);
                    brightness = qadd8(brightness, scale8(brightness, nonlinear_boost));
                }
                
                // Color
                CRGB16 color;
                if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
                    uint8_t pal_index = solitons[s].hue + (wave_amplitude * 50).getInteger();
                    color = palette_to_crgb16(palette_arr[frame_config.PALETTE],
                                            pal_index, brightness);
                } else if (frame_config.COLOR_MODE == COLOR_MODE_HYBRID) {
                    // Color shifts with amplitude (dispersion) - audio reactive
                    uint8_t hue = solitons[s].hue + (wave_amplitude * 20).getInteger();
                    // High frequency energy adds color dispersion
                    hue += (uint8_t)(high_freq_energy * 40);
                    color = hsv_to_rgb_fast(hue, frame_config.SATURATION, brightness);
                } else {
                    // Audio reactive hue modulation
                    uint8_t hue = solitons[s].hue + (uint8_t)(mid_freq_energy * 30);
                    color = hsv_to_rgb_fast(hue, frame_config.SATURATION, brightness);
                }
                
                leds_16[i] = add_clipped(leds_16[i], color);
            }
        }
    }
    
    // Soliton collision detection and interaction
    for (int s1 = 0; s1 < MAX_SOLITONS - 1; s1++) {
        if (!solitons[s1].active) continue;
        
        for (int s2 = s1 + 1; s2 < MAX_SOLITONS; s2++) {
            if (!solitons[s2].active) continue;
            
            SQ15x16 distance = abs(solitons[s1].position - solitons[s2].position);
            
            // Check for collision - audio reactive
            if (distance < solitons[s1].width + solitons[s2].width) {
                // Solitons pass through each other but create interference
                int collision_point = ((solitons[s1].position + solitons[s2].position) / 2).getInteger();
                
                if (collision_point >= 0 && collision_point < NATIVE_RESOLUTION) {
                    // Bright flash at collision - audio reactive intensity
                    uint8_t flash_range = 5 + (uint8_t)(high_freq_energy * 8);
                    for (int i = max(0, collision_point - flash_range); 
                         i < min(NATIVE_RESOLUTION, collision_point + flash_range); i++) {
                        uint8_t flash = 255 - abs(i - collision_point) * (320 / flash_range);
                        
                        // Audio reactive flash intensity
                        flash = (uint8_t)(flash * (1.0f + low_freq_energy * 0.5f));
                        if(flash > 255) flash = 255;
                        
                        // Color based on collision energy
                        uint8_t collision_hue = (solitons[s1].hue + solitons[s2].hue) / 2;
                        collision_hue += (uint8_t)(mid_freq_energy * 60);
                        
                        CRGB16 flash_color = hsv_to_rgb_fast(collision_hue, 255, flash);
                        leds_16[i] = add_clipped(leds_16[i], flash_color);
                    }
                }
            }
        }
    }
    
    // Beat detection creates soliton burst effect
    if(beat_detected) {
        for(int s = 0; s < MAX_SOLITONS; s++) {
            if(solitons[s].active) {
                // Temporarily increase soliton amplitude
                solitons[s].amplitude = solitons[s].amplitude * SQ15x16(1.3);
                if(solitons[s].amplitude > SQ15x16(2)) solitons[s].amplitude = SQ15x16(2);
                
                // Create wave burst around soliton
                int soliton_pos = solitons[s].position.getInteger();
                for(int i = max(0, soliton_pos - 15); i < min(NATIVE_RESOLUTION, soliton_pos + 15); i++) {
                    SQ15x16 burst_dist = abs(SQ15x16(i) - solitons[s].position);
                    if(burst_dist < SQ15x16(15)) {
                        uint8_t burst_intensity = 255 - (burst_dist * 17).getInteger();
                        CRGB16 burst_color = hsv_to_rgb_fast(solitons[s].hue + 30, 255, burst_intensity);
                        leds_16[i] = add_clipped(leds_16[i], scale_color(burst_color, SQ15x16(0.6)));
                    }
                }
            }
        }
    }
    
    apply_global_brightness();
}