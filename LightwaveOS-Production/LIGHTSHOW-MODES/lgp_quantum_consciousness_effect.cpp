#include "lgp_quantum_consciousness_effect.h"
#include "../src/globals.h"
#include "../src/GDFT.h"
#include <math.h>

#define MAX_NEURAL_NODES 12
#define QUANTUM_FIELD_RESOLUTION 160
#define BRAINWAVE_BANDS 5

// Brainwave frequency bands (scaled for visual effect)
enum BrainwaveBand {
    DELTA = 0,    // 0.5-4 Hz (Deep sleep, unconscious)
    THETA = 1,    // 4-8 Hz (Dreams, meditation)
    ALPHA = 2,    // 8-13 Hz (Relaxed awareness)
    BETA = 3,     // 13-30 Hz (Active thinking)
    GAMMA = 4     // 30-100 Hz (Consciousness binding)
};

struct NeuralNode {
    SQ15x16 position;
    SQ15x16 quantum_phase;
    SQ15x16 coherence_strength;
    SQ15x16 entanglement_radius;
    uint8_t dominant_frequency;
    SQ15x16 activation_potential;
    bool firing;
    uint32_t last_fire_time;
    SQ15x16 neurotransmitter_level;
};

struct QuantumField {
    SQ15x16 amplitude[QUANTUM_FIELD_RESOLUTION];
    SQ15x16 phase[QUANTUM_FIELD_RESOLUTION];
    SQ15x16 coherence[QUANTUM_FIELD_RESOLUTION];
};

static NeuralNode neural_nodes[MAX_NEURAL_NODES];
static QuantumField quantum_field;
static SQ15x16 brainwave_amplitudes[BRAINWAVE_BANDS];
static SQ15x16 global_coherence = 0;
static SQ15x16 consciousness_level = 0;
static uint32_t quantum_collapse_timer = 0;
static bool in_superposition = true;

// Quantum microtubule oscillation patterns
static SQ15x16 microtubule_phases[QUANTUM_FIELD_RESOLUTION];
static SQ15x16 tubulin_states[QUANTUM_FIELD_RESOLUTION];

void light_mode_lgp_quantum_consciousness() {
    cache_frame_config();
    
    // Audio integration for consciousness modulation
    get_smooth_spectrogram();
    get_smooth_chromagram();
    calculate_vu();
    
    // Map audio frequencies to brainwave bands
    brainwave_amplitudes[DELTA] = 0;
    brainwave_amplitudes[THETA] = 0;
    brainwave_amplitudes[ALPHA] = 0;
    brainwave_amplitudes[BETA] = 0;
    brainwave_amplitudes[GAMMA] = 0;
    
    // Audio frequency mapping to brainwave bands
    for(int i = 0; i < 10; i++) brainwave_amplitudes[DELTA] += spectrogram_smooth[i];
    for(int i = 10; i < 20; i++) brainwave_amplitudes[THETA] += spectrogram_smooth[i];
    for(int i = 20; i < 35; i++) brainwave_amplitudes[ALPHA] += spectrogram_smooth[i];
    for(int i = 35; i < 60; i++) brainwave_amplitudes[BETA] += spectrogram_smooth[i];
    for(int i = 60; i < 96; i++) brainwave_amplitudes[GAMMA] += spectrogram_smooth[i];
    
    // Normalize brainwave amplitudes
    for(int i = 0; i < BRAINWAVE_BANDS; i++) {
        brainwave_amplitudes[i] = brainwave_amplitudes[i] / SQ15x16(20);
        if(brainwave_amplitudes[i] > 1) brainwave_amplitudes[i] = 1;
    }
    
    // Calculate global coherence from gamma band synchronization
    global_coherence = brainwave_amplitudes[GAMMA] * SQ15x16(0.5) + 
                      brainwave_amplitudes[BETA] * SQ15x16(0.3) +
                      brainwave_amplitudes[ALPHA] * SQ15x16(0.2);
    
    // Consciousness level based on integrated brainwave activity
    consciousness_level = (brainwave_amplitudes[GAMMA] * SQ15x16(2) + 
                          brainwave_amplitudes[BETA] + 
                          brainwave_amplitudes[ALPHA] * SQ15x16(0.5)) / SQ15x16(3.5);
    
    // Beat detection for quantum collapse events
    static float last_total_energy = 0;
    float total_energy = 0;
    for(int i = 0; i < 96; i++) {
        total_energy += spectrogram_smooth[i];
    }
    float energy_delta = total_energy - last_total_energy;
    bool quantum_event = (energy_delta > 0.3f && total_energy > 0.4f);
    last_total_energy = total_energy * 0.85f + last_total_energy * 0.15f;
    
    // Initialize neural nodes if needed
    static bool initialized = false;
    if (!initialized) {
        for (int n = 0; n < MAX_NEURAL_NODES; n++) {
            neural_nodes[n].position = SQ15x16(random16(NATIVE_RESOLUTION));
            neural_nodes[n].quantum_phase = SQ15x16(random16(360)) / 360 * TWO_PI;
            neural_nodes[n].coherence_strength = SQ15x16(0.5);
            neural_nodes[n].entanglement_radius = SQ15x16(20 + random8(30));
            neural_nodes[n].dominant_frequency = random8(BRAINWAVE_BANDS);
            neural_nodes[n].activation_potential = 0;
            neural_nodes[n].firing = false;
            neural_nodes[n].last_fire_time = 0;
            neural_nodes[n].neurotransmitter_level = SQ15x16(0.5);
        }
        initialized = true;
    }
    
    // Update quantum field coherence
    for (int i = 0; i < QUANTUM_FIELD_RESOLUTION; i++) {
        // Microtubule quantum oscillations (8MHz scaled down)
        microtubule_phases[i] += SQ15x16(0.1) * (SQ15x16(1) + consciousness_level);
        
        // Tubulin dimer states (quantum superposition)
        if (in_superposition) {
            tubulin_states[i] = sin_lookup((microtubule_phases[i] * 256).getInteger() & 0xFF) * 
                               global_coherence;
        } else {
            // Collapsed state
            tubulin_states[i] = (tubulin_states[i] > 0) ? SQ15x16(1) : SQ15x16(-1);
        }
        
        // Quantum field amplitude based on neural activity
        quantum_field.amplitude[i] *= SQ15x16(0.92); // Decay
        
        // Phase evolution with consciousness-dependent speed
        quantum_field.phase[i] += SQ15x16(0.02) * (SQ15x16(1) + consciousness_level * SQ15x16(2));
        
        // Coherence decay and regeneration
        quantum_field.coherence[i] *= SQ15x16(0.95);
    }
    
    // Update neural nodes
    uint32_t now = millis();
    for (int n = 0; n < MAX_NEURAL_NODES; n++) {
        NeuralNode& node = neural_nodes[n];
        
        // Accumulate activation potential based on dominant frequency band
        node.activation_potential += brainwave_amplitudes[node.dominant_frequency] * SQ15x16(0.1);
        
        // Neurotransmitter dynamics
        node.neurotransmitter_level += SQ15x16(0.01); // Slow regeneration
        if (node.neurotransmitter_level > 1) node.neurotransmitter_level = 1;
        
        // Check for neural firing (action potential)
        if (node.activation_potential > SQ15x16(0.8) && 
            node.neurotransmitter_level > SQ15x16(0.3) &&
            now - node.last_fire_time > 50) {
            
            node.firing = true;
            node.last_fire_time = now;
            node.activation_potential = 0;
            node.neurotransmitter_level -= SQ15x16(0.5);
            
            // Quantum coherence burst at firing location
            int pos = node.position.getInteger();
            if (pos >= 0 && pos < QUANTUM_FIELD_RESOLUTION) {
                quantum_field.coherence[pos] = SQ15x16(1);
                quantum_field.amplitude[pos] = SQ15x16(1);
                
                // Propagate quantum entanglement
                for (int i = 0; i < QUANTUM_FIELD_RESOLUTION; i++) {
                    float dist = abs(i - pos);
                    if (dist <= node.entanglement_radius) {
                        SQ15x16 entanglement_strength = SQ15x16(1) - (SQ15x16(dist) / node.entanglement_radius);
                        quantum_field.coherence[i] += entanglement_strength * node.coherence_strength;
                        if (quantum_field.coherence[i] > 1) quantum_field.coherence[i] = 1;
                    }
                }
            }
        } else {
            node.firing = false;
        }
        
        // Update quantum phase based on brainwave activity
        node.quantum_phase += brainwave_amplitudes[node.dominant_frequency] * SQ15x16(0.05);
        
        // Neural drift in quantum field
        SQ15x16 drift = sin_lookup((node.quantum_phase * 64).getInteger() & 0xFF) * SQ15x16(0.3);
        node.position += drift;
        if (node.position < 0) node.position += NATIVE_RESOLUTION;
        if (node.position >= NATIVE_RESOLUTION) node.position -= NATIVE_RESOLUTION;
    }
    
    // Quantum decoherence and collapse
    if (quantum_event || (consciousness_level > SQ15x16(0.8) && random8() < 50)) {
        in_superposition = false;
        quantum_collapse_timer = now + 200; // 200ms collapse state
    }
    
    if (!in_superposition && now > quantum_collapse_timer) {
        in_superposition = true; // Return to superposition
    }
    
    // Render the quantum consciousness field
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        // Base quantum field visualization
        SQ15x16 field_value = quantum_field.amplitude[i] * 
                             sin_lookup((quantum_field.phase[i] * 256).getInteger() & 0xFF);
        
        // Add microtubule oscillations
        field_value += tubulin_states[i % QUANTUM_FIELD_RESOLUTION] * SQ15x16(0.3);
        
        // Brainwave interference patterns
        SQ15x16 brainwave_interference = 0;
        for (int b = 0; b < BRAINWAVE_BANDS; b++) {
            float freq = (b + 1) * 0.05f; // Different frequencies for each band
            brainwave_interference += brainwave_amplitudes[b] * 
                                     sin_lookup((SQ15x16(i * freq) * 256).getInteger() & 0xFF);
        }
        field_value += brainwave_interference * SQ15x16(0.5);
        
        // Neural node contributions
        for (int n = 0; n < MAX_NEURAL_NODES; n++) {
            float dist = abs(i - neural_nodes[n].position.getInteger());
            if (dist <= neural_nodes[n].entanglement_radius) {
                SQ15x16 node_influence = SQ15x16(1) - (SQ15x16(dist) / neural_nodes[n].entanglement_radius);
                
                if (neural_nodes[n].firing) {
                    // Bright flash during action potential
                    field_value += node_influence * SQ15x16(2);
                } else {
                    // Background neural glow
                    field_value += node_influence * neural_nodes[n].activation_potential * SQ15x16(0.5);
                }
            }
        }
        
        // Coherence modulation
        field_value = field_value * (SQ15x16(0.5) + quantum_field.coherence[i % QUANTUM_FIELD_RESOLUTION] * SQ15x16(0.5));
        
        // Calculate color based on consciousness state
        uint8_t base_hue;
        if (consciousness_level < SQ15x16(0.2)) {
            base_hue = 170; // Deep blue (unconscious)
        } else if (consciousness_level < SQ15x16(0.4)) {
            base_hue = 140; // Purple (dream state)
        } else if (consciousness_level < SQ15x16(0.6)) {
            base_hue = 75;  // Green (relaxed awareness)
        } else if (consciousness_level < SQ15x16(0.8)) {
            base_hue = 30;  // Orange (active thinking)
        } else {
            base_hue = 280; // Violet (heightened consciousness)
        }
        
        // Add quantum uncertainty to hue
        uint8_t hue = base_hue + (field_value * 30).getInteger();
        
        // Saturation based on coherence
        uint8_t saturation = 150 + (quantum_field.coherence[i % QUANTUM_FIELD_RESOLUTION] * 105).getInteger();
        
        // Brightness from field amplitude
        uint8_t brightness = 30 + (abs(field_value) * 225).getInteger();
        if (brightness > 255) brightness = 255;
        
        // Quantum collapse visual effect
        if (!in_superposition) {
            // Sharp, discrete states during collapse
            brightness = (brightness > 128) ? 255 : 0;
            saturation = 255;
            hue = base_hue; // Remove uncertainty
        }
        
        CRGB16 color;
        if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
            uint8_t palette_idx = (hue + (i * 2)) & 0xFF;
            color = palette_to_crgb16(palette_arr[frame_config.PALETTE], palette_idx, brightness);
        } else {
            color = hsv_to_rgb_fast(hue, saturation, brightness);
        }
        
        // Add quantum entanglement sparkles
        if (quantum_field.coherence[i % QUANTUM_FIELD_RESOLUTION] > SQ15x16(0.8) && random8() < 30) {
            CRGB16 sparkle = hsv_to_rgb_fast(random8(), 100, 255);
            color = add_clipped(color, sparkle);
        }
        
        leds_16[i] = color;
    }
    
    // Neural firing indicators
    for (int n = 0; n < MAX_NEURAL_NODES; n++) {
        if (neural_nodes[n].firing) {
            int pos = neural_nodes[n].position.getInteger();
            if (pos >= 0 && pos < NATIVE_RESOLUTION) {
                // White flash at firing location
                leds_16[pos] = CRGB16(65535, 65535, 65535);
                
                // Synaptic transmission visualization
                for (int offset = -3; offset <= 3; offset++) {
                    int syn_pos = pos + offset;
                    if (syn_pos >= 0 && syn_pos < NATIVE_RESOLUTION) {
                        uint8_t syn_bright = 255 - abs(offset) * 60;
                        CRGB16 syn_color = hsv_to_rgb_fast(60, 255, syn_bright); // Yellow synapses
                        leds_16[syn_pos] = add_clipped(leds_16[syn_pos], syn_color);
                    }
                }
            }
        }
    }
    
    apply_global_brightness();
}