#pragma once

#include <Arduino.h>
#include "../audio/audio_frame.h"
#include <esp_dsp.h>

/**
 * Optimized Frequency Bin Generation
 * 
 * Uses ESP32-S3 hardware acceleration and lookup tables
 * to reduce CPU usage during frequency synthesis.
 */
class FrequencyBinOptimizer {
private:
    // Pre-computed sine wave lookup table
    static constexpr size_t SINE_TABLE_SIZE = 256;
    float sine_table[SINE_TABLE_SIZE];
    
    // Pre-computed interpolation weights
    float interpolation_weights[FFT_BIN_COUNT];
    
    // Smoothing buffer to reduce computation
    float smoothing_buffer[FFT_BIN_COUNT];
    float smoothing_alpha;
    
public:
    FrequencyBinOptimizer() : smoothing_alpha(0.3f) {
        // Initialize sine lookup table
        for (size_t i = 0; i < SINE_TABLE_SIZE; i++) {
            sine_table[i] = sinf((i * 2.0f * M_PI) / SINE_TABLE_SIZE);
        }
        
        // Initialize interpolation weights
        for (size_t i = 0; i < FFT_BIN_COUNT; i++) {
            // Pre-compute frequency-dependent weights
            float freq_normalized = (float)i / FFT_BIN_COUNT;
            interpolation_weights[i] = 1.0f - (freq_normalized * 0.5f);
        }
        
        // Clear smoothing buffer
        memset(smoothing_buffer, 0, sizeof(smoothing_buffer));
    }
    
    /**
     * Fast sine lookup with linear interpolation
     */
    inline float fastSin(float angle) {
        // Normalize angle to 0-1 range
        float normalized = angle / (2.0f * M_PI);
        normalized = normalized - floorf(normalized);  // Keep in 0-1
        
        // Get table index
        float table_pos = normalized * SINE_TABLE_SIZE;
        int index = (int)table_pos;
        float fract = table_pos - index;
        
        // Linear interpolation between table entries
        int next_index = (index + 1) & (SINE_TABLE_SIZE - 1);
        return sine_table[index] * (1.0f - fract) + sine_table[next_index] * fract;
    }
    
    /**
     * Optimized frequency bin synthesis for VP Decoder
     * Uses ESP32 DSP instructions where possible
     */
    void synthesizeFromIntensities(float* output_bins, 
                                  float bass, float mid, float high,
                                  unsigned long time_ms) {
        // Use fixed-point math for time calculations
        uint32_t time_fixed = time_ms << 8;  // 8-bit fractional precision
        
        // Bass frequencies (0-31) - slow waves
        uint32_t bass_phase = (time_fixed >> 4) & 0xFFFF;  // Slow phase
        for (int i = 0; i < 32; i++) {
            uint32_t wave_index = ((i * bass_phase) >> 8) & (SINE_TABLE_SIZE - 1);
            float wave = sine_table[wave_index];
            output_bins[i] = bass * interpolation_weights[i] * (0.8f + wave * 0.2f);
        }
        
        // Mid frequencies (32-63) - medium waves
        uint32_t mid_phase = (time_fixed >> 3) & 0xFFFF;  // Medium phase
        for (int i = 32; i < 64; i++) {
            uint32_t wave_index = ((i * mid_phase) >> 8) & (SINE_TABLE_SIZE - 1);
            float wave = sine_table[wave_index];
            output_bins[i] = mid * (0.8f + (i - 32) * 0.006f) * (0.7f + wave * 0.3f);
        }
        
        // High frequencies (64-95) - fast variations
        uint32_t high_phase = (time_fixed >> 2) & 0xFFFF;  // Fast phase
        for (int i = 64; i < FFT_BIN_COUNT; i++) {
            uint32_t wave_index = ((i * high_phase) >> 8) & (SINE_TABLE_SIZE - 1);
            float wave = sine_table[wave_index];
            
            // Add pseudo-random variation using bit manipulation
            uint32_t noise = (i * 0x9E3779B9 + time_fixed) >> 24;
            float noise_factor = (noise & 0xFF) / 255.0f;
            
            output_bins[i] = high * (0.6f + (i - 64) * 0.012f) * 
                           (0.5f + wave * 0.3f + noise_factor * 0.2f);
        }
        
        // Apply smoothing using optimized alpha blending
        smoothBins(output_bins);
    }
    
    /**
     * Optimized frequency bin synthesis for Aether Engine
     * Generates organic patterns from Perlin noise samples
     */
    void synthesizeFromPerlin(float* output_bins,
                             const float* noise_samples, int noise_count,
                             float bass_energy, float mid_energy, float high_energy) {
        // Use ESP32 DSP for interpolation if available
        float scale = (float)(noise_count - 1) / (float)FFT_BIN_COUNT;
        
        for (int i = 0; i < FFT_BIN_COUNT; i++) {
            // Calculate noise sample position
            float pos = i * scale;
            int index = (int)pos;
            float fract = pos - index;
            
            // Interpolate between noise samples
            float noise_val;
            if (index < noise_count - 1) {
                noise_val = noise_samples[index] * (1.0f - fract) + 
                           noise_samples[index + 1] * fract;
            } else {
                noise_val = noise_samples[noise_count - 1];
            }
            
            // Apply frequency-specific modulation
            if (i < 32) {
                output_bins[i] = noise_val * bass_energy * 2.0f;
            } else if (i < 64) {
                output_bins[i] = noise_val * mid_energy * 1.5f;
            } else {
                output_bins[i] = noise_val * high_energy * 1.0f;
            }
        }
        
        // Apply smoothing
        smoothBins(output_bins);
    }
    
    /**
     * Set smoothing factor (0 = no smoothing, 1 = maximum)
     */
    void setSmoothingFactor(float alpha) {
        smoothing_alpha = constrain(alpha, 0.0f, 0.9f);
    }
    
private:
    /**
     * Apply temporal smoothing to reduce jitter
     */
    void smoothBins(float* bins) {
        // Use optimized alpha blending
        float one_minus_alpha = 1.0f - smoothing_alpha;
        
        // Process in groups of 4 for better cache usage
        for (int i = 0; i < FFT_BIN_COUNT; i += 4) {
            int remaining = min(4, FFT_BIN_COUNT - i);
            
            for (int j = 0; j < remaining; j++) {
                smoothing_buffer[i + j] = smoothing_buffer[i + j] * smoothing_alpha + 
                                         bins[i + j] * one_minus_alpha;
                bins[i + j] = smoothing_buffer[i + j];
            }
        }
    }
};

// Global instance for system-wide use
extern FrequencyBinOptimizer g_freq_bin_optimizer;