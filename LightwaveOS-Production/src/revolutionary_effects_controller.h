#ifndef REVOLUTIONARY_EFFECTS_CONTROLLER_H
#define REVOLUTIONARY_EFFECTS_CONTROLLER_H

#include "globals.h"
#include "constants.h"
#include "effect_blending.h"

// Revolutionary Effects Controller - Master orchestrator for all cutting-edge effects
// Provides intelligent effect selection, transitions, and audio-reactive combinations

// Revolutionary effect IDs
enum RevolutionaryEffect {
    REV_QUANTUM_CONSCIOUSNESS,
    REV_SYNESTHETIC_ORCHESTRA,
    REV_TEMPORAL_ECHO_CHAMBER,
    REV_COSMIC_RAY_SHOWER,
    REV_DNA_HELIX_UNWINDING,
    REV_DIMENSIONAL_PORTAL,
    REV_CRYSTALLINE_LATTICE,
    REV_NEUROMORPHIC_LIGHTNING,
    REV_QUANTUM_ENTANGLEMENT,
    REV_AURORA_STORM_MATRIX,
    REV_EFFECT_COUNT
};

// Effect mood categories for intelligent selection
enum EffectMood {
    MOOD_ENERGETIC,      // High energy, fast-paced
    MOOD_CONTEMPLATIVE,  // Slow, thoughtful
    MOOD_PSYCHEDELIC,    // Mind-bending, trippy
    MOOD_SCIENTIFIC,     // Educational, accurate
    MOOD_ATMOSPHERIC,    // Ambient, environmental
    MOOD_CHAOTIC,        // Intense, unpredictable
    MOOD_HARMONIC,       // Musical, synchronized
    MOOD_COSMIC          // Space, universe-themed
};

// Effect combination presets for revolutionary experiences
enum RevolutionaryCombo {
    COMBO_NEURAL_SYMPHONY,         // Quantum Consciousness + Synesthetic Orchestra
    COMBO_SPACETIME_CASCADE,       // Temporal Echo + Cosmic Ray Shower
    COMBO_GENETIC_PORTAL,          // DNA Helix + Dimensional Portal
    COMBO_CRYSTAL_NEURONS,         // Crystalline Lattice + Neuromorphic Lightning
    COMBO_QUANTUM_AURORA,          // Quantum Entanglement + Aurora Storm
    COMBO_CONSCIOUSNESS_STORM,     // All consciousness effects combined
    COMBO_PHYSICS_PLAYGROUND,      // All physics effects combined
    COMBO_ULTIMATE_EXPERIENCE      // Everything at once (use sparingly!)
};

// Effect metadata for intelligent control
struct EffectMetadata {
    const char* name;
    const char* description;
    EffectMood primary_mood;
    EffectMood secondary_mood;
    float energy_level;         // 0.0 (calm) to 1.0 (intense)
    float complexity_rating;    // 0.0 (simple) to 1.0 (complex)
    float audio_sensitivity;    // How reactive to audio
    bool beat_reactive;         // Responds to beats
    bool frequency_selective;   // Uses specific frequency bands
    uint8_t optimal_bpm_min;    // Best tempo range
    uint8_t optimal_bpm_max;
    void (*effect_function)();  // Function pointer
};

// Revolutionary Effects Controller class
class RevolutionaryEffectsController {
private:
    static const uint8_t MAX_ACTIVE_EFFECTS = 3;
    
    RevolutionaryEffect current_effects[MAX_ACTIVE_EFFECTS];
    uint8_t active_effect_count;
    
    EffectMood current_mood;
    float mood_confidence;
    
    // Audio analysis state
    float current_bpm;
    float energy_level;
    float frequency_profile[8];  // Frequency band distribution
    float harmonic_content;
    float rhythmic_stability;
    
    // Effect transition state
    bool transitioning;
    uint32_t transition_start_time;
    uint32_t transition_duration;
    RevolutionaryEffect transition_from;
    RevolutionaryEffect transition_to;
    float transition_progress;
    
    // Intelligent selection parameters
    uint32_t last_effect_change;
    uint32_t min_effect_duration;
    float effect_change_threshold;
    bool auto_mode_enabled;
    
    // Performance optimization
    bool gpu_acceleration_enabled;
    uint8_t quality_level;  // 1-10
    
    // Effect history for variety
    RevolutionaryEffect effect_history[10];
    uint8_t history_index;
    
    // Helper functions
    void analyze_audio_characteristics();
    EffectMood determine_mood_from_audio();
    float calculate_effect_fitness(RevolutionaryEffect effect);
    bool should_change_effect();
    RevolutionaryEffect select_next_effect();
    void update_effect_history(RevolutionaryEffect effect);
    
public:
    RevolutionaryEffectsController();
    
    // Effect control
    void set_effect(RevolutionaryEffect effect);
    void set_combo(RevolutionaryCombo combo);
    void enable_auto_mode(bool enabled);
    void set_quality_level(uint8_t level);
    
    // Mood control
    void set_mood(EffectMood mood);
    void set_mood_confidence(float confidence);
    
    // Transition control
    void start_transition(RevolutionaryEffect to, uint32_t duration_ms);
    void set_transition_curve(uint8_t curve_type);
    
    // Audio response tuning
    void set_audio_sensitivity(float sensitivity);
    void set_frequency_weights(float* weights);
    void set_beat_response(float response);
    
    // Main update function
    void update();
    void render();
    
    // Status queries
    RevolutionaryEffect get_current_effect() const;
    EffectMood get_current_mood() const;
    float get_energy_level() const;
    bool is_transitioning() const;
    
    // Preset management
    void save_preset(uint8_t slot);
    void load_preset(uint8_t slot);
    
    // Special functions
    void trigger_climax();      // Maximum intensity burst
    void trigger_breakdown();   // Minimal, contemplative
    void sync_to_beat();       // Force beat synchronization
    void randomize_parameters(); // Controlled chaos
};

// Global instance
extern RevolutionaryEffectsController g_revolutionary_controller;

// Effect registration (populated at startup)
extern EffectMetadata g_revolutionary_effects[REV_EFFECT_COUNT];

// Helper functions for effect combinations
void setup_neural_symphony_combo();
void setup_spacetime_cascade_combo();
void setup_genetic_portal_combo();
void setup_crystal_neurons_combo();
void setup_quantum_aurora_combo();
void setup_consciousness_storm_combo();
void setup_physics_playground_combo();
void setup_ultimate_experience_combo();

// Audio analysis helpers
float analyze_genre_characteristics();
float detect_musical_structure();
float calculate_spectral_complexity();

// Performance monitoring integration
void log_revolutionary_performance();

#endif // REVOLUTIONARY_EFFECTS_CONTROLLER_H