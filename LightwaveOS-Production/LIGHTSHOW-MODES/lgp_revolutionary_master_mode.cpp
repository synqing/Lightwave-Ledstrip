#include "lgp_revolutionary_master_mode.h"
#include "../src/globals.h"
#include "../src/GDFT.h"
#include <math.h>

// Include all revolutionary effects
#include "lgp_quantum_consciousness_effect.h"
#include "lgp_synesthetic_orchestra_effect.h"
#include "lgp_temporal_echo_chamber_effect.h"
#include "lgp_cosmic_ray_shower_effect.h"
#include "lgp_dna_helix_unwinding_effect.h"
#include "lgp_dimensional_portal_effect.h"
#include "lgp_crystalline_lattice_effect.h"
#include "lgp_neuromorphic_lightning_effect.h"
#include "lgp_quantum_entanglement_effect.h"
#include "lgp_aurora_storm_matrix_effect.h"

#define MAX_CONCURRENT_EFFECTS 3
#define EFFECT_HISTORY_SIZE 20
#define MUSICAL_STRUCTURE_WINDOW 32

// Musical structure detection
enum MusicalSection {
    SECTION_INTRO,
    SECTION_VERSE,
    SECTION_CHORUS,
    SECTION_BRIDGE,
    SECTION_DROP,
    SECTION_BREAKDOWN,
    SECTION_BUILDUP,
    SECTION_OUTRO
};

// Effect categories for intelligent mixing
enum EffectCategory {
    CAT_CONSCIOUSNESS,   // Mind/brain effects
    CAT_PHYSICS,        // Particle/quantum effects
    CAT_BIOLOGICAL,     // DNA/neural effects
    CAT_COSMIC,         // Space/aurora effects
    CAT_TEMPORAL,       // Time-based effects
    CAT_CRYSTALLINE,    // Structure/pattern effects
    CAT_SYNESTHETIC     // Music visualization
};

// Master mode state
struct MasterModeState {
    // Currently active effects
    void (*active_effects[MAX_CONCURRENT_EFFECTS])();
    float effect_weights[MAX_CONCURRENT_EFFECTS];
    uint32_t effect_start_times[MAX_CONCURRENT_EFFECTS];
    uint8_t active_count;
    
    // Musical analysis
    MusicalSection current_section;
    float section_confidence;
    float musical_energy;
    float spectral_complexity;
    float rhythmic_regularity;
    float harmonic_richness;
    float dynamic_range;
    
    // Genre detection
    float genre_weights[8]; // Electronic, Rock, Classical, Jazz, Ambient, Hip-hop, World, Experimental
    uint8_t detected_genre;
    
    // Mood analysis
    float valence;  // Happy/sad
    float arousal;  // Calm/energetic
    float dominance; // Peaceful/aggressive
    
    // Effect selection intelligence
    uint8_t effect_history[EFFECT_HISTORY_SIZE];
    uint8_t history_index;
    uint32_t last_transition_time;
    uint32_t section_start_time;
    
    // Transition state
    bool in_transition;
    float transition_progress;
    CRGB16 transition_buffer[NATIVE_RESOLUTION];
    
    // Audio feature history
    float energy_history[MUSICAL_STRUCTURE_WINDOW];
    float centroid_history[MUSICAL_STRUCTURE_WINDOW];
    float flux_history[MUSICAL_STRUCTURE_WINDOW];
    uint8_t history_write_index;
    
    // Beat pattern recognition
    uint32_t beat_intervals[16];
    uint8_t beat_index;
    float detected_bpm;
    float beat_strength;
    
    // Special event triggers
    bool climax_detected;
    bool breakdown_detected;
    bool drop_incoming;
    uint32_t last_special_event;
};

static MasterModeState master_state;

// Effect metadata for intelligent selection
struct EffectInfo {
    void (*effect_function)();
    const char* name;
    EffectCategory category;
    float energy_affinity;      // 0=low energy, 1=high energy
    float complexity_rating;    // 0=simple, 1=complex
    float beat_reactivity;      // 0=ambient, 1=very beat reactive
    float frequency_focus;      // 0=bass, 0.5=mid, 1=treble
    uint8_t genre_affinities[8]; // 0-255 for each genre
};

static const EffectInfo EFFECT_DATABASE[] = {
    {
        light_mode_lgp_quantum_consciousness,
        "Quantum Consciousness",
        CAT_CONSCIOUSNESS,
        0.6f, 0.9f, 0.4f, 0.7f,
        {180, 120, 200, 160, 220, 100, 140, 255} // Best for Electronic, Classical, Ambient, Experimental
    },
    {
        light_mode_lgp_synesthetic_orchestra,
        "Synesthetic Orchestra",
        CAT_SYNESTHETIC,
        0.7f, 0.8f, 0.6f, 0.5f,
        {160, 180, 255, 220, 140, 120, 180, 200} // Best for Classical, Jazz
    },
    {
        light_mode_lgp_temporal_echo_chamber,
        "Temporal Echo Chamber",
        CAT_TEMPORAL,
        0.5f, 0.7f, 0.5f, 0.6f,
        {220, 140, 120, 180, 255, 160, 140, 240} // Best for Electronic, Ambient, Experimental
    },
    {
        light_mode_lgp_cosmic_ray_shower,
        "Cosmic Ray Shower",
        CAT_COSMIC,
        0.8f, 0.8f, 0.7f, 0.3f,
        {240, 200, 100, 120, 180, 220, 140, 200} // Best for Electronic, Rock, Hip-hop
    },
    {
        light_mode_lgp_dna_helix_unwinding,
        "DNA Helix Unwinding",
        CAT_BIOLOGICAL,
        0.4f, 0.9f, 0.3f, 0.5f,
        {140, 100, 180, 160, 200, 80, 120, 220} // Best for Classical, Ambient, Experimental
    },
    {
        light_mode_lgp_dimensional_portal,
        "Dimensional Portal",
        CAT_PHYSICS,
        0.9f, 0.9f, 0.8f, 0.4f,
        {255, 220, 80, 100, 160, 240, 120, 255} // Best for Electronic, Rock, Hip-hop, Experimental
    },
    {
        light_mode_lgp_crystalline_lattice,
        "Crystalline Lattice",
        CAT_CRYSTALLINE,
        0.5f, 0.7f, 0.6f, 0.8f,
        {180, 140, 200, 180, 220, 120, 160, 200} // Best for Electronic, Classical, Ambient
    },
    {
        light_mode_lgp_neuromorphic_lightning,
        "Neuromorphic Lightning",
        CAT_BIOLOGICAL,
        0.7f, 0.8f, 0.8f, 0.5f,
        {200, 180, 140, 160, 180, 200, 140, 240} // Best for Electronic, Rock, Hip-hop
    },
    {
        light_mode_lgp_quantum_entanglement,
        "Quantum Entanglement",
        CAT_PHYSICS,
        0.6f, 0.9f, 0.5f, 0.6f,
        {220, 140, 160, 180, 200, 120, 140, 255} // Best for Electronic, Experimental
    },
    {
        light_mode_lgp_aurora_storm_matrix,
        "Aurora Storm Matrix",
        CAT_COSMIC,
        0.8f, 0.7f, 0.7f, 0.4f,
        {200, 180, 140, 120, 220, 160, 180, 200} // Best for Electronic, Rock, Ambient
    }
};

// Initialize master mode
void init_master_mode() {
    static bool initialized = false;
    if (initialized) return;
    
    master_state.active_count = 0;
    master_state.current_section = SECTION_INTRO;
    master_state.section_confidence = 0.5f;
    master_state.history_index = 0;
    master_state.history_write_index = 0;
    master_state.last_transition_time = 0;
    master_state.section_start_time = millis();
    master_state.in_transition = false;
    master_state.transition_progress = 0;
    master_state.detected_genre = 0;
    master_state.beat_index = 0;
    master_state.detected_bpm = 120;
    master_state.beat_strength = 0;
    master_state.last_special_event = 0;
    
    // Initialize with a single effect
    master_state.active_effects[0] = light_mode_lgp_quantum_consciousness;
    master_state.effect_weights[0] = 1.0f;
    master_state.effect_start_times[0] = millis();
    master_state.active_count = 1;
    
    initialized = true;
}

// Analyze musical structure
MusicalSection detect_musical_section() {
    // Calculate recent averages
    float avg_energy = 0, avg_centroid = 0, avg_flux = 0;
    for (int i = 0; i < MUSICAL_STRUCTURE_WINDOW; i++) {
        avg_energy += master_state.energy_history[i];
        avg_centroid += master_state.centroid_history[i];
        avg_flux += master_state.flux_history[i];
    }
    avg_energy /= MUSICAL_STRUCTURE_WINDOW;
    avg_centroid /= MUSICAL_STRUCTURE_WINDOW;
    avg_flux /= MUSICAL_STRUCTURE_WINDOW;
    
    // Calculate recent trends
    float energy_trend = 0, centroid_trend = 0;
    for (int i = MUSICAL_STRUCTURE_WINDOW/2; i < MUSICAL_STRUCTURE_WINDOW; i++) {
        energy_trend += master_state.energy_history[i] - avg_energy;
        centroid_trend += master_state.centroid_history[i] - avg_centroid;
    }
    
    // Section detection logic
    if (avg_energy < 0.2f && avg_flux < 0.1f) {
        if (millis() - master_state.section_start_time < 20000) {
            return SECTION_INTRO;
        } else {
            return SECTION_OUTRO;
        }
    } else if (avg_energy < 0.3f && energy_trend < -0.1f) {
        return SECTION_BREAKDOWN;
    } else if (energy_trend > 0.2f && avg_energy < 0.6f) {
        return SECTION_BUILDUP;
    } else if (avg_energy > 0.8f && avg_flux > 0.3f) {
        return SECTION_DROP;
    } else if (avg_energy > 0.6f && master_state.rhythmic_regularity > 0.7f) {
        return SECTION_CHORUS;
    } else if (avg_centroid > avg_energy && master_state.harmonic_richness > 0.5f) {
        return SECTION_BRIDGE;
    } else {
        return SECTION_VERSE;
    }
}

// Select effect based on current musical context
uint8_t select_effect_for_context() {
    float best_score = -1;
    uint8_t best_effect = 0;
    
    for (uint8_t i = 0; i < sizeof(EFFECT_DATABASE) / sizeof(EffectInfo); i++) {
        const EffectInfo& effect = EFFECT_DATABASE[i];
        
        // Calculate fitness score
        float score = 0;
        
        // Energy matching
        float energy_diff = abs(effect.energy_affinity - master_state.musical_energy);
        score += (1.0f - energy_diff) * 0.3f;
        
        // Genre affinity
        score += effect.genre_affinities[master_state.detected_genre] / 255.0f * 0.25f;
        
        // Section appropriateness
        switch (master_state.current_section) {
            case SECTION_DROP:
            case SECTION_CHORUS:
                if (effect.energy_affinity > 0.7f) score += 0.2f;
                break;
            case SECTION_BREAKDOWN:
            case SECTION_INTRO:
                if (effect.energy_affinity < 0.4f) score += 0.2f;
                break;
            case SECTION_BUILDUP:
                if (effect.beat_reactivity > 0.6f) score += 0.2f;
                break;
        }
        
        // Variety bonus (penalize recent effects)
        bool recently_used = false;
        for (int h = 0; h < EFFECT_HISTORY_SIZE; h++) {
            if (master_state.effect_history[h] == i) {
                recently_used = true;
                score *= 0.5f; // Penalty for recent use
                break;
            }
        }
        
        // Complexity matching
        float complexity_diff = abs(effect.complexity_rating - master_state.spectral_complexity);
        score += (1.0f - complexity_diff) * 0.15f;
        
        // Beat reactivity for rhythmic music
        if (master_state.rhythmic_regularity > 0.7f) {
            score += effect.beat_reactivity * 0.1f;
        }
        
        if (score > best_score) {
            best_score = score;
            best_effect = i;
        }
    }
    
    return best_effect;
}

// Blend multiple effects together
void render_blended_effects() {
    // Clear blend buffer
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        leds_16[i] = CRGB16(0, 0, 0);
    }
    
    // Temporary buffers for each effect
    static CRGB16 effect_buffers[MAX_CONCURRENT_EFFECTS][NATIVE_RESOLUTION];
    
    // Render each active effect to its buffer
    for (uint8_t e = 0; e < master_state.active_count; e++) {
        // Backup current LED state
        CRGB16 backup[NATIVE_RESOLUTION];
        memcpy(backup, leds_16, sizeof(CRGB16) * NATIVE_RESOLUTION);
        
        // Clear for this effect
        memset(leds_16, 0, sizeof(CRGB16) * NATIVE_RESOLUTION);
        
        // Render effect
        if (master_state.active_effects[e]) {
            master_state.active_effects[e]();
        }
        
        // Copy to effect buffer
        memcpy(effect_buffers[e], leds_16, sizeof(CRGB16) * NATIVE_RESOLUTION);
        
        // Restore backup
        memcpy(leds_16, backup, sizeof(CRGB16) * NATIVE_RESOLUTION);
    }
    
    // Blend all effects based on weights and musical context
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        CRGB16 blended = CRGB16(0, 0, 0);
        
        for (uint8_t e = 0; e < master_state.active_count; e++) {
            float weight = master_state.effect_weights[e];
            
            // Apply section-specific blending
            switch (master_state.current_section) {
                case SECTION_DROP:
                    // Additive blending for maximum impact
                    blended = add_clipped(blended, scale_color(effect_buffers[e][i], SQ15x16(weight)));
                    break;
                    
                case SECTION_BREAKDOWN:
                    // Alpha blending for smooth transitions
                    blended = blend_alpha(blended, effect_buffers[e][i], SQ15x16(weight));
                    break;
                    
                case SECTION_BUILDUP:
                    // Oscillating blend for tension
                    float osc = sin(millis() * 0.01f + i * 0.1f) * 0.3f + 0.7f;
                    blended = add_clipped(blended, scale_color(effect_buffers[e][i], SQ15x16(weight * osc)));
                    break;
                    
                default:
                    // Standard weighted blend
                    blended = add_clipped(blended, scale_color(effect_buffers[e][i], SQ15x16(weight * 0.7f)));
            }
        }
        
        // Apply transition if active
        if (master_state.in_transition) {
            blended = blend_alpha(master_state.transition_buffer[i], blended, 
                                SQ15x16(master_state.transition_progress));
        }
        
        leds_16[i] = blended;
    }
}

void light_mode_lgp_revolutionary_master() {
    init_master_mode();
    cache_frame_config();
    
    // Comprehensive audio analysis
    get_smooth_spectrogram();
    get_smooth_chromagram();
    calculate_vu();
    
    // Calculate audio features
    float total_energy = 0;
    float spectral_centroid = 0;
    float spectral_flux = 0;
    
    for (int i = 0; i < 96; i++) {
        total_energy += spectrogram_smooth[i];
        spectral_centroid += i * spectrogram_smooth[i];
    }
    
    if (total_energy > 0) {
        spectral_centroid /= total_energy;
    }
    
    // Calculate spectral flux
    static float prev_spectrum[96] = {0};
    for (int i = 0; i < 96; i++) {
        float diff = spectrogram_smooth[i] - prev_spectrum[i];
        if (diff > 0) spectral_flux += diff;
        prev_spectrum[i] = spectrogram_smooth[i];
    }
    
    // Update history
    master_state.energy_history[master_state.history_write_index] = total_energy;
    master_state.centroid_history[master_state.history_write_index] = spectral_centroid / 96.0f;
    master_state.flux_history[master_state.history_write_index] = spectral_flux;
    master_state.history_write_index = (master_state.history_write_index + 1) % MUSICAL_STRUCTURE_WINDOW;
    
    // Update musical analysis
    master_state.musical_energy = total_energy;
    master_state.spectral_complexity = spectral_centroid / 48.0f; // Normalized
    
    // Calculate rhythmic regularity from beat intervals
    if (spectral_flux > 0.3f && total_energy > 0.4f) {
        uint32_t now = millis();
        master_state.beat_intervals[master_state.beat_index] = now;
        master_state.beat_index = (master_state.beat_index + 1) % 16;
        
        // Calculate BPM from intervals
        float avg_interval = 0;
        int valid_intervals = 0;
        for (int i = 1; i < 16; i++) {
            uint32_t interval = master_state.beat_intervals[i] - master_state.beat_intervals[i-1];
            if (interval > 200 && interval < 2000) { // Valid range: 30-300 BPM
                avg_interval += interval;
                valid_intervals++;
            }
        }
        if (valid_intervals > 0) {
            avg_interval /= valid_intervals;
            master_state.detected_bpm = 60000.0f / avg_interval;
            master_state.beat_strength = 1.0f;
        }
    }
    
    // Calculate harmonic richness from chromagram
    master_state.harmonic_richness = 0;
    int active_notes = 0;
    for (int i = 0; i < 12; i++) {
        if (chromagram_smooth[i] > 0.1f) {
            active_notes++;
            master_state.harmonic_richness += chromagram_smooth[i];
        }
    }
    master_state.harmonic_richness = active_notes / 12.0f;
    
    // Simple genre detection based on spectral characteristics
    float bass_energy = 0, mid_energy = 0, high_energy = 0;
    for (int i = 0; i < 20; i++) bass_energy += spectrogram_smooth[i];
    for (int i = 20; i < 50; i++) mid_energy += spectrogram_smooth[i];
    for (int i = 50; i < 96; i++) high_energy += spectrogram_smooth[i];
    
    bass_energy /= 20.0f;
    mid_energy /= 30.0f;
    high_energy /= 46.0f;
    
    // Update genre weights (simplified)
    if (bass_energy > mid_energy && bass_energy > high_energy) {
        if (master_state.rhythmic_regularity > 0.8f) {
            master_state.detected_genre = 0; // Electronic
        } else {
            master_state.detected_genre = 5; // Hip-hop
        }
    } else if (high_energy > bass_energy && spectral_centroid > 40) {
        master_state.detected_genre = 1; // Rock
    } else if (master_state.harmonic_richness > 0.7f) {
        if (master_state.rhythmic_regularity < 0.5f) {
            master_state.detected_genre = 2; // Classical
        } else {
            master_state.detected_genre = 3; // Jazz
        }
    } else {
        master_state.detected_genre = 4; // Ambient
    }
    
    // Update musical section
    MusicalSection new_section = detect_musical_section();
    if (new_section != master_state.current_section) {
        master_state.current_section = new_section;
        master_state.section_start_time = millis();
        
        // Trigger effect change on section change
        if (millis() - master_state.last_transition_time > 5000) { // Min 5 seconds between changes
            // Select new effect
            uint8_t new_effect_index = select_effect_for_context();
            
            // Start transition
            master_state.in_transition = true;
            master_state.transition_progress = 0;
            memcpy(master_state.transition_buffer, leds_16, sizeof(CRGB16) * NATIVE_RESOLUTION);
            
            // Update effect history
            master_state.effect_history[master_state.history_index] = new_effect_index;
            master_state.history_index = (master_state.history_index + 1) % EFFECT_HISTORY_SIZE;
            
            // Update active effects based on section
            switch (new_section) {
                case SECTION_DROP:
                case SECTION_CHORUS:
                    // High energy - use 2-3 effects
                    master_state.active_count = min(3, MAX_CONCURRENT_EFFECTS);
                    master_state.active_effects[0] = EFFECT_DATABASE[new_effect_index].effect_function;
                    master_state.effect_weights[0] = 0.6f;
                    
                    // Add complementary effects
                    if (master_state.active_count > 1) {
                        uint8_t second_effect = (new_effect_index + 3) % 10;
                        master_state.active_effects[1] = EFFECT_DATABASE[second_effect].effect_function;
                        master_state.effect_weights[1] = 0.4f;
                    }
                    if (master_state.active_count > 2) {
                        master_state.active_effects[2] = light_mode_lgp_aurora_storm_matrix;
                        master_state.effect_weights[2] = 0.3f;
                    }
                    break;
                    
                case SECTION_BREAKDOWN:
                case SECTION_INTRO:
                case SECTION_OUTRO:
                    // Low energy - single effect
                    master_state.active_count = 1;
                    master_state.active_effects[0] = EFFECT_DATABASE[new_effect_index].effect_function;
                    master_state.effect_weights[0] = 1.0f;
                    break;
                    
                default:
                    // Medium energy - 2 effects
                    master_state.active_count = 2;
                    master_state.active_effects[0] = EFFECT_DATABASE[new_effect_index].effect_function;
                    master_state.effect_weights[0] = 0.7f;
                    
                    uint8_t second_effect = (new_effect_index + 5) % 10;
                    master_state.active_effects[1] = EFFECT_DATABASE[second_effect].effect_function;
                    master_state.effect_weights[1] = 0.3f;
                    break;
            }
            
            master_state.last_transition_time = millis();
        }
    }
    
    // Update transition
    if (master_state.in_transition) {
        master_state.transition_progress += 0.02f;
        if (master_state.transition_progress >= 1.0f) {
            master_state.in_transition = false;
            master_state.transition_progress = 1.0f;
        }
    }
    
    // Special event detection
    if (total_energy > 0.9f && spectral_flux > 0.5f && 
        millis() - master_state.last_special_event > 10000) {
        master_state.climax_detected = true;
        master_state.last_special_event = millis();
        
        // Maximum intensity mode
        for (int i = 0; i < master_state.active_count; i++) {
            master_state.effect_weights[i] = 1.0f;
        }
    }
    
    // Update effect weights based on audio
    for (int i = 0; i < master_state.active_count; i++) {
        // Fade in new effects
        uint32_t age = millis() - master_state.effect_start_times[i];
        if (age < 2000) {
            master_state.effect_weights[i] *= age / 2000.0f;
        }
        
        // Modulate by energy
        if (master_state.current_section == SECTION_DROP) {
            master_state.effect_weights[i] = 0.7f + total_energy * 0.3f;
        }
    }
    
    // Render the blended effects
    render_blended_effects();
    
    // Post-processing based on section
    switch (master_state.current_section) {
        case SECTION_DROP:
            // Add strobe effect on strong beats
            if (master_state.beat_strength > 0.8f && random8() < 100) {
                for (int i = 0; i < NATIVE_RESOLUTION; i++) {
                    leds_16[i] = CRGB16(65535, 65535, 65535);
                }
            }
            break;
            
        case SECTION_BUILDUP:
            // Add tension with flickering
            for (int i = 0; i < NATIVE_RESOLUTION; i++) {
                if (random8() < 20) {
                    leds_16[i] = scale_color(leds_16[i], SQ15x16(0.3f));
                }
            }
            break;
            
        case SECTION_BREAKDOWN:
            // Subtle breathing effect
            float breath = (sin(millis() * 0.001f) + 1.0f) * 0.3f + 0.4f;
            for (int i = 0; i < NATIVE_RESOLUTION; i++) {
                leds_16[i] = scale_color(leds_16[i], SQ15x16(breath));
            }
            break;
    }
    
    // Final touch - energy reactive global modulation
    float global_mod = 0.5f + total_energy * 0.5f;
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        leds_16[i] = scale_color(leds_16[i], SQ15x16(global_mod));
    }
    
    apply_global_brightness();
}