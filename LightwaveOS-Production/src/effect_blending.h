#ifndef EFFECT_BLENDING_H
#define EFFECT_BLENDING_H

#include "globals.h"
#include "constants.h"
#include "led_utilities.h"

// Effect blending and combination system for LightwaveOS
// Allows real-time mixing of multiple effects with audio reactive control

// Blending modes for effect combination
enum BlendMode {
    BLEND_ADDITIVE,      // Add colors together (brighten)
    BLEND_MULTIPLY,      // Multiply colors (darken/filter)
    BLEND_SCREEN,        // Screen blend (brighten while preserving bright areas)
    BLEND_OVERLAY,       // Overlay blend (contrast enhancement)
    BLEND_ALPHA,         // Alpha blend with opacity control
    BLEND_XOR,           // XOR blend for interference patterns
    BLEND_SUBTRACT,      // Subtract colors (darkening effect)
    BLEND_DIFFERENCE,    // Absolute difference (creates outlines)
    BLEND_LIGHTEN,       // Keep lighter color
    BLEND_DARKEN,        // Keep darker color
    BLEND_HUE_SHIFT,     // Hue rotation blend
    BLEND_SPECTRAL       // Spectral addition (physically accurate)
};

// Effect layer structure
struct EffectLayer {
    void (*effect_function)();   // Pointer to effect function
    float opacity;               // Layer opacity (0.0-1.0)
    BlendMode blend_mode;        // How to blend with previous layers
    bool enabled;                // Whether layer is active
    float audio_sensitivity;     // How much audio affects this layer
    uint8_t frequency_band;      // Which frequency band controls this layer (0-11)
    CRGB16 tint_color;          // Color tint to apply to layer
    float speed_multiplier;      // Speed modification for this layer
    bool beat_reactive;          // Whether layer responds to beats
    float phase_offset;          // Phase offset for layer timing
};

// Effect combination presets
enum EffectCombo {
    COMBO_QUANTUM_AURORA,        // Quantum tunneling + Aurora borealis
    COMBO_GRAVITATIONAL_WAVES,   // Gravitational lens + Ripples
    COMBO_BIOLUMINESCENT_HOLO,   // Bioluminescent + Holographic
    COMBO_SOLITON_INTERFERENCE,  // Soliton waves + Moiré curtains
    COMBO_CHAOS_CRYSTALLINE,     // Chaos + Brillouin zones
    COMBO_PLASMA_STORM,          // Color accelerator + Star burst
    COMBO_METAMATERIAL_RIPPLES,  // Metamaterial cloak + Radial ripples
    COMBO_SHOCK_AURORA,          // Shock waves + Aurora
    COMBO_QUANTUM_CHAOS,         // Quantum tunneling + Chaos
    COMBO_HOLOGRAPHIC_STORM      // Holographic + Multiple effects
};

// Main effect blending system
class EffectBlender {
private:
    static const uint8_t MAX_LAYERS = 4;
    EffectLayer layers[MAX_LAYERS];
    CRGB16 layer_buffers[MAX_LAYERS][NATIVE_RESOLUTION];
    CRGB16 blend_buffer[NATIVE_RESOLUTION];
    
    uint8_t active_layers;
    float master_opacity;
    bool audio_reactive_blending;
    float blend_audio_sensitivity;
    
    // Audio analysis for blending control
    float bass_energy;
    float mid_energy;
    float high_energy;
    float total_energy;
    bool beat_detected;
    
    // Blend functions
    CRGB16 blend_additive(CRGB16 base, CRGB16 overlay, float opacity);
    CRGB16 blend_multiply(CRGB16 base, CRGB16 overlay, float opacity);
    CRGB16 blend_screen(CRGB16 base, CRGB16 overlay, float opacity);
    CRGB16 blend_overlay(CRGB16 base, CRGB16 overlay, float opacity);
    CRGB16 blend_alpha(CRGB16 base, CRGB16 overlay, float opacity);
    CRGB16 blend_xor(CRGB16 base, CRGB16 overlay, float opacity);
    CRGB16 blend_subtract(CRGB16 base, CRGB16 overlay, float opacity);
    CRGB16 blend_difference(CRGB16 base, CRGB16 overlay, float opacity);
    CRGB16 blend_lighten(CRGB16 base, CRGB16 overlay, float opacity);
    CRGB16 blend_darken(CRGB16 base, CRGB16 overlay, float opacity);
    CRGB16 blend_hue_shift(CRGB16 base, CRGB16 overlay, float opacity);
    CRGB16 blend_spectral(CRGB16 base, CRGB16 overlay, float opacity);
    
    CRGB16 apply_blend_mode(CRGB16 base, CRGB16 overlay, BlendMode mode, float opacity);
    
public:
    EffectBlender();
    
    // Layer management
    bool add_layer(void (*effect_func)(), BlendMode mode, float opacity = 1.0f);
    void remove_layer(uint8_t layer_index);
    void clear_all_layers();
    
    // Layer configuration
    void set_layer_opacity(uint8_t layer_index, float opacity);
    void set_layer_blend_mode(uint8_t layer_index, BlendMode mode);
    void set_layer_audio_sensitivity(uint8_t layer_index, float sensitivity);
    void set_layer_frequency_band(uint8_t layer_index, uint8_t band);
    void set_layer_tint(uint8_t layer_index, CRGB16 tint);
    void set_layer_speed(uint8_t layer_index, float speed_mult);
    void set_layer_beat_reactive(uint8_t layer_index, bool reactive);
    void set_layer_phase_offset(uint8_t layer_index, float offset);
    
    // Preset combinations
    void load_combo_preset(EffectCombo combo);
    
    // Audio reactive blending
    void enable_audio_reactive_blending(bool enabled);
    void set_blend_audio_sensitivity(float sensitivity);
    
    // Main processing
    void update_audio_analysis();
    void render_layers();
    void blend_layers();
    void apply_to_output();
    
    // Main render function
    void render();
    
    // Utility functions
    uint8_t get_active_layer_count() const;
    float get_layer_opacity(uint8_t layer_index) const;
    BlendMode get_layer_blend_mode(uint8_t layer_index) const;
    bool is_layer_enabled(uint8_t layer_index) const;
};

// Global effect blender instance
extern EffectBlender g_effect_blender;

// Helper functions for common effect combinations
void setup_quantum_aurora_combo();
void setup_gravitational_waves_combo();
void setup_bioluminescent_holo_combo();
void setup_soliton_interference_combo();
void setup_chaos_crystalline_combo();
void setup_plasma_storm_combo();
void setup_metamaterial_ripples_combo();
void setup_shock_aurora_combo();
void setup_quantum_chaos_combo();
void setup_holographic_storm_combo();

// Audio reactive blend control functions
void update_blend_audio_reactivity();
float calculate_frequency_band_energy(uint8_t band);
bool detect_audio_beat();

#endif // EFFECT_BLENDING_H