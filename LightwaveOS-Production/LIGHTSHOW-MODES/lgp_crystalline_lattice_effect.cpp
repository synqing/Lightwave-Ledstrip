#include "lgp_crystalline_lattice_effect.h"

// Crystal structure types
enum CrystalPhase {
    AMORPHOUS,      // Disordered
    CUBIC,          // Simple cubic lattice
    HEXAGONAL,      // Hexagonal close-packed
    DIAMOND,        // Diamond cubic
    QUASICRYSTAL    // Penrose-like aperiodic
};

// Lattice site structure
struct LatticeSite {
    SQ15x16 order_parameter;    // Crystallinity (0 = liquid, 1 = perfect crystal)
    SQ15x16 energy;            // Thermal/vibrational energy
    SQ15x16 strain;            // Mechanical strain
    uint8_t coordination;       // Number of bonds
    CrystalPhase phase;
    bool nucleation_site;
    bool defect;
};

// Phonon (lattice vibration) structure
struct Phonon {
    SQ15x16 position;
    SQ15x16 wavelength;
    SQ15x16 amplitude;
    SQ15x16 velocity;
    bool active;
};

static const int LATTICE_SIZE = NATIVE_RESOLUTION;
static LatticeSite lattice[LATTICE_SIZE];
static const int MAX_PHONONS = 10;
static Phonon phonons[MAX_PHONONS];
static SQ15x16 temperature = SQ15x16(0.5);
static SQ15x16 pressure = SQ15x16(1.0);
static CrystalPhase global_phase = AMORPHOUS;
static uint32_t last_phase_transition = 0;

// Calculate local order based on neighbors
static SQ15x16 calculate_local_order(int site) {
    SQ15x16 order_sum = 0;
    int neighbor_count = 0;
    
    // Check neighbors within interaction range
    for (int i = -3; i <= 3; i++) {
        if (i == 0) continue;
        int neighbor = site + i;
        if (neighbor >= 0 && neighbor < LATTICE_SIZE) {
            order_sum += lattice[neighbor].order_parameter;
            neighbor_count++;
        }
    }
    
    return neighbor_count > 0 ? order_sum / neighbor_count : 0;
}

// Spawn phonon at site
static void create_phonon(int site, SQ15x16 energy) {
    for (int i = 0; i < MAX_PHONONS; i++) {
        if (!phonons[i].active) {
            phonons[i].position = SQ15x16(site);
            phonons[i].wavelength = SQ15x16(2) + energy / 100;
            phonons[i].amplitude = energy / 500;
            phonons[i].velocity = SQ15x16(1) + energy / 1000;
            phonons[i].active = true;
            break;
        }
    }
}

void light_mode_lgp_crystalline_lattice() {
    cache_frame_config();
    
    // Initialize lattice
    static bool initialized = false;
    if (!initialized) {
        for (int i = 0; i < LATTICE_SIZE; i++) {
            lattice[i].order_parameter = 0;
            lattice[i].energy = SQ15x16(0.1);
            lattice[i].strain = 0;
            lattice[i].coordination = 0;
            lattice[i].phase = AMORPHOUS;
            lattice[i].nucleation_site = false;
            lattice[i].defect = false;
        }
        initialized = true;
    }
    
    // Audio analysis for crystal dynamics
    uint16_t bass_energy = 0;
    uint16_t mid_energy = 0;
    uint16_t high_energy = 0;
    bool beat_detected = false;
    
    if (MAX_SAMPLING_FREQ > 0) {
        // Frequency bands drive different processes
        bass_energy = sampleAvgMinor[0] + sampleAvgMinor[1];   // Temperature/pressure
        mid_energy = sampleAvgMinor[2] + sampleAvgMinor[3];    // Growth rate
        high_energy = sampleAvgMinor[4] + sampleAvgMinor[5];   // Defects/phonons
        
        // Beat detection for phase transitions
        static uint16_t prev_bass = 0;
        if (bass_energy > prev_bass * 1.3 && bass_energy > 2500) {
            beat_detected = true;
        }
        prev_bass = bass_energy;
    }
    
    // Update thermodynamic variables
    temperature = SQ15x16(0.3) + SQ15x16(bass_energy) / 10000;
    pressure = SQ15x16(1.0) + SQ15x16(mid_energy) / 8000;
    
    // Phase transitions on beats
    uint32_t current_time = millis();
    if (beat_detected && current_time - last_phase_transition > 1000) {
        last_phase_transition = current_time;
        
        // Determine new phase based on conditions
        if (temperature < SQ15x16(0.4)) {
            global_phase = DIAMOND;  // Low temp = ordered
        } else if (temperature < SQ15x16(0.6)) {
            global_phase = (pressure > SQ15x16(1.5)) ? HEXAGONAL : CUBIC;
        } else if (temperature < SQ15x16(0.8)) {
            global_phase = QUASICRYSTAL;
        } else {
            global_phase = AMORPHOUS;  // High temp = disordered
        }
        
        // Create nucleation sites
        for (int i = 0; i < 5; i++) {
            int site = random16(LATTICE_SIZE);
            lattice[site].nucleation_site = true;
            lattice[site].phase = global_phase;
        }
    }
    
    // Update phonons (lattice vibrations)
    for (int i = 0; i < MAX_PHONONS; i++) {
        if (phonons[i].active) {
            phonons[i].position += phonons[i].velocity;
            phonons[i].amplitude *= SQ15x16(0.95);  // Damping
            
            if (phonons[i].position < 0 || phonons[i].position >= LATTICE_SIZE ||
                phonons[i].amplitude < SQ15x16(0.01)) {
                phonons[i].active = false;
            }
        }
    }
    
    // Create new phonons from high frequency audio
    if (high_energy > 1000 && random8() < 50) {
        int phonon_site = random16(LATTICE_SIZE);
        create_phonon(phonon_site, SQ15x16(high_energy));
    }
    
    // Update lattice sites
    for (int i = 0; i < LATTICE_SIZE; i++) {
        // Calculate local environment
        SQ15x16 local_order = calculate_local_order(i);
        
        // Energy from phonons
        SQ15x16 phonon_energy = 0;
        for (int p = 0; p < MAX_PHONONS; p++) {
            if (phonons[p].active) {
                SQ15x16 distance = abs(phonons[p].position - SQ15x16(i));
                if (distance < phonons[p].wavelength) {
                    phonon_energy += phonons[p].amplitude * 
                        cos_lookup((distance * 256 / phonons[p].wavelength).getInteger() & 0xFF);
                }
            }
        }
        
        // Update site energy
        lattice[i].energy = temperature + abs(phonon_energy);
        
        // Crystal growth dynamics
        if (lattice[i].nucleation_site || local_order > SQ15x16(0.5)) {
            // Growth rate depends on temperature and pressure
            SQ15x16 growth_rate = (SQ15x16(1) - temperature) * pressure * SQ15x16(0.1);
            growth_rate *= SQ15x16(frame_config.SPEED) / 128;
            
            lattice[i].order_parameter += growth_rate;
            if (lattice[i].order_parameter > 1) lattice[i].order_parameter = 1;
            
            // Propagate crystallinity
            if (lattice[i].order_parameter > SQ15x16(0.7)) {
                lattice[i].phase = global_phase;
            }
        } else if (lattice[i].energy > SQ15x16(1.5)) {
            // Melting at high energy
            lattice[i].order_parameter *= SQ15x16(0.95);
            if (lattice[i].order_parameter < SQ15x16(0.1)) {
                lattice[i].phase = AMORPHOUS;
            }
        }
        
        // Defect formation
        if (random8() < high_energy >> 6) {
            lattice[i].defect = true;
        } else if (lattice[i].defect && random8() < 10) {
            lattice[i].defect = false;  // Defect annealing
        }
        
        // Calculate strain from neighboring order mismatch
        SQ15x16 strain = 0;
        if (i > 0 && i < LATTICE_SIZE - 1) {
            strain = abs(lattice[i-1].order_parameter - lattice[i+1].order_parameter);
        }
        lattice[i].strain = strain;
    }
    
    // Render crystal lattice
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        LatticeSite& site = lattice[i];
        
        CRGB16 color;
        uint8_t brightness = (site.order_parameter * 200 + 55).getInteger();
        
        // Color based on crystal phase
        switch(site.phase) {
            case AMORPHOUS:
                // Glassy - blue-white
                color = CRGB16(brightness * 100, brightness * 150, brightness * 256);
                break;
                
            case CUBIC:
                // Simple cubic - green
                color = CRGB16(0, brightness * 256, brightness * 128);
                break;
                
            case HEXAGONAL:
                // Hexagonal - cyan
                color = CRGB16(0, brightness * 200, brightness * 256);
                break;
                
            case DIAMOND:
                // Diamond - pure white with spectral dispersion
                if (frame_config.COLOR_MODE == COLOR_MODE_SOLID) {
                    color = CRGB16(brightness * 256, brightness * 256, brightness * 256);
                } else {
                    // Prismatic effect
                    uint8_t dispersion = (i * 10 + millis() / 10) & 0xFF;
                    color = hsv_to_rgb_fast(dispersion, 100, brightness);
                }
                break;
                
            case QUASICRYSTAL:
                // Quasicrystal - golden ratio colors
                uint8_t phi_hue = ((i * 161 + millis() / 20) >> 2) & 0xFF;  // 161/100 ≈ golden ratio
                color = hsv_to_rgb_fast(phi_hue, 200, brightness);
                break;
        }
        
        // Defect visualization
        if (site.defect) {
            // Defects glow red
            color.r = min(65535, color.r + 20000);
        }
        
        // Strain birefringence (stress-induced color changes)
        if (site.strain > SQ15x16(0.2)) {
            uint8_t strain_shift = (site.strain * 50).getInteger();
            color = hsv_to_rgb_fast(
                (frame_config.HUE + strain_shift) & 0xFF,
                frame_config.SATURATION,
                brightness
            );
        }
        
        // Nucleation sites pulse
        if (site.nucleation_site) {
            uint8_t pulse = (sin_lookup((millis() >> 2) & 0xFF) * 50 + 205).getInteger();
            color.r = (color.r * pulse) >> 8;
            color.g = (color.g * pulse) >> 8;
            color.b = (color.b * pulse) >> 8;
        }
        
        // Piezoelectric response (crystal generates light from pressure)
        if (site.phase != AMORPHOUS && pressure > SQ15x16(1.2)) {
            uint16_t piezo_glow = (pressure * site.order_parameter * 5000).getInteger();
            color = add_clipped(color, CRGB16(piezo_glow, piezo_glow, piezo_glow));
        }
        
        leds_16[i] = color;
    }
    
    // Fracture visualization on extreme strain
    for (int i = 1; i < NATIVE_RESOLUTION - 1; i++) {
        if (lattice[i].strain > SQ15x16(0.5) && random8() < 30) {
            // Crack propagation
            leds_16[i] = CRGB16(65535, 30000, 0);  // Bright orange crack
        }
    }
    
    apply_global_brightness();
}