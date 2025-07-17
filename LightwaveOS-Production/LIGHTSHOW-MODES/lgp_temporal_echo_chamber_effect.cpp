#include "lgp_temporal_echo_chamber_effect.h"
#include "../src/globals.h"
#include "../src/GDFT.h"
#include <math.h>

#define MAX_TEMPORAL_LAYERS 8
#define ECHO_BUFFER_SIZE 160
#define TIME_CRYSTAL_NODES 6
#define MEMORY_DECAY_MODES 4

// Temporal echo layer structure
struct TemporalLayer {
    CRGB16 buffer[ECHO_BUFFER_SIZE];
    float time_dilation_factor;    // Relativistic time dilation
    float quantum_decay_rate;      // Memory fade rate
    float phase_offset;            // Temporal phase shift
    float causality_coherence;     // How connected to present
    uint16_t write_position;       // Current write head
    uint16_t read_offset;          // Time delay in samples
    bool active;
    float resonance_frequency;     // Temporal oscillation freq
    float entanglement_strength;   // Connection to other layers
};

// Time crystal node for periodic time structure
struct TimeCrystalNode {
    SQ15x16 position;
    SQ15x16 temporal_phase;
    SQ15x16 oscillation_rate;
    float influence_radius;
    float temporal_charge;
    bool is_past_oriented;
    uint8_t quantum_state;
};

// Chronon particle (quantum of time)
struct Chronon {
    SQ15x16 position;
    SQ15x16 velocity;
    float temporal_momentum;
    float age;
    uint8_t timeline_id;
    bool moving_backward;
    CRGB16 color_memory;
};

static TemporalLayer temporal_layers[MAX_TEMPORAL_LAYERS];
static TimeCrystalNode time_crystals[TIME_CRYSTAL_NODES];
static Chronon chronons[32];
static float global_time_flow_rate = 1.0f;
static float temporal_turbulence = 0;
static float causality_violation_level = 0;
static bool temporal_storm_active = false;
static uint32_t temporal_anchor_point = 0;

// Memory decay patterns
enum DecayMode {
    DECAY_EXPONENTIAL,    // Standard exponential decay
    DECAY_QUANTUM,        // Quantum decoherence pattern
    DECAY_OSCILLATING,    // Oscillating memory strength
    DECAY_FRACTAL         // Self-similar decay at multiple scales
};

// Initialize a Lorentz transformation matrix for time dilation
float calculate_lorentz_factor(float velocity) {
    float c = 1.0f; // Speed of light (normalized)
    float v_squared = velocity * velocity;
    if (v_squared >= c * c) v_squared = 0.99f * c * c; // Prevent singularity
    return 1.0f / sqrt(1.0f - v_squared / (c * c));
}

// Quantum memory decay function
float quantum_decay(float age, float coherence_time) {
    // Quantum Zeno effect - frequent observation slows decay
    float observation_rate = 1.0f + temporal_turbulence * 10.0f;
    float effective_age = age / observation_rate;
    
    // Exponential decay with quantum oscillations
    return exp(-effective_age / coherence_time) * 
           (1.0f + 0.3f * cos(effective_age * TWO_PI / coherence_time));
}

void light_mode_lgp_temporal_echo_chamber() {
    cache_frame_config();
    
    // Audio integration for temporal dynamics
    get_smooth_spectrogram();
    get_smooth_chromagram();
    calculate_vu();
    
    // Calculate temporal flow parameters from audio
    float total_energy = 0;
    float spectral_variance = 0;
    float prev_value = 0;
    
    for (int i = 0; i < 96; i++) {
        total_energy += spectrogram_smooth[i];
        float diff = spectrogram_smooth[i] - prev_value;
        spectral_variance += diff * diff;
        prev_value = spectrogram_smooth[i];
    }
    total_energy /= 96.0f;
    spectral_variance = sqrt(spectral_variance / 96.0f);
    
    // Low frequencies control time flow rate
    float low_freq_energy = 0;
    for (int i = 0; i < 20; i++) {
        low_freq_energy += spectrogram_smooth[i];
    }
    low_freq_energy /= 20.0f;
    
    // High frequencies create temporal turbulence
    float high_freq_energy = 0;
    for (int i = 60; i < 96; i++) {
        high_freq_energy += spectrogram_smooth[i];
    }
    high_freq_energy /= 36.0f;
    
    // Update global temporal parameters
    global_time_flow_rate = 0.5f + low_freq_energy * 2.0f;
    temporal_turbulence = high_freq_energy;
    causality_violation_level = spectral_variance * 0.5f;
    
    // Beat detection triggers temporal storms
    static float last_energy = 0;
    float energy_delta = total_energy - last_energy;
    temporal_storm_active = (energy_delta > 0.3f && total_energy > 0.5f);
    last_energy = total_energy * 0.9f + last_energy * 0.1f;
    
    // Initialize temporal layers if needed
    static bool initialized = false;
    if (!initialized) {
        for (int l = 0; l < MAX_TEMPORAL_LAYERS; l++) {
            temporal_layers[l].time_dilation_factor = 1.0f + l * 0.3f;
            temporal_layers[l].quantum_decay_rate = 0.02f + l * 0.01f;
            temporal_layers[l].phase_offset = l * TWO_PI / MAX_TEMPORAL_LAYERS;
            temporal_layers[l].causality_coherence = 1.0f / (1.0f + l * 0.5f);
            temporal_layers[l].write_position = 0;
            temporal_layers[l].read_offset = (l + 1) * 20;
            temporal_layers[l].active = (l < 4); // Start with 4 layers
            temporal_layers[l].resonance_frequency = 0.1f + l * 0.05f;
            temporal_layers[l].entanglement_strength = 0.5f;
            
            // Clear buffer
            for (int i = 0; i < ECHO_BUFFER_SIZE; i++) {
                temporal_layers[l].buffer[i] = CRGB16(0, 0, 0);
            }
        }
        
        // Initialize time crystals
        for (int t = 0; t < TIME_CRYSTAL_NODES; t++) {
            time_crystals[t].position = SQ15x16(NATIVE_RESOLUTION * t / TIME_CRYSTAL_NODES);
            time_crystals[t].temporal_phase = SQ15x16(random16(360)) / 360 * TWO_PI;
            time_crystals[t].oscillation_rate = SQ15x16(0.05f + random8() / 1000.0f);
            time_crystals[t].influence_radius = 30.0f + random8(20);
            time_crystals[t].temporal_charge = (random8(2) == 0) ? 1.0f : -1.0f;
            time_crystals[t].is_past_oriented = (t % 2 == 0);
            time_crystals[t].quantum_state = random8(4);
        }
        
        // Initialize chronons
        for (int c = 0; c < 32; c++) {
            chronons[c].position = SQ15x16(random16(NATIVE_RESOLUTION));
            chronons[c].velocity = SQ15x16(0.5f - random8() / 256.0f);
            chronons[c].temporal_momentum = 1.0f;
            chronons[c].age = 0;
            chronons[c].timeline_id = c % MAX_TEMPORAL_LAYERS;
            chronons[c].moving_backward = (random8(2) == 0);
            chronons[c].color_memory = CRGB16(0, 0, 0);
        }
        
        temporal_anchor_point = millis();
        initialized = true;
    }
    
    // Update time crystal dynamics
    for (int t = 0; t < TIME_CRYSTAL_NODES; t++) {
        time_crystals[t].temporal_phase += time_crystals[t].oscillation_rate * 
                                          SQ15x16(global_time_flow_rate);
        
        // Quantum state transitions
        if (temporal_storm_active && random8() < 50) {
            time_crystals[t].quantum_state = (time_crystals[t].quantum_state + 1) % 4;
            time_crystals[t].temporal_charge *= -1; // Flip temporal polarity
        }
        
        // Audio modulation
        float crystal_freq = 10.0f + t * 15.0f;
        if (crystal_freq < 96) {
            time_crystals[t].influence_radius = 30.0f + spectrogram_smooth[(int)crystal_freq] * 50.0f;
        }
    }
    
    // Update chronon particles
    for (int c = 0; c < 32; c++) {
        // Temporal momentum affected by time crystals
        float net_temporal_force = 0;
        for (int t = 0; t < TIME_CRYSTAL_NODES; t++) {
            float dist = abs(chronons[c].position.getInteger() - 
                           time_crystals[t].position.getInteger());
            if (dist < time_crystals[t].influence_radius) {
                float influence = (1.0f - dist / time_crystals[t].influence_radius) * 
                                 time_crystals[t].temporal_charge;
                net_temporal_force += influence;
            }
        }
        
        // Update velocity with temporal forces
        chronons[c].velocity += SQ15x16(net_temporal_force * 0.01f);
        
        // Apply relativistic effects near light speed
        float speed = abs(chronons[c].velocity.getFloat());
        float lorentz = calculate_lorentz_factor(speed);
        chronons[c].temporal_momentum = lorentz;
        
        // Update position
        if (chronons[c].moving_backward) {
            chronons[c].position -= chronons[c].velocity * SQ15x16(global_time_flow_rate);
        } else {
            chronons[c].position += chronons[c].velocity * SQ15x16(global_time_flow_rate);
        }
        
        // Wrap position
        if (chronons[c].position < 0) chronons[c].position += NATIVE_RESOLUTION;
        if (chronons[c].position >= NATIVE_RESOLUTION) chronons[c].position -= NATIVE_RESOLUTION;
        
        // Age with time dilation
        chronons[c].age += 0.01f / chronons[c].temporal_momentum;
        
        // Timeline hopping during temporal storms
        if (temporal_storm_active && random8() < causality_violation_level * 100) {
            chronons[c].timeline_id = random8(MAX_TEMPORAL_LAYERS);
            chronons[c].moving_backward = !chronons[c].moving_backward;
        }
    }
    
    // Capture current frame into temporal layers
    for (int l = 0; l < MAX_TEMPORAL_LAYERS; l++) {
        if (!temporal_layers[l].active) continue;
        
        // Time-dilated sampling
        int sample_interval = (int)(temporal_layers[l].time_dilation_factor);
        if (frame_config.FRAME_NUMBER % sample_interval == 0) {
            // Audio-modulated time dilation
            float audio_dilation = 1.0f + chromagram_smooth[l % 12] * 0.5f;
            temporal_layers[l].time_dilation_factor = (1.0f + l * 0.3f) * audio_dilation;
            
            // Write current frame to echo buffer with sampling
            for (int i = 0; i < ECHO_BUFFER_SIZE; i++) {
                int source_pos = (i * NATIVE_RESOLUTION) / ECHO_BUFFER_SIZE;
                temporal_layers[l].buffer[temporal_layers[l].write_position] = leds_16[source_pos];
                temporal_layers[l].write_position = (temporal_layers[l].write_position + 1) % ECHO_BUFFER_SIZE;
            }
        }
    }
    
    // Render base reality layer
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        CRGB16 pixel_color = CRGB16(0, 0, 0);
        
        // Accumulate echoes from all temporal layers
        for (int l = 0; l < MAX_TEMPORAL_LAYERS; l++) {
            if (!temporal_layers[l].active) continue;
            
            // Calculate read position with temporal offset
            int buffer_pos = (i * ECHO_BUFFER_SIZE) / NATIVE_RESOLUTION;
            int read_pos = (temporal_layers[l].write_position - temporal_layers[l].read_offset + 
                           ECHO_BUFFER_SIZE) % ECHO_BUFFER_SIZE;
            
            // Add phase-based distortion
            float phase_distortion = sin(temporal_layers[l].phase_offset + 
                                       i * temporal_layers[l].resonance_frequency);
            read_pos = (read_pos + (int)(phase_distortion * 5)) % ECHO_BUFFER_SIZE;
            
            CRGB16 echo_color = temporal_layers[l].buffer[read_pos];
            
            // Apply quantum decay
            float age = temporal_layers[l].read_offset * temporal_layers[l].quantum_decay_rate;
            float decay_factor = quantum_decay(age, 5.0f);
            
            // Causality coherence affects color accuracy
            if (temporal_layers[l].causality_coherence < 0.5f) {
                // Temporal distortion - color shift
                uint16_t hue_shift = (1.0f - temporal_layers[l].causality_coherence) * 180;
                // Simple hue rotation approximation
                uint16_t temp = echo_color.r;
                echo_color.r = echo_color.g;
                echo_color.g = echo_color.b;
                echo_color.b = temp;
            }
            
            // Layer blending with temporal entanglement
            float blend_factor = decay_factor * temporal_layers[l].causality_coherence * 
                               temporal_layers[l].entanglement_strength;
            
            pixel_color = add_clipped(pixel_color, 
                                    scale_color(echo_color, SQ15x16(blend_factor)));
        }
        
        // Time crystal influence creates ripples in spacetime
        for (int t = 0; t < TIME_CRYSTAL_NODES; t++) {
            float dist = abs(i - time_crystals[t].position.getInteger());
            if (dist < time_crystals[t].influence_radius) {
                float influence = 1.0f - dist / time_crystals[t].influence_radius;
                
                // Temporal wave visualization
                float temporal_wave = sin(time_crystals[t].temporal_phase.getFloat() + 
                                        dist * 0.2f) * influence;
                
                uint8_t crystal_hue = 120 + time_crystals[t].quantum_state * 60;
                uint8_t crystal_bright = abs(temporal_wave) * 150;
                
                CRGB16 crystal_color = hsv_to_rgb_fast(crystal_hue, 200, crystal_bright);
                pixel_color = add_clipped(pixel_color, crystal_color);
            }
        }
        
        // Chronon particle trails
        for (int c = 0; c < 32; c++) {
            float dist = abs(i - chronons[c].position.getInteger());
            if (dist < 5) {
                float trail_intensity = (1.0f - dist / 5.0f) * (1.0f - chronons[c].age);
                
                // Color based on temporal momentum (blue shift/red shift)
                uint8_t chronon_hue;
                if (chronons[c].temporal_momentum > 1.5f) {
                    chronon_hue = 160; // Blue shift (time dilation)
                } else if (chronons[c].temporal_momentum < 0.7f) {
                    chronon_hue = 0;   // Red shift (time contraction)
                } else {
                    chronon_hue = 60;  // Normal time (yellow)
                }
                
                CRGB16 chronon_color = hsv_to_rgb_fast(chronon_hue, 255, trail_intensity * 255);
                pixel_color = add_clipped(pixel_color, chronon_color);
            }
        }
        
        // Temporal storm effects
        if (temporal_storm_active) {
            // Causality violations create visual glitches
            if (random8() < causality_violation_level * 50) {
                // Time skip - sample from different temporal location
                int time_skip = random8(40) - 20;
                int skip_pos = (i + time_skip + NATIVE_RESOLUTION) % NATIVE_RESOLUTION;
                pixel_color = blend_alpha(pixel_color, leds_16[skip_pos], SQ15x16(0.5f));
            }
            
            // Temporal lightning
            float storm_wave = sin(i * 0.1f + millis() * 0.01f) * temporal_turbulence;
            if (storm_wave > 0.7f) {
                CRGB16 lightning = hsv_to_rgb_fast(280, 150, storm_wave * 255);
                pixel_color = add_clipped(pixel_color, lightning);
            }
        }
        
        // Apply causality gradient (past darker, future brighter)
        float time_gradient = (float)i / NATIVE_RESOLUTION;
        float brightness_mod = 0.7f + time_gradient * 0.6f;
        pixel_color = scale_color(pixel_color, SQ15x16(brightness_mod));
        
        leds_16[i] = pixel_color;
    }
    
    // Post-processing: Temporal antialiasing
    static CRGB16 temporal_buffer[NATIVE_RESOLUTION];
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        // Blend with previous frame for smooth time flow
        leds_16[i] = blend_alpha(temporal_buffer[i], leds_16[i], 
                               SQ15x16(0.3f + temporal_turbulence * 0.5f));
        temporal_buffer[i] = leds_16[i];
    }
    
    // Add temporal anchor points (stable points in spacetime)
    if ((millis() - temporal_anchor_point) % 1000 < 50) {
        for (int i = 0; i < NATIVE_RESOLUTION; i += 20) {
            leds_16[i] = add_clipped(leds_16[i], CRGB16(10000, 10000, 10000));
        }
    }
    
    apply_global_brightness();
}