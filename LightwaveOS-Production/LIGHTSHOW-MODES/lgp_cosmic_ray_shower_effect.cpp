#include "lgp_cosmic_ray_shower_effect.h"

// Particle types in cosmic ray showers
enum ParticleType {
    PROTON,
    PION,
    MUON,
    ELECTRON,
    GAMMA,
    NEUTRINO
};

// Shower particle structure
struct ShowerParticle {
    SQ15x16 position;
    SQ15x16 velocity;
    SQ15x16 energy;
    SQ15x16 lifetime;
    ParticleType type;
    uint8_t generation;  // Interaction generation
    bool active;
};

static const int MAX_PARTICLES = 50;
static ShowerParticle particles[MAX_PARTICLES];
static SQ15x16 shower_time = 0;
static int primary_impact_pos = NATIVE_RESOLUTION / 2;
static bool shower_active = false;
static uint32_t last_shower_time = 0;

// Cherenkov radiation angle based on particle energy
static SQ15x16 calculate_cherenkov_angle(SQ15x16 energy) {
    // cos(theta) = 1/(n*beta) where n is refractive index
    // Higher energy = closer to speed of light = smaller angle
    SQ15x16 beta = min(SQ15x16(1), energy / SQ15x16(1000));
    return SQ15x16(1) - beta * SQ15x16(0.3);  // Simplified model
}

// Initialize a particle with specific properties
static void init_particle(int idx, SQ15x16 pos, SQ15x16 vel, SQ15x16 energy, ParticleType type, uint8_t gen) {
    particles[idx].position = pos;
    particles[idx].velocity = vel;
    particles[idx].energy = energy;
    particles[idx].type = type;
    particles[idx].generation = gen;
    particles[idx].active = true;
    
    // Lifetime based on particle type
    switch(type) {
        case MUON:
            particles[idx].lifetime = SQ15x16(50);  // Long-lived
            break;
        case PION:
            particles[idx].lifetime = SQ15x16(5);   // Short-lived
            break;
        case ELECTRON:
            particles[idx].lifetime = SQ15x16(20);
            break;
        default:
            particles[idx].lifetime = SQ15x16(10);
    }
}

// Create secondary particles from interaction
static void create_secondaries(int parent_idx) {
    ShowerParticle& parent = particles[parent_idx];
    
    // Find free particle slots
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].active && parent.generation < 5) {
            // Interaction kinematics
            SQ15x16 energy_fraction = SQ15x16(random8()) / 255;
            SQ15x16 angle_spread = SQ15x16(random8() - 128) / 128 * SQ15x16(0.3);
            
            // Create different particles based on parent type
            ParticleType new_type = ELECTRON;
            if (parent.type == PROTON || parent.type == PION) {
                int rnd = random8();
                if (rnd < 100) new_type = PION;
                else if (rnd < 180) new_type = MUON;
                else new_type = ELECTRON;
            }
            
            init_particle(i,
                         parent.position,
                         parent.velocity + angle_spread,
                         parent.energy * energy_fraction,
                         new_type,
                         parent.generation + 1);
            
            // Reduce parent energy
            parent.energy *= (SQ15x16(1) - energy_fraction * SQ15x16(0.5));
            
            // Only create 1-3 secondaries per interaction
            if (random8() < 100) break;
        }
    }
}

void light_mode_lgp_cosmic_ray_shower() {
    cache_frame_config();
    
    // Audio reactivity for cosmic ray frequency
    uint16_t bass_energy = 0;
    uint16_t mid_energy = 0;
    uint16_t high_energy = 0;
    
    if (MAX_SAMPLING_FREQ > 0) {
        // Sample frequency bands
        bass_energy = (uint16_t)sampleAvgMinor[0] + sampleAvgMinor[1];
        mid_energy = (uint16_t)sampleAvgMinor[2] + sampleAvgMinor[3];
        high_energy = (uint16_t)sampleAvgMinor[4] + sampleAvgMinor[5];
    }
    
    // Trigger new shower based on bass hits or timing
    uint32_t current_time = millis();
    bool trigger_shower = (bass_energy > 3000) || 
                         (current_time - last_shower_time > 3000 - frame_config.SPEED * 10);
    
    if (trigger_shower && !shower_active) {
        // Initialize primary cosmic ray
        shower_active = true;
        shower_time = 0;
        last_shower_time = current_time;
        primary_impact_pos = random16(NATIVE_RESOLUTION);
        
        // Clear all particles
        for (int i = 0; i < MAX_PARTICLES; i++) {
            particles[i].active = false;
        }
        
        // Create primary particle (ultra-high energy proton)
        init_particle(0,
                     SQ15x16(primary_impact_pos),
                     SQ15x16(0),  // Vertical impact
                     SQ15x16(1000 + mid_energy),  // Ultra-high energy
                     PROTON,
                     0);
    }
    
    // Update shower evolution
    if (shower_active) {
        shower_time += SQ15x16(frame_config.SPEED) / 64;
        
        // Update all particles
        bool any_active = false;
        for (int i = 0; i < MAX_PARTICLES; i++) {
            if (particles[i].active) {
                any_active = true;
                
                // Update position
                particles[i].position += particles[i].velocity * SQ15x16(frame_config.SPEED) / 32;
                
                // Energy loss and lifetime
                particles[i].energy *= SQ15x16(0.98);
                particles[i].lifetime -= SQ15x16(0.1);
                
                // Particle interactions
                if (particles[i].energy > 100 && random8() < 20) {
                    create_secondaries(i);
                }
                
                // Deactivate dead particles
                if (particles[i].lifetime <= 0 || particles[i].energy < 10) {
                    particles[i].active = false;
                }
            }
        }
        
        if (!any_active) {
            shower_active = false;
        }
    }
    
    // Clear strip with atmospheric glow
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        // Faint atmospheric nitrogen fluorescence
        uint8_t atmos_glow = 5 + (high_energy >> 8);
        leds_16[i] = CRGB16(0, 0, atmos_glow * 256);
    }
    
    // Render particles and their effects
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].active) {
            int pos = particles[i].position.getInteger();
            
            // Cherenkov radiation cone
            if (particles[i].type == MUON || particles[i].type == ELECTRON) {
                SQ15x16 cherenkov_spread = calculate_cherenkov_angle(particles[i].energy);
                int cone_width = (cherenkov_spread * 10).getInteger();
                
                for (int j = -cone_width; j <= cone_width; j++) {
                    int led_pos = pos + j;
                    if (led_pos >= 0 && led_pos < NATIVE_RESOLUTION) {
                        // Characteristic blue Cherenkov light
                        SQ15x16 intensity = particles[i].energy / 1000 * SQ15x16(255);
                        intensity *= SQ15x16(1) - abs(SQ15x16(j)) / cone_width;
                        
                        uint8_t bright = intensity.getInteger();
                        CRGB16 cherenkov_color = CRGB16(bright * 100, bright * 200, bright * 256);
                        
                        leds_16[led_pos] = add_clipped(leds_16[led_pos], cherenkov_color);
                    }
                }
            }
            
            // Direct particle track
            if (pos >= 0 && pos < NATIVE_RESOLUTION) {
                CRGB16 particle_color;
                uint8_t brightness = (particles[i].energy / 4).getInteger();
                
                // Color based on particle type
                switch(particles[i].type) {
                    case PROTON:
                        particle_color = CRGB16(brightness * 256, brightness * 128, 0);  // Orange
                        break;
                    case PION:
                        particle_color = CRGB16(brightness * 256, 0, brightness * 128);  // Magenta
                        break;
                    case MUON:
                        particle_color = CRGB16(0, brightness * 256, brightness * 256);  // Cyan
                        break;
                    case ELECTRON:
                        particle_color = CRGB16(brightness * 256, brightness * 256, brightness * 256);  // White
                        break;
                    case GAMMA:
                        particle_color = CRGB16(brightness * 128, brightness * 256, brightness * 128);  // Green
                        break;
                    default:
                        particle_color = CRGB16(brightness * 256, brightness * 256, 0);  // Yellow
                }
                
                leds_16[pos] = add_clipped(leds_16[pos], particle_color);
                
                // Ionization trail
                int trail_length = particles[i].generation + 2;
                for (int t = 1; t <= trail_length; t++) {
                    int trail_pos = pos - (particles[i].velocity * t).getInteger();
                    if (trail_pos >= 0 && trail_pos < NATIVE_RESOLUTION) {
                        uint8_t trail_bright = brightness / (t + 1);
                        CRGB16 trail_color = particle_color;
                        trail_color.r = (trail_color.r * trail_bright) >> 8;
                        trail_color.g = (trail_color.g * trail_bright) >> 8;
                        trail_color.b = (trail_color.b * trail_bright) >> 8;
                        
                        leds_16[trail_pos] = add_clipped(leds_16[trail_pos], trail_color);
                    }
                }
            }
        }
    }
    
    // Add scintillation flashes at high energy deposits
    if (shower_active && random8() < 50) {
        int flash_pos = primary_impact_pos + random8(40) - 20;
        if (flash_pos >= 0 && flash_pos < NATIVE_RESOLUTION) {
            CRGB16 flash = CRGB16(50000, 50000, 65535);  // Bright white-blue flash
            leds_16[flash_pos] = flash;
        }
    }
    
    // Apply color mode if not using physical accuracy
    if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
        for (int i = 0; i < NATIVE_RESOLUTION; i++) {
            uint32_t total_energy = leds_16[i].r + leds_16[i].g + leds_16[i].b;
            if (total_energy > 0) {
                uint8_t brightness = min(255, total_energy >> 8);
                uint8_t pal_index = (shower_time * 10 + i).getInteger() & 0xFF;
                leds_16[i] = palette_to_crgb16(palette_arr[frame_config.PALETTE],
                                              pal_index, brightness);
            }
        }
    }
    
    apply_global_brightness();
}