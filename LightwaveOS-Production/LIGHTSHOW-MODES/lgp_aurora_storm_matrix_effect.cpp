#include "lgp_aurora_storm_matrix_effect.h"

// Solar wind particle types
enum SolarParticle {
    PROTON,
    ELECTRON,
    ALPHA_PARTICLE,
    HEAVY_ION
};

// Magnetic field line structure
struct MagneticFieldLine {
    SQ15x16 anchor_point;
    SQ15x16 field_strength;
    SQ15x16 curvature;
    SQ15x16 oscillation_phase;
    bool reconnecting;
    uint8_t polarity;  // 0 = north, 1 = south
};

// Solar wind stream
struct SolarWindStream {
    SQ15x16 position;
    SQ15x16 velocity;
    SQ15x16 density;
    SQ15x16 temperature;
    SolarParticle particle_type;
    bool active;
};

// Auroral curtain segment
struct AuroraCurtain {
    SQ15x16 position;
    SQ15x16 height;
    SQ15x16 fold_phase;
    SQ15x16 intensity;
    uint8_t emission_spectrum;  // Oxygen vs Nitrogen dominated
};

// Global magnetospheric state
static const int MAX_FIELD_LINES = 15;
static MagneticFieldLine field_lines[MAX_FIELD_LINES];
static const int MAX_SOLAR_PARTICLES = 30;
static SolarWindStream solar_wind[MAX_SOLAR_PARTICLES];
static const int MAX_CURTAINS = 10;
static AuroraCurtain aurora_curtains[MAX_CURTAINS];

static SQ15x16 geomagnetic_storm_intensity = 0;
static SQ15x16 solar_wind_pressure = SQ15x16(1);
static SQ15x16 magnetosphere_compression = 0;
static bool coronal_mass_ejection = false;
static uint32_t last_cme_time = 0;

// Calculate magnetic field vector at position
static SQ15x16 calculate_field_strength(SQ15x16 position) {
    SQ15x16 total_field = 0;
    
    for (int i = 0; i < MAX_FIELD_LINES; i++) {
        SQ15x16 distance = abs(position - field_lines[i].anchor_point);
        if (distance < 50) {
            // Dipole field approximation
            SQ15x16 field_contribution = field_lines[i].field_strength / (distance + 1);
            total_field += field_contribution;
        }
    }
    
    return total_field;
}

// Initialize magnetic field configuration
static void init_magnetosphere() {
    // Create dipole-like field lines
    for (int i = 0; i < MAX_FIELD_LINES; i++) {
        field_lines[i].anchor_point = (i * NATIVE_RESOLUTION) / MAX_FIELD_LINES;
        field_lines[i].field_strength = SQ15x16(50) + SQ15x16(random8()) / 10;
        field_lines[i].curvature = SQ15x16(random8() - 128) / 128;
        field_lines[i].oscillation_phase = random8();
        field_lines[i].reconnecting = false;
        field_lines[i].polarity = (i < MAX_FIELD_LINES / 2) ? 0 : 1;
    }
    
    // Initialize aurora curtains
    for (int i = 0; i < MAX_CURTAINS; i++) {
        aurora_curtains[i].position = (i * NATIVE_RESOLUTION) / MAX_CURTAINS;
        aurora_curtains[i].height = SQ15x16(20) + SQ15x16(random8()) / 10;
        aurora_curtains[i].fold_phase = random8();
        aurora_curtains[i].intensity = 0;
        aurora_curtains[i].emission_spectrum = random8() & 0x03;
    }
}

void light_mode_lgp_aurora_storm_matrix() {
    cache_frame_config();
    
    // Initialize on first run
    static bool initialized = false;
    if (!initialized) {
        init_magnetosphere();
        initialized = true;
    }
    
    // Audio analysis for space weather
    uint16_t freq_bands[6] = {0};
    uint16_t total_energy = 0;
    bool major_impact = false;
    
    if (MAX_SAMPLING_FREQ > 0) {
        for (int i = 0; i < 6; i++) {
            freq_bands[i] = sampleAvgMinor[i];
            total_energy += freq_bands[i];
        }
        
        // Detect major impacts (CME events)
        static uint16_t prev_energy = 0;
        if (total_energy > prev_energy * 1.5 && total_energy > 5000) {
            major_impact = true;
        }
        prev_energy = total_energy;
    }
    
    // Update space weather parameters
    solar_wind_pressure = SQ15x16(1) + SQ15x16(freq_bands[0]) / 2000;
    geomagnetic_storm_intensity = SQ15x16(freq_bands[1] + freq_bands[2]) / 3000;
    
    // Magnetosphere compression from solar wind
    magnetosphere_compression = solar_wind_pressure * SQ15x16(0.3);
    
    // Trigger coronal mass ejection
    uint32_t current_time = millis();
    if (major_impact && current_time - last_cme_time > 2000) {
        coronal_mass_ejection = true;
        last_cme_time = current_time;
        
        // Launch high-speed solar wind
        for (int i = 0; i < MAX_SOLAR_PARTICLES; i++) {
            if (!solar_wind[i].active && random8() < 200) {
                solar_wind[i].position = random16(NATIVE_RESOLUTION);
                solar_wind[i].velocity = SQ15x16(2) + SQ15x16(random8()) / 64;
                solar_wind[i].density = SQ15x16(freq_bands[0]) / 100;
                solar_wind[i].temperature = SQ15x16(1000) + SQ15x16(freq_bands[3]);
                solar_wind[i].particle_type = (SolarParticle)(random8() & 0x03);
                solar_wind[i].active = true;
            }
        }
    } else {
        coronal_mass_ejection = false;
    }
    
    // Update magnetic field lines
    for (int i = 0; i < MAX_FIELD_LINES; i++) {
        // Field line oscillations
        field_lines[i].oscillation_phase += SQ15x16(frame_config.SPEED) / 32;
        
        // Compression from solar wind
        SQ15x16 compression_offset = magnetosphere_compression * 
            sin_lookup((field_lines[i].oscillation_phase + i * 20).getInteger() & 0xFF);
        
        // Magnetic reconnection during storms
        if (geomagnetic_storm_intensity > SQ15x16(0.7) && random8() < 50) {
            field_lines[i].reconnecting = true;
        } else if (field_lines[i].reconnecting && random8() < 20) {
            field_lines[i].reconnecting = false;
        }
        
        // Field strength modulation
        field_lines[i].field_strength = SQ15x16(50) + 
            geomagnetic_storm_intensity * 100 +
            compression_offset * 10;
    }
    
    // Update solar wind particles
    for (int i = 0; i < MAX_SOLAR_PARTICLES; i++) {
        if (solar_wind[i].active) {
            // Particle motion along field lines
            SQ15x16 field_strength = calculate_field_strength(solar_wind[i].position);
            
            // Lorentz force deflection
            SQ15x16 deflection = field_strength * SQ15x16(0.01);
            solar_wind[i].velocity += deflection;
            
            solar_wind[i].position += solar_wind[i].velocity;
            
            // Particle precipitation into atmosphere
            if (solar_wind[i].position < 0 || solar_wind[i].position >= NATIVE_RESOLUTION) {
                solar_wind[i].active = false;
                
                // Create aurora at precipitation point
                int curtain_idx = random8(MAX_CURTAINS);
                aurora_curtains[curtain_idx].intensity = 
                    min(SQ15x16(1), aurora_curtains[curtain_idx].intensity + SQ15x16(0.3));
            }
            
            // Energy loss
            solar_wind[i].temperature *= SQ15x16(0.98);
            if (solar_wind[i].temperature < 100) {
                solar_wind[i].active = false;
            }
        }
    }
    
    // Update aurora curtains
    for (int i = 0; i < MAX_CURTAINS; i++) {
        // Curtain folding and movement
        aurora_curtains[i].fold_phase += SQ15x16(frame_config.SPEED) / 64 + 
                                        geomagnetic_storm_intensity / 10;
        
        // Height variation with storm intensity
        aurora_curtains[i].height = SQ15x16(20) + 
            geomagnetic_storm_intensity * 30 +
            sin_lookup((aurora_curtains[i].fold_phase * 2).getInteger() & 0xFF) * 10;
        
        // Intensity decay
        aurora_curtains[i].intensity *= SQ15x16(0.95);
        
        // Storm-driven intensification
        if (coronal_mass_ejection) {
            aurora_curtains[i].intensity = min(SQ15x16(1), 
                aurora_curtains[i].intensity + SQ15x16(0.1));
        }
    }
    
    // Clear strip with night sky
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        // Faint starfield
        uint8_t stars = (random8() < 5) ? random8(30) : 0;
        leds_16[i] = CRGB16(stars * 256, stars * 256, stars * 300);
    }
    
    // Render magnetic field lines
    for (int i = 0; i < MAX_FIELD_LINES; i++) {
        int anchor = field_lines[i].anchor_point.getInteger();
        
        // Field line visualization
        for (int offset = -30; offset <= 30; offset++) {
            int pos = anchor + offset;
            if (pos >= 0 && pos < NATIVE_RESOLUTION) {
                // Field line shape
                SQ15x16 field_curve = field_lines[i].curvature * offset +
                    sin_lookup((field_lines[i].oscillation_phase + offset * 5).getInteger() & 0xFF) * 
                    magnetosphere_compression;
                
                uint8_t field_brightness = max(0, 50 - abs(field_curve.getInteger()));
                
                if (field_lines[i].reconnecting) {
                    // Reconnection events - bright flashes
                    field_brightness = min(255, field_brightness * 3);
                    CRGB16 reconnect_color = CRGB16(
                        field_brightness * 256,
                        field_brightness * 200,
                        field_brightness * 300
                    );
                    leds_16[pos] = add_clipped(leds_16[pos], reconnect_color);
                } else {
                    // Normal field lines - blue/purple
                    CRGB16 field_color = CRGB16(
                        field_brightness * 50,
                        field_brightness * 100,
                        field_brightness * 200
                    );
                    leds_16[pos] = add_clipped(leds_16[pos], field_color);
                }
            }
        }
    }
    
    // Render aurora curtains
    for (int i = 0; i < MAX_CURTAINS; i++) {
        if (aurora_curtains[i].intensity > SQ15x16(0.01)) {
            int curtain_center = aurora_curtains[i].position.getInteger();
            int curtain_height = aurora_curtains[i].height.getInteger();
            
            for (int h = -curtain_height; h <= curtain_height; h++) {
                int pos = curtain_center + h;
                if (pos >= 0 && pos < NATIVE_RESOLUTION) {
                    // Curtain folding
                    SQ15x16 fold = sin_lookup((aurora_curtains[i].fold_phase + h * 10).getInteger() & 0xFF);
                    int fold_offset = (fold * 5).getInteger();
                    
                    // Aurora intensity gradient
                    uint8_t height_factor = 255 - (abs(h) * 255) / (curtain_height + 1);
                    uint8_t aurora_bright = (aurora_curtains[i].intensity * height_factor).getInteger();
                    
                    // Emission spectrum colors
                    CRGB16 aurora_color;
                    switch(aurora_curtains[i].emission_spectrum) {
                        case 0:  // Oxygen green (557.7 nm)
                            aurora_color = CRGB16(0, aurora_bright * 256, aurora_bright * 50);
                            break;
                        case 1:  // Oxygen red (630.0 nm)
                            aurora_color = CRGB16(aurora_bright * 256, 0, aurora_bright * 50);
                            break;
                        case 2:  // Nitrogen blue/purple
                            aurora_color = CRGB16(aurora_bright * 150, 0, aurora_bright * 256);
                            break;
                        case 3:  // Mixed spectrum
                            aurora_color = CRGB16(
                                aurora_bright * 100,
                                aurora_bright * 200,
                                aurora_bright * 150
                            );
                            break;
                    }
                    
                    // Ray structure
                    if (random8() < aurora_bright) {
                        aurora_color.r = min(65535, aurora_color.r * 2);
                        aurora_color.g = min(65535, aurora_color.g * 2);
                        aurora_color.b = min(65535, aurora_color.b * 2);
                    }
                    
                    // Apply with fold offset
                    int folded_pos = pos + fold_offset;
                    if (folded_pos >= 0 && folded_pos < NATIVE_RESOLUTION) {
                        leds_16[folded_pos] = add_clipped(leds_16[folded_pos], aurora_color);
                    }
                }
            }
        }
    }
    
    // Render solar wind particles
    for (int i = 0; i < MAX_SOLAR_PARTICLES; i++) {
        if (solar_wind[i].active) {
            int pos = solar_wind[i].position.getInteger();
            if (pos >= 0 && pos < NATIVE_RESOLUTION) {
                uint8_t particle_bright = (solar_wind[i].temperature / 10).getInteger();
                
                // Particle color based on type
                CRGB16 particle_color;
                switch(solar_wind[i].particle_type) {
                    case PROTON:
                        particle_color = CRGB16(particle_bright * 256, particle_bright * 100, 0);
                        break;
                    case ELECTRON:
                        particle_color = CRGB16(0, particle_bright * 100, particle_bright * 256);
                        break;
                    case ALPHA_PARTICLE:
                        particle_color = CRGB16(particle_bright * 256, 0, particle_bright * 200);
                        break;
                    case HEAVY_ION:
                        particle_color = CRGB16(particle_bright * 200, particle_bright * 256, 0);
                        break;
                }
                
                // Particle trail
                leds_16[pos] = add_clipped(leds_16[pos], particle_color);
                
                // Bremsstrahlung radiation
                if (solar_wind[i].density > SQ15x16(10)) {
                    for (int r = -2; r <= 2; r++) {
                        int rad_pos = pos + r;
                        if (rad_pos >= 0 && rad_pos < NATIVE_RESOLUTION) {
                            uint8_t rad_bright = particle_bright / (abs(r) + 2);
                            CRGB16 radiation = CRGB16(rad_bright * 100, rad_bright * 100, rad_bright * 200);
                            leds_16[rad_pos] = add_clipped(leds_16[rad_pos], radiation);
                        }
                    }
                }
            }
        }
    }
    
    // Storm surge effects
    if (coronal_mass_ejection) {
        // Intense storm front
        for (int i = 0; i < NATIVE_RESOLUTION; i++) {
            if (random8() < 30) {
                CRGB16 storm_flash = CRGB16(
                    random16(20000, 50000),
                    random16(10000, 30000),
                    random16(30000, 65535)
                );
                leds_16[i] = add_clipped(leds_16[i], storm_flash);
            }
        }
    }
    
    // Geomagnetic pulsations
    if (geomagnetic_storm_intensity > SQ15x16(0.5)) {
        SQ15x16 pulse_phase = (millis() / 100) % 256;
        uint8_t pulse_bright = (sin_lookup(pulse_phase.getInteger()) * 30 + 30).getInteger();
        
        for (int i = 0; i < NATIVE_RESOLUTION; i += 5) {
            CRGB16 pulse_color = CRGB16(0, pulse_bright * 100, pulse_bright * 150);
            leds_16[i] = add_clipped(leds_16[i], pulse_color);
        }
    }
    
    apply_global_brightness();
}