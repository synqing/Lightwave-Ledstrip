#include "lgp_dna_helix_unwinding_effect.h"

// DNA base types
enum BaseType {
    ADENINE,    // A - pairs with T
    THYMINE,    // T - pairs with A  
    GUANINE,    // G - pairs with C
    CYTOSINE    // C - pairs with G
};

// Base pair structure
struct BasePair {
    BaseType base1;
    BaseType base2;
    SQ15x16 separation;      // Distance between paired bases
    SQ15x16 twist_angle;     // Helical twist angle
    bool transcribing;       // Being read by RNA polymerase
    uint8_t mutation_chance; // Audio-driven mutation probability
};

// DNA strand state
static const int MAX_BASE_PAIRS = 30;
static BasePair dna_strand[MAX_BASE_PAIRS];
static SQ15x16 helix_phase = 0;
static SQ15x16 unwinding_position = 0;
static SQ15x16 polymerase_position = 0;
static bool replication_active = false;
static uint32_t last_replication_time = 0;

// Get complementary base
static BaseType get_complement(BaseType base) {
    switch(base) {
        case ADENINE: return THYMINE;
        case THYMINE: return ADENINE;
        case GUANINE: return CYTOSINE;
        case CYTOSINE: return GUANINE;
        default: return ADENINE;
    }
}

// Get base color
static CRGB16 get_base_color(BaseType base, uint8_t brightness) {
    switch(base) {
        case ADENINE:  // Green
            return CRGB16(0, brightness * 256, brightness * 128);
        case THYMINE:  // Red
            return CRGB16(brightness * 256, 0, brightness * 128);
        case GUANINE:  // Blue
            return CRGB16(brightness * 128, brightness * 128, brightness * 256);
        case CYTOSINE: // Yellow
            return CRGB16(brightness * 256, brightness * 200, 0);
        default:
            return CRGB16(brightness * 256, brightness * 256, brightness * 256);
    }
}

// Initialize DNA strand with random sequence
static void init_dna_strand() {
    for (int i = 0; i < MAX_BASE_PAIRS; i++) {
        // Random base on first strand
        dna_strand[i].base1 = (BaseType)(random8() & 0x03);
        // Complementary base on second strand
        dna_strand[i].base2 = get_complement(dna_strand[i].base1);
        // Initially paired
        dna_strand[i].separation = 0;
        dna_strand[i].twist_angle = SQ15x16(i * 36);  // 36 degrees per base pair
        dna_strand[i].transcribing = false;
        dna_strand[i].mutation_chance = 0;
    }
}

void light_mode_lgp_dna_helix_unwinding() {
    cache_frame_config();
    
    // Initialize on first run
    static bool initialized = false;
    if (!initialized) {
        init_dna_strand();
        initialized = true;
    }
    
    // Audio analysis for biological processes
    uint16_t bass_energy = 0;
    uint16_t mid_energy = 0;
    uint16_t high_energy = 0;
    bool beat_detected = false;
    
    if (MAX_SAMPLING_FREQ > 0) {
        bass_energy = (uint16_t)sampleAvgMinor[0] + sampleAvgMinor[1];
        mid_energy = (uint16_t)sampleAvgMinor[2] + sampleAvgMinor[3];
        high_energy = (uint16_t)sampleAvgMinor[4] + sampleAvgMinor[5];
        
        // Beat detection for replication events
        static uint16_t prev_bass = 0;
        if (bass_energy > prev_bass * 1.5 && bass_energy > 2000) {
            beat_detected = true;
        }
        prev_bass = bass_energy;
    }
    
    // Start replication on beat or timer
    uint32_t current_time = millis();
    if ((beat_detected || current_time - last_replication_time > 4000) && !replication_active) {
        replication_active = true;
        unwinding_position = 0;
        polymerase_position = 0;
        last_replication_time = current_time;
    }
    
    // Update helix rotation
    helix_phase += SQ15x16(frame_config.SPEED) / 32;
    
    // Update unwinding process
    if (replication_active) {
        // Helicase unwinding speed affected by audio
        SQ15x16 unwind_speed = SQ15x16(frame_config.SPEED) / 64 + SQ15x16(bass_energy) / 10000;
        unwinding_position += unwind_speed;
        
        // RNA polymerase follows behind
        if (unwinding_position > 5) {
            polymerase_position += unwind_speed * SQ15x16(0.7);
        }
        
        // Complete replication
        if (unwinding_position >= MAX_BASE_PAIRS) {
            replication_active = false;
            // Re-zip the DNA
            for (int i = 0; i < MAX_BASE_PAIRS; i++) {
                dna_strand[i].separation = 0;
                dna_strand[i].transcribing = false;
            }
        }
    }
    
    // Update base pair states
    for (int i = 0; i < MAX_BASE_PAIRS; i++) {
        if (replication_active) {
            // Unwinding region
            if (i < unwinding_position.getInteger()) {
                // Separate base pairs
                SQ15x16 target_sep = SQ15x16(10);
                dna_strand[i].separation += (target_sep - dna_strand[i].separation) * SQ15x16(0.1);
                
                // Transcription region
                if (i < polymerase_position.getInteger() && i > polymerase_position.getInteger() - 3) {
                    dna_strand[i].transcribing = true;
                } else {
                    dna_strand[i].transcribing = false;
                }
            } else {
                // Still wound
                dna_strand[i].separation *= SQ15x16(0.9);
            }
        }
        
        // Audio-driven mutations
        dna_strand[i].mutation_chance = (high_energy >> 4);
        if (random8() < dna_strand[i].mutation_chance && beat_detected) {
            // Mutation event!
            dna_strand[i].base1 = (BaseType)(random8() & 0x03);
            dna_strand[i].base2 = get_complement(dna_strand[i].base1);
        }
    }
    
    // Clear strip
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        leds_16[i] = CRGB16(0, 0, 0);
    }
    
    // Render DNA helix
    float pixels_per_base = (float)NATIVE_RESOLUTION / MAX_BASE_PAIRS;
    
    for (int bp = 0; bp < MAX_BASE_PAIRS; bp++) {
        int base_pos = bp * pixels_per_base;
        
        // Calculate helix positions
        SQ15x16 angle = dna_strand[bp].twist_angle + helix_phase;
        SQ15x16 sin_val = sin_lookup(angle.getInteger() & 0xFF);
        SQ15x16 cos_val = cos_lookup(angle.getInteger() & 0xFF);
        
        // Helix radius affected by separation
        SQ15x16 radius = SQ15x16(8) + dna_strand[bp].separation;
        
        // Position of each base in the pair
        int strand1_offset = (sin_val * radius).getInteger();
        int strand2_offset = -(sin_val * radius).getInteger();
        
        // Draw phosphate-sugar backbone
        for (int p = 0; p < pixels_per_base && base_pos + p < NATIVE_RESOLUTION; p++) {
            int pixel = base_pos + p;
            
            // Backbone brightness
            uint8_t backbone_bright = 40 + (mid_energy >> 6);
            
            // Strand 1 backbone
            int pos1 = pixel + strand1_offset;
            if (pos1 >= 0 && pos1 < NATIVE_RESOLUTION) {
                CRGB16 backbone_color = CRGB16(backbone_bright * 100, backbone_bright * 100, backbone_bright * 200);
                leds_16[pos1] = add_clipped(leds_16[pos1], backbone_color);
            }
            
            // Strand 2 backbone
            int pos2 = pixel + strand2_offset;
            if (pos2 >= 0 && pos2 < NATIVE_RESOLUTION) {
                CRGB16 backbone_color = CRGB16(backbone_bright * 100, backbone_bright * 100, backbone_bright * 200);
                leds_16[pos2] = add_clipped(leds_16[pos2], backbone_color);
            }
        }
        
        // Draw base pairs
        uint8_t base_brightness = 150 + (cos_val * 50).getInteger();
        
        // Add transcription glow
        if (dna_strand[bp].transcribing) {
            base_brightness = 255;
        }
        
        // Draw hydrogen bonds between bases
        if (dna_strand[bp].separation < 5) {
            int bond_steps = abs(strand2_offset - strand1_offset);
            for (int b = 0; b <= bond_steps; b++) {
                int bond_pos = base_pos + strand1_offset + (b * (strand2_offset - strand1_offset)) / bond_steps;
                if (bond_pos >= 0 && bond_pos < NATIVE_RESOLUTION) {
                    uint8_t bond_bright = base_brightness * (1.0 - dna_strand[bp].separation.getInteger() / 10.0);
                    
                    // Mix colors of both bases
                    CRGB16 base1_color = get_base_color(dna_strand[bp].base1, bond_bright / 2);
                    CRGB16 base2_color = get_base_color(dna_strand[bp].base2, bond_bright / 2);
                    CRGB16 bond_color = CRGB16(
                        (base1_color.r + base2_color.r) / 2,
                        (base1_color.g + base2_color.g) / 2,
                        (base1_color.b + base2_color.b) / 2
                    );
                    
                    leds_16[bond_pos] = add_clipped(leds_16[bond_pos], bond_color);
                }
            }
        } else {
            // Draw separated bases
            int pos1 = base_pos + strand1_offset;
            if (pos1 >= 0 && pos1 < NATIVE_RESOLUTION) {
                CRGB16 base1_color = get_base_color(dna_strand[bp].base1, base_brightness);
                leds_16[pos1] = add_clipped(leds_16[pos1], base1_color);
            }
            
            int pos2 = base_pos + strand2_offset;
            if (pos2 >= 0 && pos2 < NATIVE_RESOLUTION) {
                CRGB16 base2_color = get_base_color(dna_strand[bp].base2, base_brightness);
                leds_16[pos2] = add_clipped(leds_16[pos2], base2_color);
            }
        }
        
        // RNA polymerase visualization
        if (dna_strand[bp].transcribing) {
            for (int r = -3; r <= 3; r++) {
                int pol_pos = base_pos + r;
                if (pol_pos >= 0 && pol_pos < NATIVE_RESOLUTION) {
                    CRGB16 pol_color = CRGB16(20000, 10000, 40000);  // Purple polymerase
                    leds_16[pol_pos] = add_clipped(leds_16[pol_pos], pol_color);
                }
            }
        }
    }
    
    // Add mutation flashes
    if (beat_detected) {
        for (int i = 0; i < 3; i++) {
            int flash_pos = random16(NATIVE_RESOLUTION);
            CRGB16 mutation_flash = CRGB16(65535, 30000, 0);  // Orange flash
            leds_16[flash_pos] = mutation_flash;
        }
    }
    
    apply_global_brightness();
}