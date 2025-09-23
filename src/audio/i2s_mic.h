#pragma once

#include "../config/features.h"

#if FEATURE_AUDIO_SYNC
#include <Arduino.h>
#include <driver/i2s.h>
#include "audio_frame.h"
#include "DCOffsetCalibrator.h"
#include "Goertzel96.h"
#include "AudioSnapshot.h"

/**
 * I2S Microphone Handler for I2S Microphone
 * 
 * Provides real-time audio capture and analysis from I2S microphone
 * Pin Configuration (from hardware_config.h):
 * - BCLK (SCK): GPIO 3
 * - DOUT (DIN): GPIO 4  
 * - LRCL (WS): GPIO 2
 * - SEL: Tied to GND (left channel)
 */
class I2SMic {
private:
    static constexpr int I2S_NUM = I2S_NUM_0;
    static constexpr int SAMPLE_RATE = 16000;  // SPH0645 optimized rate
    static constexpr int DMA_BUF_COUNT = 4;
    static constexpr int DMA_BUF_LEN = 512;
    static constexpr int SAMPLE_BUFFER_SIZE = 128;  // EXACT AP_SOT: 128 samples
    
    // Pin configuration (from AP_SOT reference for SPH0645)
    static constexpr gpio_num_t PIN_BCLK = GPIO_NUM_16;  // I2S_BCLK_PIN
    static constexpr gpio_num_t PIN_DOUT = GPIO_NUM_10;  // I2S_DIN_PIN
    static constexpr gpio_num_t PIN_LRCL = GPIO_NUM_4;   // I2S_LRCLK_PIN
    
    // Audio processing buffers
    int32_t* sampleBuffer = nullptr;
    float* fftInput = nullptr;
    
    // State
    bool initialized = false;
    bool capturing = false;
    
    // Energy tracking
    float bassEnergy = 0;
    float midEnergy = 0;
    float highEnergy = 0;
    float overallEnergy = 0;
    
    // Beat detection
    float energyHistory[43] = {0}; // ~1 second at 43 FPS
    int historyIndex = 0;
    float beatThreshold = 1.2f;  // Lowered from 1.5f for better sensitivity
    bool beatDetected = false;
    unsigned long lastBeatTime = 0;
    int estimatedBPM = 120; // Default BPM
    
    // Simple FFT bins (no actual FFT, just frequency band analysis)
    static constexpr int FFT_BINS = 16;
    float fftBins[FFT_BINS] = {0};
    
    // DC offset calibrator
    DCOffsetCalibrator dcCalibrator;
    
    // Goertzel spectral analyzer
    Goertzel96 spectralAnalyzer;

    // Timing metrics
    uint32_t lastChunkTimestampUs = 0;
    uint32_t prevChunkTimestampUs = 0;
    uint32_t lastChunkIntervalUs = 0;
    int32_t lastChunkJitterUs = 0;
    uint32_t expectedChunkIntervalUs = (SAMPLE_BUFFER_SIZE * 1000000UL) / SAMPLE_RATE;
    uint32_t lastReadDurationUs = 0;
    uint32_t lastLedLatencyUs = 0;
    uint32_t timingSampleCounter = 0;
    uint32_t latencySampleCounter = 0;

public:
    I2SMic() = default;
    ~I2SMic();
    
    // Initialize I2S microphone
    bool begin();
    
    // Start/stop audio capture
    void startCapture();
    void stopCapture();
    
    // Update - call this in main loop
    void update();
    
    // Get current audio frame
    AudioFrame getCurrentFrame() const;
    
    // Check if mic is active (capturing)
    bool isActive() const { return capturing; }
    
    // Check if mic is initialized (driver installed)
    bool isInitialized() const { return initialized; }
    
    // Get raw energy levels
    float getBassEnergy() const { return bassEnergy; }
    float getMidEnergy() const { return midEnergy; }
    float getHighEnergy() const { return highEnergy; }
    float getOverallEnergy() const { return overallEnergy; }
    
    // Beat detection
    bool isBeatDetected() const { return beatDetected; }
    
    // Configuration
    void setBeatThreshold(float threshold) { beatThreshold = constrain(threshold, 1.0f, 3.0f); }
    float getBeatThreshold() const { return beatThreshold; }

    // Timing instrumentation
    uint32_t getLastChunkTimestampUs() const { return lastChunkTimestampUs; }
    uint32_t getLastChunkIntervalUs() const { return lastChunkIntervalUs; }
    int32_t getLastChunkJitterUs() const { return lastChunkJitterUs; }
    uint32_t getExpectedChunkIntervalUs() const { return expectedChunkIntervalUs; }
    uint32_t getLastLedLatencyUs() const { return lastLedLatencyUs; }
    uint32_t getLastReadDurationUs() const { return lastReadDurationUs; }
    void markLedFrameComplete(uint32_t ledCompleteUs);

private:
    // Process audio samples
    void processSamples();
    
    // Frequency band analysis
    void analyzeFrequencyBands(int32_t* samples, size_t count);
    
    // Beat detection algorithm
    void detectBeat();
    
    // Generate synthetic FFT bins from frequency analysis
    void generateFFTBins();
};

// Global instance
extern I2SMic i2sMic;
#endif // FEATURE_AUDIO_SYNC
