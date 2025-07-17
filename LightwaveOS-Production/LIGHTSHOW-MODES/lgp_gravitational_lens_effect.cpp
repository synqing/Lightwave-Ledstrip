#include "lgp_gravitational_lens_effect.h"
#include "../src/globals.h"
#include "../src/GDFT.h"

// Gravitational source
struct MassiveObject {
    SQ15x16 position;
    SQ15x16 mass;
    SQ15x16 velocity;
};

static MassiveObject black_hole;
static SQ15x16 background_offset = 0;

void light_mode_lgp_gravitational_lens() {
    cache_frame_config();
    
    // Audio integration for gravitational dynamics
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
    
    // Beat detection for gravitational waves
    static float last_bass_energy = 0;
    float current_bass_energy = chromagram_smooth[0] + chromagram_smooth[1] + chromagram_smooth[2];
    float bass_delta = current_bass_energy - last_bass_energy;
    bool beat_detected = (bass_delta > 0.3f && current_bass_energy > 0.5f);
    last_bass_energy = current_bass_energy * 0.9f + last_bass_energy * 0.1f;
    
    // Initialize black hole
    static bool initialized = false;
    if (!initialized) {
        black_hole.position = NATIVE_RESOLUTION / 2;
        black_hole.mass = SQ15x16(1);
        black_hole.velocity = 0;
        initialized = true;
    }
    
    // Move the black hole - audio reactive
    SQ15x16 base_speed = SQ15x16(frame_config.SPEED) / 255;
    // Mid-frequency energy affects movement speed
    SQ15x16 audio_speed_multiplier = SQ15x16(1) + SQ15x16(mid_freq_energy * 1.5);
    SQ15x16 speed = base_speed * audio_speed_multiplier;
    
    black_hole.velocity = sin_lookup((millis() / 100) & 0xFF) * speed * SQ15x16(2);
    
    // Beat detection creates sudden position jumps (gravitational waves)
    if(beat_detected) {
        black_hole.position += SQ15x16(random8(20) - 10);
    }
    
    black_hole.position += black_hole.velocity;
    
    // Keep black hole in bounds
    if (black_hole.position < NATIVE_RESOLUTION * 0.2 || 
        black_hole.position > NATIVE_RESOLUTION * 0.8) {
        black_hole.position = SQ15x16(NATIVE_RESOLUTION / 2);
    }
    
    // Mass varies with density and audio
    SQ15x16 base_mass = SQ15x16(0.5) + (SQ15x16(frame_config.DENSITY) / 255);
    // Low frequency energy increases mass
    SQ15x16 audio_mass_boost = SQ15x16(low_freq_energy * 0.8);
    black_hole.mass = base_mass + audio_mass_boost;
    
    // Background starfield movement - audio reactive
    SQ15x16 bg_speed = speed * SQ15x16(0.5);
    // High frequency energy creates starfield turbulence
    bg_speed = bg_speed * (SQ15x16(1) + SQ15x16(high_freq_energy * 0.6));
    background_offset += bg_speed;
    
    // Render the effect
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        // Calculate gravitational lensing
        SQ15x16 dist = abs(SQ15x16(i) - black_hole.position);
        
        // Schwarzschild radius (event horizon)
        SQ15x16 schwarzschild = black_hole.mass * SQ15x16(5);
        
        if (dist < schwarzschild) {
            // Inside event horizon - black
            leds_16[i] = CRGB16(0, 0, 0);
        } else {
            // Calculate light deflection angle
            SQ15x16 deflection = (black_hole.mass * SQ15x16(30)) / (dist + SQ15x16(1));
            
            // Source position (where light appears to come from)
            SQ15x16 source_pos = SQ15x16(i) + deflection;
            
            // Wrap around for periodic boundary
            while (source_pos < 0) source_pos += NATIVE_RESOLUTION;
            while (source_pos >= NATIVE_RESOLUTION) source_pos -= NATIVE_RESOLUTION;
            
            // Background starfield - audio reactive
            uint16_t star_noise = inoise16(source_pos.getInteger() * 100 + background_offset.getInteger());
            uint8_t star_brightness = 0;
            
            // High frequency energy lowers star threshold (more stars)
            uint16_t star_threshold = 50000 - (uint16_t)(high_freq_energy * 15000);
            if(star_threshold < 35000) star_threshold = 35000;
            
            if (star_noise > star_threshold) {
                // Star detected
                star_brightness = map(star_noise, star_threshold, 65535, 0, 255);
                // Audio reactive star brightness
                star_brightness = (uint8_t)(star_brightness * (1.0f + high_freq_energy * 0.5f));
                if(star_brightness > 255) star_brightness = 255;
            }
            
            // Einstein ring effect near critical radius - audio reactive
            SQ15x16 einstein_radius = schwarzschild * SQ15x16(2.5);
            if (abs(dist - einstein_radius) < 3) {
                // Bright ring - enhanced by audio
                uint8_t ring_brightness = 200 + (uint8_t)(mid_freq_energy * 50);
                if(ring_brightness > 255) ring_brightness = 255;
                star_brightness = qadd8(star_brightness, ring_brightness);
            }
            
            // Gravitational redshift
            SQ15x16 redshift_factor = SQ15x16(1) - (schwarzschild / (dist * SQ15x16(2)));
            
            // Apply magnification near lens - audio reactive
            if (dist < einstein_radius * SQ15x16(2)) {
                SQ15x16 magnification = SQ15x16(1) + (einstein_radius / dist) * SQ15x16(0.5);
                // Low frequency energy increases magnification
                magnification = magnification * (SQ15x16(1) + SQ15x16(low_freq_energy * 0.4));
                star_brightness = qadd8(star_brightness, 
                                       (magnification * 50).getInteger());
            }
            
            // Color with redshift
            CRGB16 color;
            if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
                uint8_t pal_index = (redshift_factor * 255).getInteger();
                color = palette_to_crgb16(palette_arr[frame_config.PALETTE],
                                        pal_index, star_brightness);
            } else {
                // Redshift moves colors toward red - audio reactive
                uint8_t hue;
                if (redshift_factor > SQ15x16(0.8)) {
                    hue = frame_config.HUE;  // Normal color far from hole
                    // High frequency energy adds color variation
                    hue += (uint8_t)(high_freq_energy * 30);
                } else {
                    // Shift toward red as we get closer
                    hue = frame_config.HUE * redshift_factor.getInteger() / 255;
                    // Mid frequency energy affects redshift color
                    hue += (uint8_t)(mid_freq_energy * 20);
                }
                
                color = hsv_to_rgb_fast(hue, frame_config.SATURATION, star_brightness);
            }
            
            // Add accretion disk glow - audio reactive
            if (dist > schwarzschild && dist < schwarzschild * SQ15x16(3)) {
                SQ15x16 disk_intensity = SQ15x16(1) - ((dist - schwarzschild) / (schwarzschild * SQ15x16(2)));
                uint8_t disk_bright = (disk_intensity * 100).getInteger();
                
                // Audio reactive disk brightness
                disk_bright = (uint8_t)(disk_bright * (1.0f + low_freq_energy * 0.8f));
                if(disk_bright > 255) disk_bright = 255;
                
                // Hot accretion disk - blue-white
                CRGB16 disk_color = CRGB16(disk_bright * 200, disk_bright * 200, disk_bright * 256);
                color = add_clipped(color, disk_color);
            }
            
            leds_16[i] = color;
        }
    }
    
    // Add Hawking radiation sparkles at event horizon - audio reactive
    int bh_pos = black_hole.position.getInteger();
    for (int i = -10; i < 10; i++) {
        int pos = bh_pos + i;
        if (pos >= 0 && pos < NATIVE_RESOLUTION) {
            SQ15x16 dist = abs(SQ15x16(i));
            // High frequency energy increases Hawking radiation probability
            uint8_t radiation_prob = 10 + (uint8_t)(high_freq_energy * 30);
            if (dist < black_hole.mass * SQ15x16(5) + SQ15x16(1) && random8() < radiation_prob) {
                // Audio reactive radiation intensity
                uint16_t radiation_intensity = 10000 + (uint16_t)(high_freq_energy * 20000);
                leds_16[pos] = add_clipped(leds_16[pos], 
                                         CRGB16(radiation_intensity, radiation_intensity, radiation_intensity + 10000));
            }
        }
    }
    
    // Beat detection creates gravitational wave ripples
    if(beat_detected) {
        for(int i = 0; i < NATIVE_RESOLUTION; i++) {
            SQ15x16 dist = abs(SQ15x16(i) - black_hole.position);
            if(dist < SQ15x16(30)) {
                uint8_t wave_intensity = 255 - (dist * 8).getInteger();
                CRGB16 wave_color = CRGB16(wave_intensity * 100, wave_intensity * 100, wave_intensity * 200);
                leds_16[i] = add_clipped(leds_16[i], wave_color);
            }
        }
    }
    
    apply_global_brightness();
}