#include "lgp_quantum_entanglement_effect.h"

// Quantum states
enum QuantumState {
    SUPERPOSITION,   // |ψ⟩ = α|0⟩ + β|1⟩
    ENTANGLED,      // |Φ+⟩ = |00⟩ + |11⟩
    MEASURED_UP,    // |0⟩
    MEASURED_DOWN,  // |1⟩
    DECOHERED       // Mixed state
};

// Bell states for maximum entanglement
enum BellState {
    PHI_PLUS,    // |Φ+⟩ = (|00⟩ + |11⟩)/√2
    PHI_MINUS,   // |Φ-⟩ = (|00⟩ - |11⟩)/√2
    PSI_PLUS,    // |Ψ+⟩ = (|01⟩ + |10⟩)/√2
    PSI_MINUS    // |Ψ-⟩ = (|01⟩ - |10⟩)/√2
};

// Quantum particle (qubit)
struct Qubit {
    SQ15x16 position;
    SQ15x16 amplitude_zero;     // α coefficient
    SQ15x16 amplitude_one;      // β coefficient
    SQ15x16 phase;             // Quantum phase
    QuantumState state;
    int entangled_partner;      // Index of entangled particle (-1 if none)
    BellState bell_state;
    SQ15x16 decoherence_rate;
    bool teleporting;
};

// Quantum channel for teleportation
struct QuantumChannel {
    int sender_pos;
    int receiver_pos;
    SQ15x16 fidelity;
    bool active;
};

static const int MAX_QUBITS = 20;
static Qubit qubits[MAX_QUBITS];
static QuantumChannel teleport_channel;
static SQ15x16 global_phase = 0;
static SQ15x16 measurement_probability = SQ15x16(0.1);
static uint32_t last_entanglement_time = 0;

// Create entangled pair
static void create_entangled_pair(int idx1, int idx2) {
    if (idx1 >= 0 && idx1 < MAX_QUBITS && idx2 >= 0 && idx2 < MAX_QUBITS) {
        // Choose random Bell state
        BellState bell = (BellState)(random8() & 0x03);
        
        qubits[idx1].state = ENTANGLED;
        qubits[idx2].state = ENTANGLED;
        qubits[idx1].entangled_partner = idx2;
        qubits[idx2].entangled_partner = idx1;
        qubits[idx1].bell_state = bell;
        qubits[idx2].bell_state = bell;
        
        // Set appropriate amplitudes for Bell state
        switch(bell) {
            case PHI_PLUS:
            case PHI_MINUS:
                qubits[idx1].amplitude_zero = SQ15x16(0.707);  // 1/√2
                qubits[idx1].amplitude_one = SQ15x16(0.707);
                qubits[idx2].amplitude_zero = SQ15x16(0.707);
                qubits[idx2].amplitude_one = SQ15x16(0.707);
                break;
            case PSI_PLUS:
            case PSI_MINUS:
                qubits[idx1].amplitude_zero = SQ15x16(0.707);
                qubits[idx1].amplitude_one = SQ15x16(0.707);
                qubits[idx2].amplitude_zero = SQ15x16(0.707);
                qubits[idx2].amplitude_one = SQ15x16(0.707);
                break;
        }
    }
}

// Measure qubit (wave function collapse)
static void measure_qubit(int idx) {
    if (qubits[idx].state == SUPERPOSITION || qubits[idx].state == ENTANGLED) {
        // Collapse based on probability amplitudes
        SQ15x16 prob_zero = qubits[idx].amplitude_zero * qubits[idx].amplitude_zero;
        
        if (SQ15x16(random8()) / 255 < prob_zero) {
            qubits[idx].state = MEASURED_UP;
            qubits[idx].amplitude_zero = 1;
            qubits[idx].amplitude_one = 0;
        } else {
            qubits[idx].state = MEASURED_DOWN;
            qubits[idx].amplitude_zero = 0;
            qubits[idx].amplitude_one = 1;
        }
        
        // Instant correlation for entangled partner
        int partner = qubits[idx].entangled_partner;
        if (partner >= 0 && partner < MAX_QUBITS && qubits[partner].state == ENTANGLED) {
            // Spooky action at a distance!
            switch(qubits[idx].bell_state) {
                case PHI_PLUS:
                    // Same state as measured particle
                    qubits[partner].state = qubits[idx].state;
                    qubits[partner].amplitude_zero = qubits[idx].amplitude_zero;
                    qubits[partner].amplitude_one = qubits[idx].amplitude_one;
                    break;
                case PHI_MINUS:
                    // Same computational state, opposite phase
                    qubits[partner].state = qubits[idx].state;
                    qubits[partner].amplitude_zero = qubits[idx].amplitude_zero;
                    qubits[partner].amplitude_one = qubits[idx].amplitude_one;
                    qubits[partner].phase += SQ15x16(128);  // π phase
                    break;
                case PSI_PLUS:
                case PSI_MINUS:
                    // Opposite state
                    qubits[partner].state = (qubits[idx].state == MEASURED_UP) ? MEASURED_DOWN : MEASURED_UP;
                    qubits[partner].amplitude_zero = qubits[idx].amplitude_one;
                    qubits[partner].amplitude_one = qubits[idx].amplitude_zero;
                    break;
            }
        }
    }
}

void light_mode_lgp_quantum_entanglement() {
    cache_frame_config();
    
    // Initialize qubits
    static bool initialized = false;
    if (!initialized) {
        for (int i = 0; i < MAX_QUBITS; i++) {
            qubits[i].position = (i * NATIVE_RESOLUTION) / MAX_QUBITS;
            qubits[i].amplitude_zero = SQ15x16(0.707);
            qubits[i].amplitude_one = SQ15x16(0.707);
            qubits[i].phase = random8();
            qubits[i].state = SUPERPOSITION;
            qubits[i].entangled_partner = -1;
            qubits[i].decoherence_rate = SQ15x16(0.01);
            qubits[i].teleporting = false;
        }
        teleport_channel.active = false;
        initialized = true;
    }
    
    // Audio analysis for quantum dynamics
    uint16_t freq_bands[6] = {0};
    uint16_t total_energy = 0;
    bool beat_detected = false;
    
    if (MAX_SAMPLING_FREQ > 0) {
        for (int i = 0; i < 6; i++) {
            freq_bands[i] = sampleAvgMinor[i];
            total_energy += freq_bands[i];
        }
        
        // Beat detection for measurements
        static uint16_t prev_bass = 0;
        if (freq_bands[0] > prev_bass * 1.5 && freq_bands[0] > 2000) {
            beat_detected = true;
        }
        prev_bass = freq_bands[0];
    }
    
    // Update measurement probability based on audio
    measurement_probability = SQ15x16(freq_bands[2]) / 10000 + SQ15x16(0.05);
    
    // Decoherence increases with high frequency noise
    SQ15x16 environmental_noise = SQ15x16(freq_bands[5]) / 5000;
    
    // Global phase evolution
    global_phase += SQ15x16(frame_config.SPEED) / 32;
    
    // Create entangled pairs periodically or on beats
    uint32_t current_time = millis();
    if (beat_detected || current_time - last_entanglement_time > 2000) {
        last_entanglement_time = current_time;
        
        // Find unpaired qubits
        for (int i = 0; i < MAX_QUBITS - 1; i++) {
            if (qubits[i].entangled_partner == -1 && qubits[i].state != DECOHERED) {
                for (int j = i + 1; j < MAX_QUBITS; j++) {
                    if (qubits[j].entangled_partner == -1 && qubits[j].state != DECOHERED) {
                        create_entangled_pair(i, j);
                        break;
                    }
                }
            }
        }
    }
    
    // Quantum teleportation on strong beats
    if (beat_detected && freq_bands[0] > 4000 && !teleport_channel.active) {
        // Select random sender and receiver
        int sender = random8(MAX_QUBITS);
        int receiver = random8(MAX_QUBITS);
        if (sender != receiver) {
            teleport_channel.sender_pos = qubits[sender].position.getInteger();
            teleport_channel.receiver_pos = qubits[receiver].position.getInteger();
            teleport_channel.fidelity = SQ15x16(0.9) - environmental_noise;
            teleport_channel.active = true;
            qubits[sender].teleporting = true;
            qubits[receiver].teleporting = true;
        }
    }
    
    // Update qubit states
    for (int i = 0; i < MAX_QUBITS; i++) {
        Qubit& q = qubits[i];
        
        // Phase evolution
        q.phase += SQ15x16(frame_config.SPEED) / 64 + SQ15x16(i) / 10;
        
        // Position drift for visual interest
        SQ15x16 drift = sin_lookup((global_phase + i * 20).getInteger() & 0xFF) * SQ15x16(freq_bands[3]) / 2000;
        q.position += drift;
        if (q.position < 0) q.position = 0;
        if (q.position >= NATIVE_RESOLUTION) q.position = NATIVE_RESOLUTION - 1;
        
        // Apply decoherence
        if (q.state == ENTANGLED || q.state == SUPERPOSITION) {
            q.decoherence_rate += environmental_noise * SQ15x16(0.001);
            
            if (random8() < (q.decoherence_rate * 255).getInteger()) {
                q.state = DECOHERED;
                if (q.entangled_partner >= 0) {
                    qubits[q.entangled_partner].state = DECOHERED;
                }
            }
        }
        
        // Random measurements based on audio
        if (random8() < (measurement_probability * 255).getInteger()) {
            measure_qubit(i);
        }
        
        // Reset decohered qubits occasionally
        if (q.state == DECOHERED && random8() < 5) {
            q.state = SUPERPOSITION;
            q.amplitude_zero = SQ15x16(0.707);
            q.amplitude_one = SQ15x16(0.707);
            q.entangled_partner = -1;
            q.decoherence_rate = SQ15x16(0.01);
        }
    }
    
    // Update teleportation channel
    if (teleport_channel.active) {
        teleport_channel.fidelity -= SQ15x16(0.02);
        if (teleport_channel.fidelity <= 0) {
            teleport_channel.active = false;
            // Clear teleporting flags
            for (int i = 0; i < MAX_QUBITS; i++) {
                qubits[i].teleporting = false;
            }
        }
    }
    
    // Clear strip with quantum vacuum fluctuations
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        // Zero-point energy visualization
        uint8_t vacuum_fluct = 5 + random8(10);
        leds_16[i] = CRGB16(vacuum_fluct * 50, vacuum_fluct * 50, vacuum_fluct * 100);
    }
    
    // Render qubits
    for (int i = 0; i < MAX_QUBITS; i++) {
        Qubit& q = qubits[i];
        int pos = q.position.getInteger();
        
        // Calculate probability cloud
        SQ15x16 prob_zero = q.amplitude_zero * q.amplitude_zero;
        SQ15x16 prob_one = q.amplitude_one * q.amplitude_one;
        
        // Render based on state
        CRGB16 qubit_color;
        int spread = 3;
        
        switch(q.state) {
            case SUPERPOSITION:
                // Purple cloud showing uncertainty
                for (int s = -spread; s <= spread; s++) {
                    int led_pos = pos + s;
                    if (led_pos >= 0 && led_pos < NATIVE_RESOLUTION) {
                        uint8_t cloud_bright = 150 * exp(-abs(s) / 2.0);
                        
                        // Mix of |0⟩ (blue) and |1⟩ (red) based on amplitudes
                        uint16_t red = (prob_one * cloud_bright * 256).getInteger();
                        uint16_t blue = (prob_zero * cloud_bright * 256).getInteger();
                        uint16_t green = cloud_bright * 50;  // Small amount for visibility
                        
                        leds_16[led_pos] = add_clipped(leds_16[led_pos], CRGB16(red, green, blue));
                    }
                }
                break;
                
            case ENTANGLED:
                // Entangled particles show correlated oscillations
                {
                    SQ15x16 entangle_phase = q.phase + global_phase;
                    uint8_t pulse = (sin_lookup(entangle_phase.getInteger() & 0xFF) * 100 + 155).getInteger();
                    
                    // Color based on Bell state
                    switch(q.bell_state) {
                        case PHI_PLUS:
                            qubit_color = CRGB16(0, pulse * 256, pulse * 200);  // Cyan
                            break;
                        case PHI_MINUS:
                            qubit_color = CRGB16(pulse * 256, 0, pulse * 200);  // Magenta
                            break;
                        case PSI_PLUS:
                            qubit_color = CRGB16(pulse * 200, pulse * 256, 0);  // Yellow
                            break;
                        case PSI_MINUS:
                            qubit_color = CRGB16(pulse * 256, pulse * 128, 0);  // Orange
                            break;
                    }
                    
                    // Draw with entanglement spread
                    for (int s = -spread*2; s <= spread*2; s++) {
                        int led_pos = pos + s;
                        if (led_pos >= 0 && led_pos < NATIVE_RESOLUTION) {
                            CRGB16 spread_color = qubit_color;
                            uint8_t falloff = 255 / (abs(s) + 1);
                            spread_color.r = (spread_color.r * falloff) >> 8;
                            spread_color.g = (spread_color.g * falloff) >> 8;
                            spread_color.b = (spread_color.b * falloff) >> 8;
                            
                            leds_16[led_pos] = add_clipped(leds_16[led_pos], spread_color);
                        }
                    }
                }
                break;
                
            case MEASURED_UP:
                // Definite |0⟩ state - blue
                qubit_color = CRGB16(0, 0, 50000);
                leds_16[pos] = add_clipped(leds_16[pos], qubit_color);
                break;
                
            case MEASURED_DOWN:
                // Definite |1⟩ state - red
                qubit_color = CRGB16(50000, 0, 0);
                leds_16[pos] = add_clipped(leds_16[pos], qubit_color);
                break;
                
            case DECOHERED:
                // Lost quantum properties - dim white
                qubit_color = CRGB16(5000, 5000, 5000);
                leds_16[pos] = add_clipped(leds_16[pos], qubit_color);
                break;
        }
        
        // Teleportation effect
        if (q.teleporting && teleport_channel.active) {
            uint8_t teleport_bright = (teleport_channel.fidelity * 255).getInteger();
            CRGB16 teleport_color = CRGB16(teleport_bright * 100, teleport_bright * 256, teleport_bright * 200);
            leds_16[pos] = add_clipped(leds_16[pos], teleport_color);
        }
    }
    
    // Draw entanglement connections
    for (int i = 0; i < MAX_QUBITS; i++) {
        if (qubits[i].state == ENTANGLED && qubits[i].entangled_partner > i) {
            int partner = qubits[i].entangled_partner;
            int pos1 = qubits[i].position.getInteger();
            int pos2 = qubits[partner].position.getInteger();
            
            // Draw quantum correlation line
            int steps = abs(pos2 - pos1);
            if (steps > 0) {
                for (int s = 0; s <= steps; s++) {
                    int led_pos = pos1 + ((pos2 - pos1) * s) / steps;
                    if (led_pos >= 0 && led_pos < NATIVE_RESOLUTION) {
                        // Pulsing connection
                        SQ15x16 conn_phase = global_phase * 3 + SQ15x16(s) / 10;
                        uint8_t conn_bright = (sin_lookup(conn_phase.getInteger() & 0xFF) * 30 + 50).getInteger();
                        
                        CRGB16 conn_color = CRGB16(conn_bright * 100, 0, conn_bright * 200);
                        leds_16[led_pos] = add_clipped(leds_16[led_pos], conn_color);
                    }
                }
            }
        }
    }
    
    // Teleportation channel visualization
    if (teleport_channel.active) {
        int steps = abs(teleport_channel.receiver_pos - teleport_channel.sender_pos);
        for (int s = 0; s <= steps; s++) {
            int led_pos = teleport_channel.sender_pos + 
                         ((teleport_channel.receiver_pos - teleport_channel.sender_pos) * s) / steps;
            
            if (led_pos >= 0 && led_pos < NATIVE_RESOLUTION) {
                // Quantum information transfer
                uint8_t transfer_bright = (teleport_channel.fidelity * 200 * 
                                          sin_lookup((s * 20 + millis() / 5) & 0xFF) / 256).getInteger();
                
                CRGB16 transfer_color = CRGB16(0, transfer_bright * 256, transfer_bright * 300);
                leds_16[led_pos] = add_clipped(leds_16[led_pos], transfer_color);
            }
        }
    }
    
    apply_global_brightness();
}