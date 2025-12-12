/*
-----------------------------------------------------------------------------------------
    Optimized Audio Processing with Fixed-Point Math
    High-performance audio capture and preprocessing for ESP32-S3
-----------------------------------------------------------------------------------------

    DEPRECATION NOTICE
    ==================
    This file is part of the legacy monolithic audio pipeline.
    It will be replaced by the pluggable node architecture.
    
    Replacement: i2s_input_node.h + dc_offset_node.h
    Migration: See DEPRECATION_TRACKER.md
    Target removal: After Phase 3 completion
    
    DO NOT ADD NEW FEATURES TO THIS FILE
*/

#include "audio/audio_processing.h"
#include "audio/optimized_math.h"
#include <Arduino.h>

// Fixed-point constants for audio processing
#define FIXED_POINT_SHIFT 15  // Q15 format for audio samples
#define FIXED_ONE_Q15 (1 << FIXED_POINT_SHIFT)
#define FIXED_HALF_Q15 (1 << (FIXED_POINT_SHIFT - 1))

// Pre-calculated fixed-point constants
static const int32_t PRE_EMPHASIS_FIXED = (int32_t)(0.97f * FIXED_ONE_Q15);
static const int32_t RECIP_SCALE_FIXED = (int32_t)((1.0f / 131072.0f) * FIXED_ONE_Q15);
static const int32_t TARGET_RMS_FIXED = 8192 << 8;  // Q8 format for RMS

// Fast approximation for RMS using lookup table
static uint16_t rms_sqrt_lut[256];  // Sqrt lookup for RMS calculation
static bool rms_lut_initialized = false;

static void initRMSLookup() {
    if (rms_lut_initialized) return;
    
    for (int i = 0; i < 256; i++) {
        rms_sqrt_lut[i] = (uint16_t)(sqrt(i * 256.0f) * 16.0f);
    }
    rms_lut_initialized = true;
}

void AudioProcessor::init() {
    Serial.println("AudioProcessor::init() - Starting initialization");
    
    // Initialize RMS lookup table
    initRMSLookup();
    Serial.println("  - RMS lookup table initialized");
    
    // SPH0645-specific I2S configuration
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,    // SPH0645 is on LEFT channel!
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S | I2S_COMM_FORMAT_STAND_MSB),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = SAMPLE_BUFFER_SIZE / 4,  // 128/4 = 32, matches working version
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        .bits_per_chan = I2S_BITS_PER_CHAN_32BIT
    };
    
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK_PIN,
        .ws_io_num = I2S_LRCLK_PIN,
        .data_out_num = -1,
        .data_in_num = I2S_DIN_PIN
    };
    
    Serial.println("  - Installing I2S driver...");
    esp_err_t ret = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    if (ret != ESP_OK) {
        Serial.printf("ERROR: Failed to install I2S driver: %s\n", esp_err_to_name(ret));
        i2s_initialized = false;
        return;
    }
    Serial.println("  - I2S driver installed successfully");
    
    Serial.printf("  - Setting I2S pins: BCLK=%d, LRCLK=%d, DIN=%d\n", 
                  I2S_BCLK_PIN, I2S_LRCLK_PIN, I2S_DIN_PIN);
    ret = i2s_set_pin(I2S_NUM_0, &pin_config);
    if (ret != ESP_OK) {
        Serial.printf("ERROR: Failed to set I2S pins: %s\n", esp_err_to_name(ret));
        i2s_driver_uninstall(I2S_NUM_0);
        i2s_initialized = false;
        return;
    }
    Serial.println("  - I2S pins configured successfully");
    
    delay(50);  // SPH0645 startup time
    
    prev_sample = 0;
    target_rms = 8192.0f;  // Float version for compatibility
    target_rms_fixed = TARGET_RMS_FIXED;  // Fixed-point version
    consecutive_failures = 0;
    i2s_initialized = true;
    
    // Initialize DC offset calibrator - CRITICAL for audio integrity
    dc_calibrator.begin();
    Serial.println("  - DC offset calibrator initialized (proven 360.0f system)");
}

bool AudioProcessor::readSamples() {
    if (!i2s_initialized) {
        return false;
    }
    
    size_t bytes_read;
    int32_t mono_buffer[SAMPLE_BUFFER_SIZE];
    
    esp_err_t result = i2s_read(I2S_NUM_0, mono_buffer, 
                               SAMPLE_BUFFER_SIZE * sizeof(int32_t), 
                               &bytes_read, portMAX_DELAY);
    
    if (result == ESP_OK && bytes_read >= SAMPLE_BUFFER_SIZE * sizeof(int32_t)) {
        consecutive_failures = 0;
        
        // Debug: check for all zeros and print sample info
        static int debug_counter = 0;
        if (++debug_counter % 50 == 0) {
            int zero_count = 0;
            int32_t max_val = 0;
            int32_t min_val = 0;
            for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
                if (mono_buffer[i] == 0) zero_count++;
                if (mono_buffer[i] > max_val) max_val = mono_buffer[i];
                if (mono_buffer[i] < min_val) min_val = mono_buffer[i];
            }
            Serial.printf("I2S Debug: zeros=%d/%d, raw_min=%d, raw_max=%d, mode=RAW\n", 
                         zero_count, SAMPLE_BUFFER_SIZE, min_val, max_val);
        }
        
        // Process samples with battle-tested DC offset calibration
        for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
            // Extract 18-bit data from 32-bit I2S frame
            int32_t sample32 = mono_buffer[i] >> 14;
            
            // Feed SHIFTED samples to DC offset calibrator (not raw 32-bit values!)
            dc_calibrator.processCalibrationSample(sample32);
            
            // RAW MODE - The legacy system found this was 10x better!
            // NO DC offset correction - let the DC blocking filter handle drift
            // This preserves the natural signal characteristics
            // sample32 += 0;  // RAW mode - no offset!
            
            // Store without clipping to preserve FULL dynamic range for beat detector
            // The beat detector needs to see the full amplitude variations
            samples[i] = (int16_t)sample32;
            
            // Apply DC blocking filter for drift removal
            samples[i] = dc_filter.process(samples[i]);
        }
        
        // Debug processed samples after DC offset
        static uint32_t processed_debug_counter = 0;
        static uint32_t clipping_warning_counter = 0;
        if (++processed_debug_counter % 100 == 0) {
            int16_t processed_min = 32767;
            int16_t processed_max = -32768;
            int32_t unclipped_min = 32767;
            int32_t unclipped_max = -32768;
            
            for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
                if (samples[i] < processed_min) processed_min = samples[i];
                if (samples[i] > processed_max) processed_max = samples[i];
                
                // Check unclipped range
                int32_t sample32 = (mono_buffer[i] >> 14) - (int32_t)dc_calibrator.getCurrentOffset();
                if (sample32 < unclipped_min) unclipped_min = sample32;
                if (sample32 > unclipped_max) unclipped_max = sample32;
            }
            
            Serial.printf("Processed audio range: [%d, %d] (after DC offset + blocking filter)\n", 
                         processed_min, processed_max);
                         
            // Warn if we would clip with proper bounds checking
            if (unclipped_min < -32768 || unclipped_max > 32767) {
                if (++clipping_warning_counter % 10 == 0) {
                    Serial.printf("WARNING: Audio would clip! Unclipped range: [%d, %d]\n", 
                                 unclipped_min, unclipped_max);
                }
            }
        }
        
        return true;
    }
    
    consecutive_failures++;
    if (consecutive_failures > 100) {
        Serial.println("I2S read failures exceeded threshold, reinitializing...");
        reinitialize();
    }
    return false;
}

void AudioProcessor::preprocess() {
    applyNoiseGateOptimized();
    applyPreEmphasisOptimized();
    // REMOVED: applyNormalizationOptimized() - AGC was destroying dynamic range needed for beat detection
    // Normalization should be applied AFTER beat detection, only for visualization if needed
}

void AudioProcessor::applyNoiseGateOptimized() {
    // Vectorized noise gate using absolute value comparison
    for (int i = 0; i < SAMPLE_BUFFER_SIZE - 3; i += 4) {
        // Process 4 samples at once
        int16_t s0 = samples[i];
        int16_t s1 = samples[i + 1];
        int16_t s2 = samples[i + 2];
        int16_t s3 = samples[i + 3];
        
        // Fast absolute value using bit manipulation
        uint16_t abs0 = (s0 ^ (s0 >> 15)) - (s0 >> 15);
        uint16_t abs1 = (s1 ^ (s1 >> 15)) - (s1 >> 15);
        uint16_t abs2 = (s2 ^ (s2 >> 15)) - (s2 >> 15);
        uint16_t abs3 = (s3 ^ (s3 >> 15)) - (s3 >> 15);
        
        // Conditional zeroing
        samples[i] = (abs0 < NOISE_THRESHOLD) ? 0 : s0;
        samples[i + 1] = (abs1 < NOISE_THRESHOLD) ? 0 : s1;
        samples[i + 2] = (abs2 < NOISE_THRESHOLD) ? 0 : s2;
        samples[i + 3] = (abs3 < NOISE_THRESHOLD) ? 0 : s3;
    }
    
    // Handle remaining samples
    for (int i = (SAMPLE_BUFFER_SIZE & ~3); i < SAMPLE_BUFFER_SIZE; i++) {
        if (abs(samples[i]) < NOISE_THRESHOLD) {
            samples[i] = 0;
        }
    }
}

void AudioProcessor::applyPreEmphasisOptimized() {
    // Fixed-point pre-emphasis filter
    for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
        int16_t current = samples[i];
        // Fixed-point multiplication: current - 0.97 * prev
        int32_t filtered = current - ((PRE_EMPHASIS_FIXED * prev_sample) >> FIXED_POINT_SHIFT);
        
        // Saturate to 16-bit
        if (filtered > 32767) filtered = 32767;
        else if (filtered < -32768) filtered = -32768;
        
        samples[i] = (int16_t)filtered;
        prev_sample = current;
    }
}

// Fast fixed-point RMS calculation
int32_t AudioProcessor::calculateRMSFixed() {
    uint32_t sum_squares = 0;
    
    // Unrolled loop for better performance
    for (int i = 0; i < SAMPLE_BUFFER_SIZE - 3; i += 4) {
        int32_t s0 = samples[i];
        int32_t s1 = samples[i + 1];
        int32_t s2 = samples[i + 2];
        int32_t s3 = samples[i + 3];
        
        sum_squares += (s0 * s0) >> 8;  // Scale down to prevent overflow
        sum_squares += (s1 * s1) >> 8;
        sum_squares += (s2 * s2) >> 8;
        sum_squares += (s3 * s3) >> 8;
    }
    
    // Handle remaining samples
    for (int i = (SAMPLE_BUFFER_SIZE & ~3); i < SAMPLE_BUFFER_SIZE; i++) {
        int32_t s = samples[i];
        sum_squares += (s * s) >> 8;
    }
    
    // Average and approximate square root
    uint32_t mean_squares = sum_squares / SAMPLE_BUFFER_SIZE;
    
    // Fast square root approximation for RMS
    if (mean_squares < 256) {
        return rms_sqrt_lut[mean_squares];
    } else {
        // For larger values, use FastMath
        return FastMath::fastSqrt32(mean_squares << 8);
    }
}

void AudioProcessor::applyNormalizationOptimized() {
    int32_t current_rms = calculateRMSFixed();
    
    if (current_rms > 100) {  // Avoid division by zero
        // Fixed-point gain calculation
        int32_t gain_fixed = (target_rms_fixed << 8) / current_rms;
        
        // Clamp gain to reasonable range (0.1 to 10.0 in Q8)
        if (gain_fixed < 26) gain_fixed = 26;       // 0.1 * 256
        else if (gain_fixed > 2560) gain_fixed = 2560;  // 10.0 * 256
        
        // Apply gain with saturation
        for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
            int32_t scaled = (samples[i] * gain_fixed) >> 8;
            
            // Fast saturation
            if (scaled > 32767) samples[i] = 32767;
            else if (scaled < -32768) samples[i] = -32768;
            else samples[i] = (int16_t)scaled;
        }
    }
}

// Override the virtual functions with optimized versions
void AudioProcessor::applyNoiseGate() {
    applyNoiseGateOptimized();
}

void AudioProcessor::applyPreEmphasis() {
    applyPreEmphasisOptimized();
}

void AudioProcessor::applyNormalization() {
    applyNormalizationOptimized();
}

float AudioProcessor::calculateRMS() {
    // Convert fixed-point RMS to float for compatibility
    return (float)calculateRMSFixed() / 16.0f;
}

void AudioProcessor::reinitialize() {
    Serial.println("Reinitializing I2S driver...");
    
    if (i2s_initialized) {
        i2s_driver_uninstall(I2S_NUM_0);
        i2s_initialized = false;
    }
    
    delay(100);
    init();
}