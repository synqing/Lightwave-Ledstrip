#include "effect_blending.h"
#include "globals.h"
#include "GDFT.h"
#include <cstring>

// Global effect blender instance
EffectBlender g_effect_blender;

EffectBlender::EffectBlender() {
    active_layers = 0;
    master_opacity = 1.0f;
    audio_reactive_blending = true;
    blend_audio_sensitivity = 0.5f;
    
    bass_energy = 0;
    mid_energy = 0;
    high_energy = 0;
    total_energy = 0;
    beat_detected = false;
    
    // Initialize all layers as disabled
    for (uint8_t i = 0; i < MAX_LAYERS; i++) {
        layers[i].effect_function = nullptr;
        layers[i].opacity = 1.0f;
        layers[i].blend_mode = BLEND_ADDITIVE;
        layers[i].enabled = false;
        layers[i].audio_sensitivity = 0.5f;
        layers[i].frequency_band = 0;
        layers[i].tint_color = CRGB16(65535, 65535, 65535);
        layers[i].speed_multiplier = 1.0f;
        layers[i].beat_reactive = false;
        layers[i].phase_offset = 0.0f;
        
        // Clear layer buffer
        memset(layer_buffers[i], 0, sizeof(CRGB16) * NATIVE_RESOLUTION);
    }
    
    // Clear blend buffer
    memset(blend_buffer, 0, sizeof(CRGB16) * NATIVE_RESOLUTION);
}

bool EffectBlender::add_layer(void (*effect_func)(), BlendMode mode, float opacity) {
    if (active_layers >= MAX_LAYERS || effect_func == nullptr) {
        return false;
    }
    
    layers[active_layers].effect_function = effect_func;
    layers[active_layers].opacity = opacity;
    layers[active_layers].blend_mode = mode;
    layers[active_layers].enabled = true;
    layers[active_layers].audio_sensitivity = 0.5f;
    layers[active_layers].frequency_band = active_layers % 12;  // Distribute across frequency bands
    layers[active_layers].tint_color = CRGB16(65535, 65535, 65535);
    layers[active_layers].speed_multiplier = 1.0f;
    layers[active_layers].beat_reactive = true;
    layers[active_layers].phase_offset = 0.0f;
    
    active_layers++;
    return true;
}

void EffectBlender::remove_layer(uint8_t layer_index) {
    if (layer_index >= active_layers) return;
    
    // Shift layers down
    for (uint8_t i = layer_index; i < active_layers - 1; i++) {
        layers[i] = layers[i + 1];
        memcpy(layer_buffers[i], layer_buffers[i + 1], sizeof(CRGB16) * NATIVE_RESOLUTION);
    }
    
    active_layers--;
    
    // Clear the last layer
    if (active_layers < MAX_LAYERS) {
        layers[active_layers].enabled = false;
        layers[active_layers].effect_function = nullptr;
        memset(layer_buffers[active_layers], 0, sizeof(CRGB16) * NATIVE_RESOLUTION);
    }
}

void EffectBlender::clear_all_layers() {
    active_layers = 0;
    for (uint8_t i = 0; i < MAX_LAYERS; i++) {
        layers[i].enabled = false;
        layers[i].effect_function = nullptr;
        memset(layer_buffers[i], 0, sizeof(CRGB16) * NATIVE_RESOLUTION);
    }
}

void EffectBlender::update_audio_analysis() {
    if (!audio_reactive_blending) return;
    
    // Get fresh audio data
    get_smooth_spectrogram();
    get_smooth_chromagram();
    calculate_vu();
    
    // Calculate frequency band energies
    bass_energy = 0;
    for (int i = 0; i < 20; i++) {
        bass_energy += spectrogram_smooth[i];
    }
    bass_energy /= 20.0f;
    
    mid_energy = 0;
    for (int i = 20; i < 50; i++) {
        mid_energy += spectrogram_smooth[i];
    }
    mid_energy /= 30.0f;
    
    high_energy = 0;
    for (int i = 50; i < 96; i++) {
        high_energy += spectrogram_smooth[i];
    }
    high_energy /= 46.0f;
    
    total_energy = (bass_energy + mid_energy + high_energy) / 3.0f;
    
    // Beat detection
    static float last_bass_energy = 0;
    float bass_delta = bass_energy - last_bass_energy;
    beat_detected = (bass_delta > 0.2f && bass_energy > 0.3f);
    last_bass_energy = bass_energy * 0.9f + last_bass_energy * 0.1f;
}

void EffectBlender::render_layers() {
    // Backup original LED buffer
    CRGB16 original_buffer[NATIVE_RESOLUTION];
    memcpy(original_buffer, leds_16, sizeof(CRGB16) * NATIVE_RESOLUTION);
    
    for (uint8_t layer = 0; layer < active_layers; layer++) {
        if (!layers[layer].enabled || layers[layer].effect_function == nullptr) continue;
        
        // Clear LED buffer for this layer
        memset(leds_16, 0, sizeof(CRGB16) * NATIVE_RESOLUTION);
        
        // Render the effect for this layer
        layers[layer].effect_function();
        
        // Apply layer modifications
        for (int i = 0; i < NATIVE_RESOLUTION; i++) {
            CRGB16 pixel = leds_16[i];
            
            // Apply tint color
            pixel.r = (pixel.r * layers[layer].tint_color.r) >> 16;
            pixel.g = (pixel.g * layers[layer].tint_color.g) >> 16;
            pixel.b = (pixel.b * layers[layer].tint_color.b) >> 16;
            
            // Apply audio reactive opacity modulation
            float layer_opacity = layers[layer].opacity;
            if (audio_reactive_blending) {
                float freq_energy = calculate_frequency_band_energy(layers[layer].frequency_band);
                layer_opacity *= (1.0f + freq_energy * layers[layer].audio_sensitivity);
                
                // Beat reactive boost
                if (layers[layer].beat_reactive && beat_detected) {
                    layer_opacity *= 1.5f;
                }
            }
            
            // Clamp opacity
            if (layer_opacity > 1.0f) layer_opacity = 1.0f;
            if (layer_opacity < 0.0f) layer_opacity = 0.0f;
            
            // Store in layer buffer with opacity applied
            layer_buffers[layer][i].r = (uint16_t)(pixel.r * layer_opacity);
            layer_buffers[layer][i].g = (uint16_t)(pixel.g * layer_opacity);
            layer_buffers[layer][i].b = (uint16_t)(pixel.b * layer_opacity);
        }
    }
    
    // Restore original buffer
    memcpy(leds_16, original_buffer, sizeof(CRGB16) * NATIVE_RESOLUTION);
}

void EffectBlender::blend_layers() {
    // Start with black
    memset(blend_buffer, 0, sizeof(CRGB16) * NATIVE_RESOLUTION);
    
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        CRGB16 result = CRGB16(0, 0, 0);
        
        for (uint8_t layer = 0; layer < active_layers; layer++) {
            if (!layers[layer].enabled) continue;
            
            float layer_opacity = layers[layer].opacity;
            
            // Audio reactive opacity modulation
            if (audio_reactive_blending) {
                float freq_energy = calculate_frequency_band_energy(layers[layer].frequency_band);
                layer_opacity *= (1.0f + freq_energy * layers[layer].audio_sensitivity);
                
                if (layers[layer].beat_reactive && beat_detected) {
                    layer_opacity *= 1.3f;
                }
            }
            
            // Clamp opacity
            if (layer_opacity > 1.0f) layer_opacity = 1.0f;
            if (layer_opacity < 0.0f) layer_opacity = 0.0f;
            
            result = apply_blend_mode(result, layer_buffers[layer][i], 
                                    layers[layer].blend_mode, layer_opacity);
        }
        
        blend_buffer[i] = result;
    }
}

void EffectBlender::apply_to_output() {
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        leds_16[i] = blend_buffer[i];
    }
}

void EffectBlender::render() {
    update_audio_analysis();
    render_layers();
    blend_layers();
    apply_to_output();
}

// Blend mode implementations
CRGB16 EffectBlender::blend_additive(CRGB16 base, CRGB16 overlay, float opacity) {
    CRGB16 result;
    result.r = min(65535, base.r + (uint16_t)(overlay.r * opacity));
    result.g = min(65535, base.g + (uint16_t)(overlay.g * opacity));
    result.b = min(65535, base.b + (uint16_t)(overlay.b * opacity));
    return result;
}

CRGB16 EffectBlender::blend_multiply(CRGB16 base, CRGB16 overlay, float opacity) {
    CRGB16 result;
    uint16_t mult_r = (base.r * overlay.r) >> 16;
    uint16_t mult_g = (base.g * overlay.g) >> 16;
    uint16_t mult_b = (base.b * overlay.b) >> 16;
    
    result.r = base.r + (uint16_t)((mult_r - base.r) * opacity);
    result.g = base.g + (uint16_t)((mult_g - base.g) * opacity);
    result.b = base.b + (uint16_t)((mult_b - base.b) * opacity);
    return result;
}

CRGB16 EffectBlender::blend_screen(CRGB16 base, CRGB16 overlay, float opacity) {
    CRGB16 result;
    uint16_t screen_r = 65535 - (((65535 - base.r) * (65535 - overlay.r)) >> 16);
    uint16_t screen_g = 65535 - (((65535 - base.g) * (65535 - overlay.g)) >> 16);
    uint16_t screen_b = 65535 - (((65535 - base.b) * (65535 - overlay.b)) >> 16);
    
    result.r = base.r + (uint16_t)((screen_r - base.r) * opacity);
    result.g = base.g + (uint16_t)((screen_g - base.g) * opacity);
    result.b = base.b + (uint16_t)((screen_b - base.b) * opacity);
    return result;
}

CRGB16 EffectBlender::blend_alpha(CRGB16 base, CRGB16 overlay, float opacity) {
    CRGB16 result;
    result.r = base.r + (uint16_t)((overlay.r - base.r) * opacity);
    result.g = base.g + (uint16_t)((overlay.g - base.g) * opacity);
    result.b = base.b + (uint16_t)((overlay.b - base.b) * opacity);
    return result;
}

CRGB16 EffectBlender::blend_xor(CRGB16 base, CRGB16 overlay, float opacity) {
    CRGB16 result;
    uint16_t xor_r = base.r ^ overlay.r;
    uint16_t xor_g = base.g ^ overlay.g;
    uint16_t xor_b = base.b ^ overlay.b;
    
    result.r = base.r + (uint16_t)((xor_r - base.r) * opacity);
    result.g = base.g + (uint16_t)((xor_g - base.g) * opacity);
    result.b = base.b + (uint16_t)((xor_b - base.b) * opacity);
    return result;
}

CRGB16 EffectBlender::blend_subtract(CRGB16 base, CRGB16 overlay, float opacity) {
    CRGB16 result;
    result.r = max(0, (int32_t)base.r - (int32_t)(overlay.r * opacity));
    result.g = max(0, (int32_t)base.g - (int32_t)(overlay.g * opacity));
    result.b = max(0, (int32_t)base.b - (int32_t)(overlay.b * opacity));
    return result;
}

CRGB16 EffectBlender::blend_difference(CRGB16 base, CRGB16 overlay, float opacity) {
    CRGB16 result;
    uint16_t diff_r = abs((int32_t)base.r - (int32_t)overlay.r);
    uint16_t diff_g = abs((int32_t)base.g - (int32_t)overlay.g);
    uint16_t diff_b = abs((int32_t)base.b - (int32_t)overlay.b);
    
    result.r = base.r + (uint16_t)((diff_r - base.r) * opacity);
    result.g = base.g + (uint16_t)((diff_g - base.g) * opacity);
    result.b = base.b + (uint16_t)((diff_b - base.b) * opacity);
    return result;
}

CRGB16 EffectBlender::blend_lighten(CRGB16 base, CRGB16 overlay, float opacity) {
    CRGB16 result;
    uint16_t light_r = max(base.r, overlay.r);
    uint16_t light_g = max(base.g, overlay.g);
    uint16_t light_b = max(base.b, overlay.b);
    
    result.r = base.r + (uint16_t)((light_r - base.r) * opacity);
    result.g = base.g + (uint16_t)((light_g - base.g) * opacity);
    result.b = base.b + (uint16_t)((light_b - base.b) * opacity);
    return result;
}

CRGB16 EffectBlender::blend_darken(CRGB16 base, CRGB16 overlay, float opacity) {
    CRGB16 result;
    uint16_t dark_r = min(base.r, overlay.r);
    uint16_t dark_g = min(base.g, overlay.g);
    uint16_t dark_b = min(base.b, overlay.b);
    
    result.r = base.r + (uint16_t)((dark_r - base.r) * opacity);
    result.g = base.g + (uint16_t)((dark_g - base.g) * opacity);
    result.b = base.b + (uint16_t)((dark_b - base.b) * opacity);
    return result;
}

CRGB16 EffectBlender::apply_blend_mode(CRGB16 base, CRGB16 overlay, BlendMode mode, float opacity) {
    switch (mode) {
        case BLEND_ADDITIVE:    return blend_additive(base, overlay, opacity);
        case BLEND_MULTIPLY:    return blend_multiply(base, overlay, opacity);
        case BLEND_SCREEN:      return blend_screen(base, overlay, opacity);
        case BLEND_ALPHA:       return blend_alpha(base, overlay, opacity);
        case BLEND_XOR:         return blend_xor(base, overlay, opacity);
        case BLEND_SUBTRACT:    return blend_subtract(base, overlay, opacity);
        case BLEND_DIFFERENCE:  return blend_difference(base, overlay, opacity);
        case BLEND_LIGHTEN:     return blend_lighten(base, overlay, opacity);
        case BLEND_DARKEN:      return blend_darken(base, overlay, opacity);
        default:                return blend_additive(base, overlay, opacity);
    }
}

float EffectBlender::calculate_frequency_band_energy(uint8_t band) {
    if (band >= 12) return 0.0f;
    return chromagram_smooth[band];
}

// Layer configuration functions
void EffectBlender::set_layer_opacity(uint8_t layer_index, float opacity) {
    if (layer_index < active_layers) {
        layers[layer_index].opacity = constrain(opacity, 0.0f, 1.0f);
    }
}

void EffectBlender::set_layer_blend_mode(uint8_t layer_index, BlendMode mode) {
    if (layer_index < active_layers) {
        layers[layer_index].blend_mode = mode;
    }
}

void EffectBlender::set_layer_audio_sensitivity(uint8_t layer_index, float sensitivity) {
    if (layer_index < active_layers) {
        layers[layer_index].audio_sensitivity = constrain(sensitivity, 0.0f, 2.0f);
    }
}

void EffectBlender::set_layer_frequency_band(uint8_t layer_index, uint8_t band) {
    if (layer_index < active_layers && band < 12) {
        layers[layer_index].frequency_band = band;
    }
}

void EffectBlender::set_layer_tint(uint8_t layer_index, CRGB16 tint) {
    if (layer_index < active_layers) {
        layers[layer_index].tint_color = tint;
    }
}

void EffectBlender::set_layer_beat_reactive(uint8_t layer_index, bool reactive) {
    if (layer_index < active_layers) {
        layers[layer_index].beat_reactive = reactive;
    }
}

// Preset combinations
void EffectBlender::load_combo_preset(EffectCombo combo) {
    clear_all_layers();
    
    switch (combo) {
        case COMBO_QUANTUM_AURORA:
            setup_quantum_aurora_combo();
            break;
        case COMBO_GRAVITATIONAL_WAVES:
            setup_gravitational_waves_combo();
            break;
        case COMBO_BIOLUMINESCENT_HOLO:
            setup_bioluminescent_holo_combo();
            break;
        case COMBO_SOLITON_INTERFERENCE:
            setup_soliton_interference_combo();
            break;
        case COMBO_CHAOS_CRYSTALLINE:
            setup_chaos_crystalline_combo();
            break;
        case COMBO_PLASMA_STORM:
            setup_plasma_storm_combo();
            break;
        default:
            break;
    }
}

// Utility functions
uint8_t EffectBlender::get_active_layer_count() const {
    return active_layers;
}

float EffectBlender::get_layer_opacity(uint8_t layer_index) const {
    if (layer_index < active_layers) {
        return layers[layer_index].opacity;
    }
    return 0.0f;
}

BlendMode EffectBlender::get_layer_blend_mode(uint8_t layer_index) const {
    if (layer_index < active_layers) {
        return layers[layer_index].blend_mode;
    }
    return BLEND_ADDITIVE;
}

bool EffectBlender::is_layer_enabled(uint8_t layer_index) const {
    if (layer_index < active_layers) {
        return layers[layer_index].enabled;
    }
    return false;
}