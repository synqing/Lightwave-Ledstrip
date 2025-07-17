#include "lgp_dimensional_portal_effect.h"

// Portal state structure
struct PortalState {
    SQ15x16 center_position;
    SQ15x16 radius;
    SQ15x16 rotation_angle;
    SQ15x16 stability;
    SQ15x16 energy_level;
    bool collapsing;
    uint8_t dimension_id;
};

// Exotic matter particle
struct ExoticParticle {
    SQ15x16 position;
    SQ15x16 velocity;
    SQ15x16 angular_momentum;
    SQ15x16 lifetime;
    uint8_t dimension_origin;
    bool active;
};

static PortalState portal;
static const int MAX_EXOTIC_PARTICLES = 20;
static ExoticParticle exotic_matter[MAX_EXOTIC_PARTICLES];
static SQ15x16 spacetime_phase = 0;
static SQ15x16 gravitational_waves[NATIVE_RESOLUTION];
static bool portal_opening = false;
static uint32_t last_portal_event = 0;

// Calculate spacetime curvature around portal
static SQ15x16 calculate_curvature(SQ15x16 distance, SQ15x16 portal_mass) {
    // Schwarzschild metric approximation
    if (distance < 1) distance = 1;
    SQ15x16 curvature = portal_mass / (distance * distance);
    return min(curvature, SQ15x16(10));  // Cap extreme values
}

// Gravitational lensing effect
static SQ15x16 apply_gravitational_lensing(int position, SQ15x16 light_angle) {
    SQ15x16 distance = abs(SQ15x16(position) - portal.center_position);
    SQ15x16 curvature = calculate_curvature(distance, portal.energy_level);
    
    // Bend light around massive object
    SQ15x16 deflection = curvature * sin_lookup((light_angle + spacetime_phase).getInteger() & 0xFF);
    return light_angle + deflection * SQ15x16(0.3);
}

// Initialize exotic particle
static void spawn_exotic_particle(int idx) {
    exotic_matter[idx].position = portal.center_position;
    exotic_matter[idx].angular_momentum = SQ15x16(random8() - 128) / 32;
    exotic_matter[idx].velocity = SQ15x16(random8() - 128) / 64;
    exotic_matter[idx].lifetime = SQ15x16(random8() + 128) / 32;
    exotic_matter[idx].dimension_origin = random8() & 0x07;
    exotic_matter[idx].active = true;
}

void light_mode_lgp_dimensional_portal() {
    cache_frame_config();
    
    // Initialize portal
    static bool initialized = false;
    if (!initialized) {
        portal.center_position = NATIVE_RESOLUTION / 2;
        portal.radius = 10;
        portal.rotation_angle = 0;
        portal.stability = SQ15x16(0.5);
        portal.energy_level = 100;
        portal.collapsing = false;
        portal.dimension_id = 0;
        initialized = true;
        
        // Initialize gravitational wave buffer
        for (int i = 0; i < NATIVE_RESOLUTION; i++) {
            gravitational_waves[i] = 0;
        }
    }
    
    // Audio analysis for portal dynamics
    uint16_t bass_energy = 0;
    uint16_t mid_energy = 0;
    uint16_t high_energy = 0;
    uint16_t total_energy = 0;
    
    if (MAX_SAMPLING_FREQ > 0) {
        for (int i = 0; i < 6; i++) {
            uint16_t band_energy = sampleAvgMinor[i];
            total_energy += band_energy;
            if (i < 2) bass_energy += band_energy;
            else if (i < 4) mid_energy += band_energy;
            else high_energy += band_energy;
        }
    }
    
    // Portal stability affected by audio
    SQ15x16 target_stability = SQ15x16(mid_energy) / 5000;
    portal.stability += (target_stability - portal.stability) * SQ15x16(0.1);
    
    // Energy level from bass
    portal.energy_level = 50 + (bass_energy >> 3);
    
    // Portal events triggered by beats
    uint32_t current_time = millis();
    bool trigger_event = (bass_energy > 3000) || (current_time - last_portal_event > 5000);
    
    if (trigger_event) {
        last_portal_event = current_time;
        portal_opening = !portal_opening;
        
        // Change dimension
        portal.dimension_id = (portal.dimension_id + 1) & 0x07;
        
        // Spawn exotic matter burst
        for (int i = 0; i < MAX_EXOTIC_PARTICLES; i++) {
            if (!exotic_matter[i].active && random8() < 128) {
                spawn_exotic_particle(i);
            }
        }
    }
    
    // Update portal dynamics
    portal.rotation_angle += SQ15x16(frame_config.SPEED) / 16 + portal.stability;
    spacetime_phase += SQ15x16(frame_config.SPEED) / 32;
    
    // Portal radius oscillation
    if (portal_opening) {
        portal.radius += SQ15x16(0.5);
        if (portal.radius > 20) portal.radius = 20;
    } else {
        portal.radius -= SQ15x16(0.3);
        if (portal.radius < 5) portal.radius = 5;
    }
    
    // Move portal center with high frequencies
    SQ15x16 drift = sin_lookup((spacetime_phase * 2).getInteger() & 0xFF) * SQ15x16(high_energy) / 1000;
    portal.center_position += drift;
    if (portal.center_position < 10) portal.center_position = 10;
    if (portal.center_position > NATIVE_RESOLUTION - 10) portal.center_position = NATIVE_RESOLUTION - 10;
    
    // Update exotic particles
    for (int i = 0; i < MAX_EXOTIC_PARTICLES; i++) {
        if (exotic_matter[i].active) {
            // Spiral motion around portal
            SQ15x16 angle = atan2_approx(exotic_matter[i].position - portal.center_position, SQ15x16(1));
            angle += exotic_matter[i].angular_momentum;
            
            SQ15x16 radius = abs(exotic_matter[i].position - portal.center_position);
            radius += exotic_matter[i].velocity;
            
            // Update position
            exotic_matter[i].position = portal.center_position + 
                cos_lookup(angle.getInteger() & 0xFF) * radius;
            
            // Lifetime decay
            exotic_matter[i].lifetime -= SQ15x16(0.1);
            if (exotic_matter[i].lifetime <= 0 || 
                exotic_matter[i].position < 0 || 
                exotic_matter[i].position >= NATIVE_RESOLUTION) {
                exotic_matter[i].active = false;
            }
        }
    }
    
    // Update gravitational waves
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        SQ15x16 distance = abs(SQ15x16(i) - portal.center_position);
        SQ15x16 wave_amplitude = portal.energy_level / (distance + 10);
        
        // Ripples in spacetime
        SQ15x16 wave_phase = spacetime_phase * 3 - distance * SQ15x16(0.5);
        gravitational_waves[i] = sin_lookup(wave_phase.getInteger() & 0xFF) * wave_amplitude;
    }
    
    // Clear strip with dimensional background
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        // Dimensional bleed-through based on portal proximity
        SQ15x16 distance = abs(SQ15x16(i) - portal.center_position);
        uint8_t bleed = max(0, 20 - distance.getInteger());
        
        // Different dimensions have different background colors
        CRGB16 dim_color;
        switch(portal.dimension_id) {
            case 0: dim_color = CRGB16(bleed * 100, 0, bleed * 200); break;      // Purple
            case 1: dim_color = CRGB16(0, bleed * 200, bleed * 100); break;      // Cyan
            case 2: dim_color = CRGB16(bleed * 200, bleed * 100, 0); break;      // Orange
            case 3: dim_color = CRGB16(bleed * 150, 0, bleed * 150); break;      // Magenta
            case 4: dim_color = CRGB16(0, bleed * 150, bleed * 150); break;      // Teal
            case 5: dim_color = CRGB16(bleed * 100, bleed * 200, 0); break;      // Lime
            case 6: dim_color = CRGB16(bleed * 200, bleed * 200, bleed * 200); break; // White
            case 7: dim_color = CRGB16(bleed * 50, bleed * 50, bleed * 200); break;   // Deep blue
        }
        
        leds_16[i] = dim_color;
    }
    
    // Render portal vortex
    int portal_center = portal.center_position.getInteger();
    int portal_rad = portal.radius.getInteger();
    
    for (int i = -portal_rad * 2; i <= portal_rad * 2; i++) {
        int pos = portal_center + i;
        if (pos >= 0 && pos < NATIVE_RESOLUTION) {
            SQ15x16 distance = abs(SQ15x16(i));
            
            // Vortex swirl
            SQ15x16 swirl_angle = portal.rotation_angle + distance * SQ15x16(0.3);
            swirl_angle = apply_gravitational_lensing(pos, swirl_angle);
            
            // Event horizon
            if (distance < portal.radius) {
                // Inside the portal - intense warping
                uint8_t vortex_bright = 255 - (distance * 255 / portal.radius).getInteger();
                
                // Swirling colors based on angle and instability
                uint8_t hue = (swirl_angle * 40 + portal.dimension_id * 32).getInteger() & 0xFF;
                CRGB16 vortex_color;
                
                if (portal.stability < SQ15x16(0.3)) {
                    // Unstable portal - chaotic colors
                    vortex_color = hsv_to_rgb_fast(
                        hue + random8(30),
                        255 - portal.stability.getInteger() * 100,
                        vortex_bright
                    );
                    
                    // Lightning inside unstable portal
                    if (random8() < 20) {
                        vortex_color = CRGB16(65535, 65535, 65535);
                    }
                } else {
                    // Stable portal - smooth gradient
                    vortex_color = hsv_to_rgb_fast(hue, 200, vortex_bright);
                }
                
                leds_16[pos] = vortex_color;
            } else {
                // Accretion disk
                SQ15x16 disk_intensity = portal.radius / distance;
                if (disk_intensity > 0) {
                    uint8_t disk_bright = (disk_intensity * 150).getInteger();
                    
                    // Hot plasma in accretion disk
                    CRGB16 disk_color = CRGB16(
                        disk_bright * 256,
                        disk_bright * 200,
                        disk_bright * 100
                    );
                    
                    leds_16[pos] = add_clipped(leds_16[pos], disk_color);
                }
            }
            
            // Gravitational wave distortion
            uint8_t wave_offset = (gravitational_waves[pos] * 20).getInteger();
            if (wave_offset > 0) {
                CRGB16 wave_color = CRGB16(0, wave_offset * 100, wave_offset * 200);
                leds_16[pos] = add_clipped(leds_16[pos], wave_color);
            }
        }
    }
    
    // Render exotic matter particles
    for (int i = 0; i < MAX_EXOTIC_PARTICLES; i++) {
        if (exotic_matter[i].active) {
            int pos = exotic_matter[i].position.getInteger();
            if (pos >= 0 && pos < NATIVE_RESOLUTION) {
                uint8_t particle_bright = (exotic_matter[i].lifetime * 30).getInteger();
                
                // Exotic matter has negative energy - unusual colors
                CRGB16 exotic_color;
                switch(exotic_matter[i].dimension_origin & 0x03) {
                    case 0: exotic_color = CRGB16(0, particle_bright * 256, particle_bright * 300); break;
                    case 1: exotic_color = CRGB16(particle_bright * 300, 0, particle_bright * 256); break;
                    case 2: exotic_color = CRGB16(particle_bright * 256, particle_bright * 300, 0); break;
                    case 3: exotic_color = CRGB16(particle_bright * 300, particle_bright * 300, particle_bright * 300); break;
                }
                
                leds_16[pos] = add_clipped(leds_16[pos], exotic_color);
                
                // Particle trail
                int trail_dir = (exotic_matter[i].velocity > 0) ? -1 : 1;
                for (int t = 1; t <= 3; t++) {
                    int trail_pos = pos + (t * trail_dir);
                    if (trail_pos >= 0 && trail_pos < NATIVE_RESOLUTION) {
                        CRGB16 trail_color = exotic_color;
                        trail_color.r = trail_color.r / (t + 1);
                        trail_color.g = trail_color.g / (t + 1);
                        trail_color.b = trail_color.b / (t + 1);
                        leds_16[trail_pos] = add_clipped(leds_16[trail_pos], trail_color);
                    }
                }
            }
        }
    }
    
    // Reality tears at high instability
    if (portal.stability < SQ15x16(0.2)) {
        for (int i = 0; i < 5; i++) {
            int tear_pos = random16(NATIVE_RESOLUTION);
            CRGB16 tear_color = CRGB16(65535, 0, 65535);  // Violent purple
            leds_16[tear_pos] = tear_color;
        }
    }
    
    apply_global_brightness();
}