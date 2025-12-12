#ifndef AUDIO_PROCESSING_H
#define AUDIO_PROCESSING_H

#include <driver/i2s.h>
#include "../dc_blocking_filter.h"  // Robust DC removal
#include "dc_offset_calibrator.h"  // Battle-tested DC offset calibration

// I2S Pin definitions - Custom hardware
#define I2S_LRCLK_PIN 4   // LRCLK
#define I2S_BCLK_PIN  16  // BCLK  
#define I2S_DIN_PIN   10  // DOUT (mic data in)

class AudioProcessor {
private:
    static const int SAMPLE_BUFFER_SIZE = 128;
    static const int NOISE_THRESHOLD = 512;
    static constexpr float PRE_EMPHASIS_COEFF = 0.97f;
    static constexpr float RECIP_SCALE = 1.0f / 131072.0f; // max 18 bit signed value
    
    int16_t samples[SAMPLE_BUFFER_SIZE] __attribute__((aligned(16)));
    int16_t prev_sample;
    float target_rms;
    int32_t target_rms_fixed;  // Fixed-point version of target_rms
    uint32_t consecutive_failures = 0;
    bool i2s_initialized = false;
    DCBlockingFilter dc_filter;  // Robust DC removal
    DCOffsetCalibrator dc_calibrator;  // Battle-tested DC offset calibration
    
public:
    void init();
    bool readSamples();
    void preprocess();
    int16_t* getSamples() { return samples; }
    bool isHealthy() { return i2s_initialized && consecutive_failures < 10; }
    void reinitialize();
    
private:
    void applyNoiseGate();
    void applyPreEmphasis();
    void applyNormalization();
    float calculateRMS();
    
    // Optimized fixed-point versions
    void applyNoiseGateOptimized();
    void applyPreEmphasisOptimized();
    void applyNormalizationOptimized();
    int32_t calculateRMSFixed();
};

#endif
