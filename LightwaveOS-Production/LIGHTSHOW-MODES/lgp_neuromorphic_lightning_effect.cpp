#include "lgp_neuromorphic_lightning_effect.h"

// Neuron states
enum NeuronState {
    RESTING,
    DEPOLARIZING,
    FIRING,
    REFRACTORY,
    HYPERPOLARIZED
};

// Neurotransmitter types
enum NeurotransmitterType {
    GLUTAMATE,      // Excitatory
    GABA,           // Inhibitory
    DOPAMINE,       // Reward/motivation
    SEROTONIN,      // Mood/regulation
    ACETYLCHOLINE   // Attention/learning
};

// Neuron structure
struct Neuron {
    SQ15x16 membrane_potential;
    SQ15x16 threshold;
    SQ15x16 refractory_timer;
    NeuronState state;
    uint8_t neurotransmitter_level[5];  // Levels of each transmitter
    uint8_t dendrite_count;
    uint8_t axon_terminals;
    bool long_term_potentiation;  // LTP - strengthened connection
};

// Synaptic connection
struct Synapse {
    uint8_t pre_neuron;
    uint8_t post_neuron;
    SQ15x16 strength;
    NeurotransmitterType type;
    bool active;
};

// Neural network parameters
static const int NEURON_COUNT = 40;
static Neuron neurons[NEURON_COUNT];
static const int MAX_SYNAPSES = 100;
static Synapse synapses[MAX_SYNAPSES];
static SQ15x16 global_inhibition = 0;
static SQ15x16 neural_oscillation_phase = 0;
static uint8_t dominant_brainwave = 0;  // 0=delta, 1=theta, 2=alpha, 3=beta, 4=gamma

// Initialize neural network
static void init_neural_network() {
    // Initialize neurons
    for (int i = 0; i < NEURON_COUNT; i++) {
        neurons[i].membrane_potential = SQ15x16(-70);  // Resting potential in mV
        neurons[i].threshold = SQ15x16(-55);
        neurons[i].refractory_timer = 0;
        neurons[i].state = RESTING;
        neurons[i].dendrite_count = 2 + random8(4);
        neurons[i].axon_terminals = 1 + random8(3);
        neurons[i].long_term_potentiation = false;
        
        // Random neurotransmitter profiles
        for (int nt = 0; nt < 5; nt++) {
            neurons[i].neurotransmitter_level[nt] = random8(100);
        }
    }
    
    // Create random synaptic connections
    for (int i = 0; i < MAX_SYNAPSES; i++) {
        synapses[i].pre_neuron = random8(NEURON_COUNT);
        synapses[i].post_neuron = random8(NEURON_COUNT);
        synapses[i].strength = SQ15x16(random8(100)) / 100;
        synapses[i].type = (NeurotransmitterType)(random8(5));
        synapses[i].active = random8() < 200;  // 78% active
    }
}

// Calculate neuron position on LED strip
static int neuron_to_led_position(int neuron_idx) {
    return (neuron_idx * NATIVE_RESOLUTION) / NEURON_COUNT;
}

// Propagate action potential
static void propagate_action_potential(int neuron_idx) {
    // Find all synapses from this neuron
    for (int i = 0; i < MAX_SYNAPSES; i++) {
        if (synapses[i].active && synapses[i].pre_neuron == neuron_idx) {
            int post_idx = synapses[i].post_neuron;
            
            // Synaptic transmission
            SQ15x16 signal_strength = synapses[i].strength;
            
            // Modulate by neurotransmitter type
            switch(synapses[i].type) {
                case GLUTAMATE:  // Excitatory
                    neurons[post_idx].membrane_potential += signal_strength * 15;
                    break;
                case GABA:  // Inhibitory
                    neurons[post_idx].membrane_potential -= signal_strength * 10;
                    break;
                case DOPAMINE:  // Reward - strengthen synapse
                    synapses[i].strength = min(SQ15x16(1), synapses[i].strength + SQ15x16(0.01));
                    neurons[post_idx].membrane_potential += signal_strength * 8;
                    break;
                case SEROTONIN:  // Mood - affects threshold
                    neurons[post_idx].threshold -= signal_strength * 2;
                    break;
                case ACETYLCHOLINE:  // Learning - LTP
                    if (signal_strength > SQ15x16(0.7)) {
                        neurons[post_idx].long_term_potentiation = true;
                    }
                    neurons[post_idx].membrane_potential += signal_strength * 10;
                    break;
            }
        }
    }
}

void light_mode_lgp_neuromorphic_lightning() {
    cache_frame_config();
    
    // Initialize neural network
    static bool initialized = false;
    if (!initialized) {
        init_neural_network();
        initialized = true;
    }
    
    // Audio analysis for neural modulation
    uint16_t freq_bands[6] = {0};
    uint16_t total_energy = 0;
    
    if (MAX_SAMPLING_FREQ > 0) {
        for (int i = 0; i < 6; i++) {
            freq_bands[i] = sampleAvgMinor[i];
            total_energy += freq_bands[i];
        }
    }
    
    // Map frequency bands to brainwave states
    dominant_brainwave = 0;
    uint16_t max_band = 0;
    for (int i = 0; i < 5; i++) {
        if (freq_bands[i] > max_band) {
            max_band = freq_bands[i];
            dominant_brainwave = i;
        }
    }
    
    // Neural oscillation frequency based on dominant brainwave
    SQ15x16 oscillation_freq = SQ15x16(1 + dominant_brainwave) / 10;
    neural_oscillation_phase += oscillation_freq * SQ15x16(frame_config.SPEED) / 32;
    
    // Global inhibition from high frequencies (cortical inhibition)
    global_inhibition = SQ15x16(freq_bands[5]) / 5000;
    
    // Update neuron states
    for (int i = 0; i < NEURON_COUNT; i++) {
        Neuron& n = neurons[i];
        
        // Apply neural oscillation (brain rhythms)
        SQ15x16 oscillation = sin_lookup((neural_oscillation_phase + i * 10).getInteger() & 0xFF) * 5;
        n.membrane_potential += oscillation;
        
        // Spontaneous activity based on audio energy
        if (random16(10000) < total_energy) {
            n.membrane_potential += SQ15x16(random8(10));
        }
        
        // State machine
        switch(n.state) {
            case RESTING:
                // Slowly return to resting potential
                n.membrane_potential += (SQ15x16(-70) - n.membrane_potential) * SQ15x16(0.05);
                
                // Check for depolarization
                if (n.membrane_potential > n.threshold) {
                    n.state = DEPOLARIZING;
                }
                break;
                
            case DEPOLARIZING:
                // Rapid depolarization (Hodgkin-Huxley dynamics)
                n.membrane_potential += SQ15x16(10);
                if (n.membrane_potential > 30) {
                    n.state = FIRING;
                    propagate_action_potential(i);
                }
                break;
                
            case FIRING:
                // Action potential peak
                n.membrane_potential = SQ15x16(40);
                n.state = REFRACTORY;
                n.refractory_timer = SQ15x16(5);
                
                // LTP increases threshold stability
                if (n.long_term_potentiation) {
                    n.threshold = SQ15x16(-50);  // Easier to fire
                }
                break;
                
            case REFRACTORY:
                // Absolute refractory period
                n.membrane_potential -= SQ15x16(20);
                n.refractory_timer -= SQ15x16(0.5);
                
                if (n.refractory_timer <= 0) {
                    n.state = HYPERPOLARIZED;
                }
                break;
                
            case HYPERPOLARIZED:
                // Below resting potential
                n.membrane_potential += SQ15x16(2);
                if (n.membrane_potential > -75) {
                    n.state = RESTING;
                    n.threshold = SQ15x16(-55);  // Reset threshold
                }
                break;
        }
        
        // Apply global inhibition
        if (n.state == RESTING || n.state == DEPOLARIZING) {
            n.membrane_potential -= global_inhibition * 5;
        }
        
        // Update neurotransmitter levels based on audio
        n.neurotransmitter_level[GLUTAMATE] = freq_bands[0] >> 4;
        n.neurotransmitter_level[GABA] = freq_bands[1] >> 4;
        n.neurotransmitter_level[DOPAMINE] = freq_bands[2] >> 4;
        n.neurotransmitter_level[SEROTONIN] = freq_bands[3] >> 4;
        n.neurotransmitter_level[ACETYLCHOLINE] = freq_bands[4] >> 4;
    }
    
    // Clear strip with background neural glow
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        // Faint background based on brainwave state
        uint8_t bg_brightness = 10 + (dominant_brainwave * 5);
        CRGB16 bg_color;
        
        switch(dominant_brainwave) {
            case 0:  // Delta - deep sleep (deep blue)
                bg_color = CRGB16(0, 0, bg_brightness * 256);
                break;
            case 1:  // Theta - meditation (purple)
                bg_color = CRGB16(bg_brightness * 128, 0, bg_brightness * 256);
                break;
            case 2:  // Alpha - relaxed (green)
                bg_color = CRGB16(0, bg_brightness * 256, bg_brightness * 128);
                break;
            case 3:  // Beta - active (yellow)
                bg_color = CRGB16(bg_brightness * 256, bg_brightness * 200, 0);
                break;
            case 4:  // Gamma - high cognition (white)
                bg_color = CRGB16(bg_brightness * 256, bg_brightness * 256, bg_brightness * 256);
                break;
        }
        
        leds_16[i] = bg_color;
    }
    
    // Render neurons and action potentials
    for (int i = 0; i < NEURON_COUNT; i++) {
        Neuron& n = neurons[i];
        int center_pos = neuron_to_led_position(i);
        
        // Neuron visualization based on state
        CRGB16 neuron_color;
        uint8_t brightness = 0;
        int spread = 1;
        
        switch(n.state) {
            case RESTING:
                // Dim, cool colors
                brightness = 30 + ((n.membrane_potential + 70) * 2).getInteger();
                neuron_color = CRGB16(0, brightness * 100, brightness * 256);
                break;
                
            case DEPOLARIZING:
                // Warming up
                brightness = 100 + ((n.membrane_potential + 55) * 5).getInteger();
                neuron_color = CRGB16(brightness * 200, brightness * 150, brightness * 100);
                spread = 2;
                break;
                
            case FIRING:
                // Bright white flash
                brightness = 255;
                neuron_color = CRGB16(65535, 65535, 65535);
                spread = 5;  // Action potential spreads
                break;
                
            case REFRACTORY:
                // Fading yellow
                brightness = 200 - (n.refractory_timer * 20).getInteger();
                neuron_color = CRGB16(brightness * 256, brightness * 200, 0);
                spread = 3;
                break;
                
            case HYPERPOLARIZED:
                // Deep blue
                brightness = 50;
                neuron_color = CRGB16(0, 0, brightness * 300);
                break;
        }
        
        // LTP neurons have enhanced glow
        if (n.long_term_potentiation) {
            neuron_color.g = min(65535, neuron_color.g + 10000);
        }
        
        // Draw neuron with spread
        for (int s = -spread; s <= spread; s++) {
            int pos = center_pos + s;
            if (pos >= 0 && pos < NATIVE_RESOLUTION) {
                uint8_t falloff = 255 / (abs(s) + 1);
                CRGB16 spread_color = neuron_color;
                spread_color.r = (spread_color.r * falloff) >> 8;
                spread_color.g = (spread_color.g * falloff) >> 8;
                spread_color.b = (spread_color.b * falloff) >> 8;
                
                leds_16[pos] = add_clipped(leds_16[pos], spread_color);
            }
        }
    }
    
    // Render synaptic transmissions
    for (int i = 0; i < MAX_SYNAPSES; i++) {
        if (synapses[i].active && neurons[synapses[i].pre_neuron].state == FIRING) {
            int pre_pos = neuron_to_led_position(synapses[i].pre_neuron);
            int post_pos = neuron_to_led_position(synapses[i].post_neuron);
            
            // Draw synaptic transmission line
            int steps = abs(post_pos - pre_pos);
            if (steps > 0) {
                for (int s = 0; s <= steps; s++) {
                    int pos = pre_pos + ((post_pos - pre_pos) * s) / steps;
                    if (pos >= 0 && pos < NATIVE_RESOLUTION) {
                        // Color based on neurotransmitter
                        CRGB16 synapse_color;
                        uint8_t syn_bright = (synapses[i].strength * 150).getInteger();
                        
                        switch(synapses[i].type) {
                            case GLUTAMATE:
                                synapse_color = CRGB16(syn_bright * 256, 0, 0);  // Red
                                break;
                            case GABA:
                                synapse_color = CRGB16(0, 0, syn_bright * 256);  // Blue
                                break;
                            case DOPAMINE:
                                synapse_color = CRGB16(syn_bright * 256, syn_bright * 128, 0);  // Orange
                                break;
                            case SEROTONIN:
                                synapse_color = CRGB16(syn_bright * 200, 0, syn_bright * 256);  // Purple
                                break;
                            case ACETYLCHOLINE:
                                synapse_color = CRGB16(0, syn_bright * 256, 0);  // Green
                                break;
                        }
                        
                        leds_16[pos] = add_clipped(leds_16[pos], synapse_color);
                    }
                }
            }
        }
    }
    
    // Thought pattern visualization (emergent network activity)
    static SQ15x16 thought_position = 0;
    thought_position += SQ15x16(frame_config.SPEED) / 16;
    
    int thought_center = (thought_position.getInteger() + 
                         sin_lookup((neural_oscillation_phase * 2).getInteger() & 0xFF).getInteger() * 20) 
                         % NATIVE_RESOLUTION;
    
    // Render thought wave
    for (int i = -20; i <= 20; i++) {
        int pos = thought_center + i;
        if (pos >= 0 && pos < NATIVE_RESOLUTION) {
            uint8_t thought_bright = 100 * exp(-abs(i) / 5.0);
            CRGB16 thought_color = hsv_to_rgb_fast(
                (neural_oscillation_phase * 10 + i * 5).getInteger() & 0xFF,
                200,
                thought_bright
            );
            leds_16[pos] = add_clipped(leds_16[pos], thought_color);
        }
    }
    
    apply_global_brightness();
}