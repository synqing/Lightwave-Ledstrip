#include "lgp_star_burst_effect.h"

static SQ15x16 rotation_angle = 0;
static SQ15x16 pulse_phase = 0;

void light_mode_lgp_star_burst() {
    cache_frame_config();
    
    // Rotation speed
    SQ15x16 speed = SQ15x16(frame_config.SPEED) / 128;
    rotation_angle += speed;
    pulse_phase += speed * SQ15x16(2);
    
    // Number of rays based on density
    int num_rays = 3 + (frame_config.DENSITY / 51);  // 3-8 rays
    
    // Clear the strip
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        leds_16[i] = CRGB16(0, 0, 0);
    }
    
    // Draw each ray
    for (int ray = 0; ray < num_rays; ray++) {
        // Ray angle
        SQ15x16 ray_angle = (SQ15x16(ray) * SQ15x16(256) / num_rays) + rotation_angle;
        
        // Pulse timing for this ray
        SQ15x16 ray_pulse = pulse_phase + (SQ15x16(ray) * SQ15x16(32));
        uint8_t pulse_bright = 128 + (sin_lookup(ray_pulse.getInteger() & 0xFF) * 127).getInteger();
        
        // Draw ray from center outward
        for (int i = 0; i < NATIVE_RESOLUTION; i++) {
            SQ15x16 dist = abs(SQ15x16(i) - SQ15x16(NATIVE_RESOLUTION/2));
            
            // Calculate ray intensity based on angular position
            // In 1D, we simulate this with distance-based patterns
            SQ15x16 ray_pattern = sin_lookup((dist * 4 + ray_angle).getInteger() & 0xFF);
            
            // Ray width varies with distance
            SQ15x16 width_factor = SQ15x16(1) - (dist / (NATIVE_RESOLUTION/2));
            ray_pattern = ray_pattern * width_factor;
            
            // Only draw positive parts of the wave (the rays)
            if (ray_pattern > 0) {
                uint8_t ray_brightness = (ray_pattern * pulse_bright).getInteger();
                
                // Add shimmer
                uint8_t shimmer = inoise8(i * 20, ray * 100, millis() / 20);
                ray_brightness = scale8(ray_brightness, 200 + scale8(shimmer, 55));
                
                // Color for this ray
                CRGB16 color;
                if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
                    uint8_t pal_index = (ray * 255 / num_rays) + (dist / 2).getInteger();
                    color = palette_to_crgb16(palette_arr[frame_config.PALETTE],
                                            pal_index, ray_brightness);
                } else if (frame_config.COLOR_MODE == COLOR_MODE_HYBRID) {
                    // Each ray gets a different hue
                    uint8_t ray_hue = frame_config.HUE + (ray * 255 / num_rays);
                    color = hsv_to_rgb_fast(ray_hue, frame_config.SATURATION, ray_brightness);
                } else {
                    // All rays same hue, vary saturation
                    uint8_t sat = scale8(frame_config.SATURATION, 255 - (ray * 30));
                    color = hsv_to_rgb_fast(frame_config.HUE, sat, ray_brightness);
                }
                
                // Add glow at center
                if (dist < 5) {
                    uint8_t glow = 255 - (dist * 51).getInteger();
                    CRGB16 white_glow = CRGB16(glow * 256, glow * 256, glow * 256);
                    color = add_clipped(color, white_glow);
                }
                
                leds_16[i] = add_clipped(leds_16[i], color);
            }
        }
    }
    
    // Add central bright spot
    int center = NATIVE_RESOLUTION / 2;
    uint8_t center_pulse = 200 + (sin_lookup((pulse_phase * 2).getInteger() & 0xFF) * 55).getInteger();
    CRGB16 center_color;
    
    if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
        center_color = palette_to_crgb16(palette_arr[frame_config.PALETTE], 0, center_pulse);
    } else {
        center_color = hsv_to_rgb_fast(frame_config.HUE, frame_config.SATURATION / 2, center_pulse);
    }
    
    leds_16[center] = add_clipped(leds_16[center], center_color);
    if (center > 0) leds_16[center-1] = add_clipped(leds_16[center-1], scale_color(center_color, SQ15x16(0.5)));
    if (center < NATIVE_RESOLUTION-1) leds_16[center+1] = add_clipped(leds_16[center+1], scale_color(center_color, SQ15x16(0.5)));
    
    apply_global_brightness();
}