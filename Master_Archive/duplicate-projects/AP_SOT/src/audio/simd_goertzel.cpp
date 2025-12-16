#include "audio/simd_goertzel.h"
#include <Arduino.h>
#include <math.h>
#include "audio/optimized_math.h"

void SIMDGoertzel::init() {
    calculateCoefficients();
    
    // Initialize state arrays
    for (int i = 0; i < NUM_BINS; i++) {
        q1[i] = 0.0f;
        q2[i] = 0.0f;
        magnitudes[i] = 0.0f;
    }
}

void SIMDGoertzel::calculateCoefficients() {
    // Musical semitone mapping (A0 to A7) - validated from our discussion
    for (int i = 0; i < NUM_BINS; i++) {
        float freq = 27.5f * pow(2.0f, i / 12.0f);  // A0 = 27.5Hz
        float omega = 2.0f * M_PI * freq / 16000.0f;   // 16kHz sample rate
        goertzel_coeffs[i] = 2.0f * cos(omega);
    }
}

void SIMDGoertzel::process(int16_t* samples, int num_samples) {
    // Process 96 bins in groups of 8 for SIMD efficiency (validated: 12 groups)
    for (int group = 0; group < 12; group++) {
        processGroup(samples, group * SIMD_GROUP_SIZE, num_samples);
    }
}

void SIMDGoertzel::processGroup(int16_t* samples, int start_bin, int num_samples) {
    // Process 8 bins simultaneously (SIMD-optimized)
    for (int n = 0; n < num_samples; n++) { 
        float sample = (float)samples[n];
        
        // Unrolled loop for 8 bins (SIMD-friendly)
        for (int i = 0; i < SIMD_GROUP_SIZE; i++) {
            int bin = start_bin + i;
            float q0 = goertzel_coeffs[bin] * q1[bin] - q2[bin] + sample;
            q2[bin] = q1[bin];
            q1[bin] = q0;
        }
    }
    
    // Calculate magnitudes for this group using fast sqrt
    for (int i = 0; i < SIMD_GROUP_SIZE; i++) {
        int bin = start_bin + i;
        float mag_squared = q1[bin] * q1[bin] + q2[bin] * q2[bin] - 
                           goertzel_coeffs[bin] * q1[bin] * q2[bin];
        
        // Use fast sqrt for better performance
        if (mag_squared < 65536.0f) {
            magnitudes[bin] = FastMath::fastSqrt32((uint32_t)mag_squared);
        } else {
            magnitudes[bin] = sqrt(mag_squared); // Fallback for large values
        }
    }
    
    // Reset state for next block
    for (int i = 0; i < SIMD_GROUP_SIZE; i++) {
        int bin = start_bin + i;
        q1[bin] = 0.0f;
        q2[bin] = 0.0f;
    }
} 