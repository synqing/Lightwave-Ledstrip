/*
 * SpectraSynq God-Tier Goertzel Engine
 * =====================================
 * The definitive musical analysis engine for embedded systems
 * 
 * ARCHITECTURE PILLARS:
 * - Musical Fidelity: Exact semitone frequencies A0-A7
 * - Brutal Efficiency: Compile-time LUTs, zero runtime trig
 * - Cache Dominance: Optimized loop structure for L1 cache
 * - Real-Time Discipline: Predictable performance for embedded systems
 * 
 * DEPRECATION NOTICE
 * ==================
 * This file is part of the legacy monolithic audio pipeline.
 * It will be replaced by the pluggable node architecture.
 * 
 * Replacement: goertzel_node.h
 * Migration: See DEPRECATION_TRACKER.md
 * Target removal: After Phase 3 completion
 * 
 * DO NOT ADD NEW FEATURES TO THIS FILE
 */

#include "audio/goertzel_engine.h"

void GoertzelEngineGodTier::init() {
    // Zero initialization - LUTs are compile-time constants
    for (int i = 0; i < GOERTZEL_BINS; i++) {
        magnitudes[i] = 0.0f;
    }
    
    Serial.println("GoertzelEngineGodTier: Initialized with MUSICAL PRECISION");
    Serial.printf("  - Frequency range: %.1fHz to %.1fHz\n", 
                 MUSIC_FREQUENCIES[0], MUSIC_FREQUENCIES[GOERTZEL_BINS-1]);
    Serial.printf("  - %d exact semitones (A0 to A7)\n", GOERTZEL_BINS);
    Serial.println("  - Zero runtime trig calculations (LUT optimized)");
    Serial.println("  - Cache-optimized loop structure");
}
void GoertzelEngineGodTier::process(int16_t* samples, int num_samples) {
    // THE GOD-TIER ARCHITECTURE:
    // Loop order: for(bin) { for(sample) } - CACHE DOMINANCE!
    
    for (int bin = 0; bin < GOERTZEL_BINS; bin++) {
        // State variables for this frequency bin (hot in L1 cache)
        float q1 = 0.0f;
        float q2 = 0.0f;
        
        // LUT lookup - compile-time constant, zero CPU cost
        const float coeff = GOERTZEL_COEFFS[bin];
        
        // Process ALL samples for this single bin (cache-friendly)
        for (int n = 0; n < num_samples; n++) {
            float sample_value = (float)samples[n];
            
            // Core Goertzel IIR filter
            float q0 = coeff * q1 - q2 + sample_value;
            q2 = q1;
            q1 = q0;
        }
        
        // Magnitude calculation using PRE-COMPUTED LUTs
        // NO MORE RUNTIME TRIG CALCULATIONS!
        float real = q1 - q2 * MAG_COS_TERMS[bin];
        float imag = q2 * MAG_SIN_TERMS[bin];
        
        // Final magnitude with legacy scaling for compatibility
        // Legacy implementation divides magnitude_squared by (block_size/2)
        // With block_size=128, this is equivalent to dividing magnitude by sqrt(64) = 8
        float raw_magnitude = sqrtf(real * real + imag * imag);
        magnitudes[bin] = raw_magnitude / 8.0f;  // Apply legacy scaling factor
        
        // Debug: Check if magnitudes are unusually high
        static int mag_debug_counter = 0;
        if (++mag_debug_counter % (250 * GOERTZEL_BINS) == 0 && bin == 0) {  // Every 2 seconds
            Serial.printf("GOERTZEL MAG DEBUG: raw_mag=%.1f, scaled=%.1f (bin 0)\n", 
                         raw_magnitude, magnitudes[bin]);
        }
    }
}
float* GoertzelEngineGodTier::getMagnitudes() {
    return magnitudes;
}

float GoertzelEngineGodTier::getMagnitude(int bin) {
    if (bin >= 0 && bin < GOERTZEL_BINS) {
        return magnitudes[bin];
    }
    return 0.0f;
}

float GoertzelEngineGodTier::getFrequency(int bin) {
    if (bin >= 0 && bin < GOERTZEL_BINS) {
        return MUSIC_FREQUENCIES[bin];
    }
    return 0.0f;
}

int GoertzelEngineGodTier::getBinCount() {
    return GOERTZEL_BINS;
}

// Debug function to verify musical accuracy
void GoertzelEngineGodTier::printFrequencyMap() {
    Serial.println("\n=== SpectraSynq Musical Frequency Map ===");
    
    const char* note_names[] = {"A", "A#", "B", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#"};
    
    for (int octave = 0; octave < 8; octave++) {
        Serial.printf("Octave %d: ", octave);
        for (int note = 0; note < 12; note++) {
            int bin = octave * 12 + note;
            Serial.printf("%s%.1f ", note_names[note], MUSIC_FREQUENCIES[bin]);
        }
        Serial.println();
    }
    Serial.println("=========================================\n");
}

// Global instance for the audio pipeline
GoertzelEngineGodTier goertzel_god_tier;