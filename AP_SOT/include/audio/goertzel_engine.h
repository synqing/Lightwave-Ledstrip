/*
 * SpectraSynq God-Tier Goertzel Engine Header
 * ==========================================
 * The definitive musical analysis engine for embedded systems
 * 
 * ARCHITECTURE PILLARS:
 * - Musical Fidelity: Exact semitone frequencies A0-A7
 * - Brutal Efficiency: Compile-time LUTs, zero runtime trig
 * - Cache Dominance: Optimized loop structure for L1 cache
 * - Real-Time Discipline: Predictable performance for embedded systems
 */

#ifndef GOERTZEL_ENGINE_H
#define GOERTZEL_ENGINE_H

#include "audio/goertzel_luts.h"
#include <Arduino.h>

class GoertzelEngineGodTier {
private:
    float magnitudes[GOERTZEL_BINS];
    
public:
    void init();
    void process(int16_t* samples, int num_samples);
    float* getMagnitudes();
    float getMagnitude(int bin);
    float getFrequency(int bin);
    int getBinCount();
    void printFrequencyMap();
};

// Global instance for the audio pipeline
extern GoertzelEngineGodTier goertzel_god_tier;

#endif // GOERTZEL_ENGINE_H