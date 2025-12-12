#ifndef MULTIBAND_AGC_SYSTEM_H
#define MULTIBAND_AGC_SYSTEM_H

#include <Arduino.h>
#include "../config.h"
#include <cmath>

/**
 * Multiband AGC System - Cochlear-Inspired Frequency Band Processing
 * 
 * This implementation divides the frequency spectrum into 4 perceptually-balanced bands,
 * each with independent gain control optimized for its frequency characteristics.
 * 
 * Based on empirical testing showing dramatic improvements in audio responsiveness,
 * particularly for beats, drops, and note changes.
 */

class MultibandAGCSystem {
public:
    // Band definitions
    enum Band {
        BAND_BASS = 0,      // 20-200Hz    - Kick drums, bass fundamentals
        BAND_LOW_MID = 1,   // 200-800Hz   - Vocals, instruments body
        BAND_HIGH_MID = 2,  // 800-3kHz    - Presence, clarity
        BAND_TREBLE = 3,    // 3k-20kHz    - Cymbals, air, sparkle
        NUM_BANDS = 4
    };
    
    // Band frequency boundaries in Hz
    static constexpr float BAND_BOUNDARIES[NUM_BANDS + 1] = {
        20.0f,      // Bass start
        200.0f,     // Bass end / Low-mid start  
        800.0f,     // Low-mid end / High-mid start
        3000.0f,    // High-mid end / Treble start
        20000.0f    // Treble end
    };
    
    // Band-specific AGC parameters (empirically tuned for music visualization)
    struct BandConfig {
        float attack_ms;     // Attack time in milliseconds
        float release_ms;    // Release time in milliseconds  
        float max_gain;      // Maximum allowed gain
        float threshold;     // Compression threshold (0-1)
        float target_level;  // Target output level
    };
    
    static constexpr BandConfig BAND_CONFIGS[NUM_BANDS] = {
        // Bass: Slower attack prevents pumping, moderate release for groove
        { 10.0f, 200.0f, 8.0f, 0.3f, 0.7f },
        
        // Low-mid: Balanced for vocals and instruments
        { 20.0f, 300.0f, 6.0f, 0.4f, 0.6f },
        
        // High-mid: Fast attack for transients, slower release for presence
        { 15.0f, 400.0f, 5.0f, 0.5f, 0.5f },
        
        // Treble: Ultra-fast attack for cymbals, slow release preserves sparkle
        { 5.0f, 500.0f, 4.0f, 0.6f, 0.4f }
    };
    
    // Cross-band coupling for smooth transitions
    static constexpr float BAND_COUPLING_FACTOR = 0.2f;  // 20% influence from adjacent bands
    static constexpr float MAX_BAND_DIVERGENCE_DB = 6.0f;  // Maximum gain difference between bands
    
private:
    // Per-band state
    struct BandState {
        float current_gain = 1.0f;
        float target_gain = 1.0f;
        float energy = 0.0f;
        float peak_level = 0.0f;
        float noise_floor = 0.0001f;
        float attack_coeff;
        float release_coeff;
        
        // Dynamic range tracking
        float min_silent_level = 1.0f;
        float dynamic_ceiling = 0.0001f;
        
        // Variance tracking for dynamic time constants
        float energy_history[8] = {0};  // Rolling buffer for variance calculation
        int history_index = 0;
        float variance = 0.0f;
        float dynamic_attack_coeff;   // Adjusted based on variance
        float dynamic_release_coeff;  // Adjusted based on variance
    };
    
    BandState bands[NUM_BANDS];
    uint8_t freq_bin_to_band[FREQUENCY_BINS];  // Mapping of FFT bins to bands
    
    // Configuration
    float sample_rate = SAMPLE_RATE;
    bool use_a_weighting = false;
    bool initialized = false;
    
    // Silence detection integration
    bool in_silence = false;
    int silence_frames = 0;
    
public:
    MultibandAGCSystem() = default;
    
    void init(float sample_rate_hz = SAMPLE_RATE) {
        sample_rate = sample_rate_hz;
        
        // Initialize frequency bin to band mapping
        // Note: Using Goertzel musical frequencies, not linear FFT frequencies!
        // The Goertzel engine produces 96 bins from A0 (27.5Hz) to A7 (3520Hz)
        const float goertzel_frequencies[96] = {
            27.5f, 29.14f, 30.87f, 32.7f, 34.65f, 36.71f, 38.89f, 41.2f, 43.65f, 46.25f, 49.0f, 51.91f,  // A0-G#0
            55.0f, 58.27f, 61.74f, 65.41f, 69.3f, 73.42f, 77.78f, 82.41f, 87.31f, 92.5f, 98.0f, 103.83f,  // A1-G#1
            110.0f, 116.54f, 123.47f, 130.81f, 138.59f, 146.83f, 155.56f, 164.81f, 174.61f, 185.0f, 196.0f, 207.65f,  // A2-G#2
            220.0f, 233.08f, 246.94f, 261.63f, 277.18f, 293.66f, 311.13f, 329.63f, 349.23f, 369.99f, 392.0f, 415.3f,  // A3-G#3
            440.0f, 466.16f, 493.88f, 523.25f, 554.37f, 587.33f, 622.25f, 659.25f, 698.46f, 739.99f, 783.99f, 830.61f,  // A4-G#4
            880.0f, 932.33f, 987.77f, 1046.5f, 1108.73f, 1174.66f, 1244.51f, 1318.51f, 1396.91f, 1479.98f, 1567.98f, 1661.22f,  // A5-G#5
            1760.0f, 1864.66f, 1975.53f, 2093.0f, 2217.46f, 2349.32f, 2489.02f, 2637.02f, 2793.83f, 2959.96f, 3135.96f, 3322.44f,  // A6-G#6
            3520.0f, 3729.31f, 3951.07f, 4186.01f, 4434.92f, 4698.63f, 4978.03f, 5274.04f, 5587.65f, 5919.91f, 6271.93f, 6644.88f   // A7-G#7
        };
        
        for (int i = 0; i < FREQUENCY_BINS; i++) {
            float freq = (i < 96) ? goertzel_frequencies[i] : ((i * sample_rate) / (2.0f * FREQUENCY_BINS));
            
            // Determine which band this frequency belongs to
            freq_bin_to_band[i] = BAND_TREBLE;  // Default to highest band
            for (int band = 0; band < NUM_BANDS; band++) {
                if (freq >= BAND_BOUNDARIES[band] && freq < BAND_BOUNDARIES[band + 1]) {
                    freq_bin_to_band[i] = band;
                    break;
                }
            }
        }
        
        // Initialize band parameters
        for (int band = 0; band < NUM_BANDS; band++) {
            const auto& config = BAND_CONFIGS[band];
            
            // Calculate attack/release coefficients
            // Using one-pole filter: coeff = 1 - exp(-1 / (time_ms * sample_rate / 1000))
            float attack_samples = config.attack_ms * sample_rate / 1000.0f;
            float release_samples = config.release_ms * sample_rate / 1000.0f;
            
            bands[band].attack_coeff = 1.0f - exp(-1.0f / attack_samples);
            bands[band].release_coeff = 1.0f - exp(-1.0f / release_samples);
            bands[band].current_gain = 1.0f;
            bands[band].target_gain = 1.0f;
            
            // Initialize dynamic coefficients to default values
            bands[band].dynamic_attack_coeff = bands[band].attack_coeff;
            bands[band].dynamic_release_coeff = bands[band].release_coeff;
        }
        
        initialized = true;
        
        Serial.println("Multiband AGC initialized:");
        for (int band = 0; band < NUM_BANDS; band++) {
            Serial.printf("  Band %d (%d-%d Hz): attack=%.1fms, release=%.1fms, max_gain=%.1f\n",
                         band, 
                         (int)BAND_BOUNDARIES[band],
                         (int)BAND_BOUNDARIES[band + 1],
                         BAND_CONFIGS[band].attack_ms,
                         BAND_CONFIGS[band].release_ms,
                         BAND_CONFIGS[band].max_gain);
        }
    }
    
    /**
     * Process frequency magnitude data through multiband AGC
     * 
     * @param magnitudes Input frequency bin magnitudes
     * @param output Output frequency bin magnitudes (can be same as input)
     * @param num_bins Number of frequency bins
     * @param is_silence Whether the current frame is silence
     */
    void process(const float* magnitudes, float* output, int num_bins, bool is_silence = false) {
        if (!initialized) {
            Serial.println("WARNING: MultibandAGCSystem not initialized!");
            memcpy(output, magnitudes, num_bins * sizeof(float));
            return;
        }
        
        in_silence = is_silence;
        if (is_silence) {
            silence_frames++;
        } else {
            silence_frames = 0;
        }
        
        // Step 1: Calculate per-band energy and peak levels
        float band_energy[NUM_BANDS] = {0};
        float band_peak[NUM_BANDS] = {0};
        int band_bin_count[NUM_BANDS] = {0};
        
        for (int i = 0; i < num_bins && i < FREQUENCY_BINS; i++) {
            int band = freq_bin_to_band[i];
            float magnitude = magnitudes[i];
            
            // Accumulate energy (RMS) and track peak
            band_energy[band] += magnitude * magnitude;
            band_bin_count[band]++;
            
            if (magnitude > band_peak[band]) {
                band_peak[band] = magnitude;
            }
        }
        
        // Calculate RMS energy per band
        for (int band = 0; band < NUM_BANDS; band++) {
            if (band_bin_count[band] > 0) {
                bands[band].energy = sqrt(band_energy[band] / band_bin_count[band]);
                bands[band].peak_level = band_peak[band];
                
                // Apply A-weighting to the entire band energy if enabled
                if (use_a_weighting) {
                    bands[band].energy *= A_WEIGHTING_COEFFS[band];
                    bands[band].peak_level *= A_WEIGHTING_COEFFS[band];
                }
                
                // Update energy history for variance calculation
                bands[band].energy_history[bands[band].history_index] = bands[band].energy;
                bands[band].history_index = (bands[band].history_index + 1) % 8;
                
                // Calculate variance over the history window
                float mean = 0.0f;
                for (int i = 0; i < 8; i++) {
                    mean += bands[band].energy_history[i];
                }
                mean /= 8.0f;
                
                float variance = 0.0f;
                for (int i = 0; i < 8; i++) {
                    float diff = bands[band].energy_history[i] - mean;
                    variance += diff * diff;
                }
                variance /= 8.0f;
                bands[band].variance = variance;
                
                // Adjust time constants based on variance
                // High variance = transient content = faster response
                // Low variance = sustained content = slower response
                float variance_factor = constrain(variance / (mean * mean + 0.0001f), 0.0f, 1.0f);
                
                // Scale time constants: faster attack/release for transients
                float attack_scale = 1.0f - (variance_factor * 0.7f);   // 30-100% of original
                float release_scale = 1.0f - (variance_factor * 0.5f);  // 50-100% of original
                
                bands[band].dynamic_attack_coeff = bands[band].attack_coeff / attack_scale;
                bands[band].dynamic_release_coeff = bands[band].release_coeff / release_scale;
            }
        }
        
        // Step 2: Update dynamic range tracking during silence
        if (in_silence && silence_frames > 10) {
            for (int band = 0; band < NUM_BANDS; band++) {
                // Track minimum levels during silence for noise floor
                if (bands[band].energy < bands[band].min_silent_level && bands[band].energy > 0) {
                    bands[band].min_silent_level = bands[band].energy;
                }
                
                // Slowly recover the noise floor tracker
                bands[band].min_silent_level *= 1.001f;  // Slow upward drift
                
                // Update noise floor
                bands[band].noise_floor = bands[band].min_silent_level * 2.0f;  // 2x safety margin
            }
        }
        
        // Step 3: Update dynamic ceiling tracking
        for (int band = 0; band < NUM_BANDS; band++) {
            float current_peak = bands[band].peak_level * 0.995f;  // Slight decay
            
            if (current_peak > bands[band].dynamic_ceiling) {
                // Fast attack on ceiling
                float delta = current_peak - bands[band].dynamic_ceiling;
                bands[band].dynamic_ceiling += delta * 0.05f;
            } else {
                // Slow decay on ceiling  
                float delta = bands[band].dynamic_ceiling - current_peak;
                bands[band].dynamic_ceiling -= delta * 0.0025f;
            }
            
            // Ensure ceiling doesn't go below noise floor
            if (bands[band].dynamic_ceiling < bands[band].noise_floor * 10.0f) {
                bands[band].dynamic_ceiling = bands[band].noise_floor * 10.0f;
            }
        }
        
        // Step 4: Calculate target gain for each band
        for (int band = 0; band < NUM_BANDS; band++) {
            const auto& config = BAND_CONFIGS[band];
            float energy = bands[band].energy;
            
            // Skip very quiet signals
            if (energy < bands[band].noise_floor) {
                bands[band].target_gain = 1.0f;
                continue;
            }
            
            // Normalize energy to 0-1 range based on dynamic ceiling
            float normalized_energy = energy / bands[band].dynamic_ceiling;
            normalized_energy = constrain(normalized_energy, 0.0f, 1.0f);
            
            // Dual-mode compression/expansion (cochlear AGC style)
            if (normalized_energy > config.threshold) {
                // COMPRESSION MODE: Above threshold, reduce gain
                float excess = normalized_energy - config.threshold;
                float compression_ratio = 3.0f;  // 3:1 compression
                float compressed = config.threshold + (excess / compression_ratio);
                bands[band].target_gain = config.target_level / compressed;
            } else if (normalized_energy > bands[band].noise_floor * 3.0f) {
                // EXPANSION MODE: Between noise floor and threshold
                // Gently boost quiet signals without amplifying noise
                float expansion_ratio = 0.7f;  // 1:1.4 expansion
                float range = config.threshold - (bands[band].noise_floor * 3.0f);
                float position = (normalized_energy - bands[band].noise_floor * 3.0f) / range;
                
                // Apply soft knee expansion curve
                float expansion_curve = powf(position, expansion_ratio);
                float expanded_level = (bands[band].noise_floor * 3.0f) + expansion_curve * range;
                
                bands[band].target_gain = config.target_level / expanded_level;
            } else {
                // GATE MODE: Near noise floor, minimal gain
                bands[band].target_gain = 1.0f;
            }
            
            // Limit gain to configured maximum
            bands[band].target_gain = constrain(bands[band].target_gain, 0.1f, config.max_gain);
        }
        
        // Step 5: Apply cross-band coupling for smooth transitions
        if (BAND_COUPLING_FACTOR > 0) {
            float coupled_gains[NUM_BANDS];
            for (int i = 0; i < NUM_BANDS; i++) {
                coupled_gains[i] = bands[i].target_gain;
            }
            
            for (int band = 1; band < NUM_BANDS - 1; band++) {
                // Average with adjacent bands
                float adjacent_avg = (bands[band-1].target_gain + bands[band+1].target_gain) * 0.5f;
                coupled_gains[band] = bands[band].target_gain * (1.0f - BAND_COUPLING_FACTOR) +
                                     adjacent_avg * BAND_COUPLING_FACTOR;
            }
            
            // Apply coupled gains back
            for (int band = 0; band < NUM_BANDS; band++) {
                bands[band].target_gain = coupled_gains[band];
            }
        }
        
        // Step 6: Smooth gain changes using DYNAMIC attack/release
        for (int band = 0; band < NUM_BANDS; band++) {
            // Use dynamic coefficients that adapt to signal variance
            float coeff = (bands[band].target_gain < bands[band].current_gain) ? 
                          bands[band].dynamic_attack_coeff : bands[band].dynamic_release_coeff;
            
            bands[band].current_gain += (bands[band].target_gain - bands[band].current_gain) * coeff;
        }
        
        // Step 7: Apply cross-band stability measures (prevents "swimming" artifacts)
        // This is the KEY innovation from cochlear AGC that prevents bands from fighting each other
        for (int band = 0; band < NUM_BANDS - 1; band++) {
            // Calculate gain difference between adjacent bands
            float gain_diff = bands[band].current_gain - bands[band + 1].current_gain;
            
            // Convert to dB for perceptually meaningful comparison
            float gain_diff_db = 20.0f * log10f(fabsf(gain_diff) + 1.0f);
            
            // If difference exceeds maximum allowed, apply coupling correction
            if (gain_diff_db > MAX_BAND_DIVERGENCE_DB) {
                // Calculate correction amount
                float correction = (gain_diff_db - MAX_BAND_DIVERGENCE_DB) * BAND_COUPLING_FACTOR;
                
                // Convert back from dB to linear
                correction = powf(10.0f, correction / 20.0f) - 1.0f;
                
                // Apply symmetric correction to both bands
                if (bands[band].current_gain > bands[band + 1].current_gain) {
                    bands[band].current_gain -= correction;
                    bands[band + 1].current_gain += correction;
                } else {
                    bands[band].current_gain += correction;
                    bands[band + 1].current_gain -= correction;
                }
                
                // Ensure gains stay within valid range
                bands[band].current_gain = constrain(bands[band].current_gain, 0.1f, 
                                                     BAND_CONFIGS[band].max_gain);
                bands[band + 1].current_gain = constrain(bands[band + 1].current_gain, 0.1f, 
                                                         BAND_CONFIGS[band + 1].max_gain);
            }
        }
        
        // Step 8: Apply gains to output
        for (int i = 0; i < num_bins && i < FREQUENCY_BINS; i++) {
            int band = freq_bin_to_band[i];
            output[i] = magnitudes[i] * bands[band].current_gain;
        }
        
        // Debug output statistics
        static int debug_counter = 0;
        if (++debug_counter % 625 == 0) {  // Once per 5 seconds at 125Hz
            float out_min = 999999.0f, out_max = 0.0f, out_avg = 0.0f;
            for (int i = 0; i < num_bins; i++) {
                if (output[i] < out_min) out_min = output[i];
                if (output[i] > out_max) out_max = output[i];
                out_avg += output[i];
            }
            out_avg /= num_bins;
            
            Serial.printf("AGC DEBUG: Gains=[%.2f,%.2f,%.2f,%.2f] | Output: min=%.1f, max=%.1f, avg=%.1f\n",
                         bands[0].current_gain, bands[1].current_gain, 
                         bands[2].current_gain, bands[3].current_gain,
                         out_min, out_max, out_avg);
        }
    }
    
    // Get current band information for visualization/debugging
    void getBandInfo(int band, float& gain, float& energy, float& ceiling) const {
        if (band >= 0 && band < NUM_BANDS) {
            gain = bands[band].current_gain;
            energy = bands[band].energy;
            ceiling = bands[band].dynamic_ceiling;
        }
    }
    
    // Enable/disable A-weighting
    void setAWeighting(bool enable) {
        use_a_weighting = enable;
    }
    
private:
    // A-weighting coefficients from cochlear AGC
    // These represent relative sensitivity at different frequency bands
    static constexpr float A_WEIGHTING_COEFFS[NUM_BANDS] = {
        0.2f,    // BAND_BASS: -14dB (less sensitive to bass)
        0.5f,    // BAND_LOW_MID: -6dB
        1.0f,    // BAND_HIGH_MID: 0dB (reference band)
        0.7f     // BAND_TREBLE: -3dB
    };
    
    // A-weighting curve approximation
    float calculateAWeighting(float freq) {
        // For band-based A-weighting, return the coefficient for the band
        // This is more efficient than the full curve calculation
        for (int band = 0; band < NUM_BANDS; band++) {
            if (freq >= BAND_BOUNDARIES[band] && freq < BAND_BOUNDARIES[band + 1]) {
                return A_WEIGHTING_COEFFS[band];
            }
        }
        return 1.0f; // Default if out of range
        
        /* Full A-weighting calculation (commented out for efficiency):
        // Simplified A-weighting calculation
        float f2 = freq * freq;
        float f4 = f2 * f2;
        
        float num = 12194.0f * 12194.0f * f4;
        float den = (f2 + 20.6f * 20.6f) * 
                   sqrt((f2 + 107.7f * 107.7f) * (f2 + 737.9f * 737.9f)) * 
                   (f2 + 12194.0f * 12194.0f);
        
        return num / den;
        */
    }
};

#endif // MULTIBAND_AGC_SYSTEM_H