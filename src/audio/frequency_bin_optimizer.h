#pragma once

#include <Arduino.h>
#include "audio_frame_constants.h"

/**
 * Simple frequency bin optimizer for VP_DECODER
 * Generates synthetic frequency data from bass/mid/high values
 */
class FrequencyBinOptimizer {
public:
    FrequencyBinOptimizer() = default;
    
    /**
     * Synthesize frequency bins from intensity values
     */
    void synthesizeFromIntensities(float* bins, float bass, float mid, float high, unsigned long time) {
        if (!bins) return;
        
        // Simple frequency distribution
        // Bass: bins 0-31 (low frequencies)
        // Mid: bins 32-63 (mid frequencies)  
        // High: bins 64-95 (high frequencies)
        
        // Add some variation based on time
        float variation = sin(time * 0.001f) * 0.2f + 0.8f;
        
        // Fill bass frequencies
        for (int i = 0; i < 32; i++) {
            float position = (float)i / 32.0f;
            float envelope = 1.0f - position * 0.5f;  // Decay envelope
            bins[i] = bass * envelope * variation;
            
            // Add some noise
            bins[i] += (random(100) / 1000.0f) * bass;
        }
        
        // Fill mid frequencies
        for (int i = 32; i < 64; i++) {
            float position = (float)(i - 32) / 32.0f;
            float envelope = sin(position * PI);  // Bell curve
            bins[i] = mid * envelope * variation;
            
            // Add some noise
            bins[i] += (random(100) / 1000.0f) * mid;
        }
        
        // Fill high frequencies
        for (int i = 64; i < FFT_BIN_COUNT; i++) {
            float position = (float)(i - 64) / (FFT_BIN_COUNT - 64);
            float envelope = exp(-position * 3.0f);  // Exponential decay
            bins[i] = high * envelope * variation;
            
            // Add some noise
            bins[i] += (random(100) / 1000.0f) * high;
        }
        
        // Ensure all values are in valid range [0, 1]
        for (int i = 0; i < FFT_BIN_COUNT; i++) {
            bins[i] = constrain(bins[i], 0.0f, 1.0f);
        }
    }
};

// Global instance
extern FrequencyBinOptimizer g_freq_bin_optimizer;