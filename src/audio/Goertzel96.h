#ifndef GOERTZEL96_H
#define GOERTZEL96_H

#include <Arduino.h>
#include <math.h>

/**
 * Goertzel96 - Efficient spectral analysis for audio effects
 * 
 * Features:
 * - 96 frequency bins optimized for LED effects
 * - Efficient Goertzel algorithm (no FFT required)
 * - Logarithmic frequency spacing for musical relevance
 * - Built-in window function and normalization
 * - Optimized for ESP32-S3 performance
 */
class Goertzel96 {
public:
    static constexpr int NUM_BINS = 96;
    static constexpr int SAMPLE_RATE = 16000;  // MUST MATCH I2S MIC (AP_SOT)
    static constexpr int BLOCK_SIZE = 128;     // 8ms blocks at 16kHz
    
private:
    // Frequency bin configuration
    struct FrequencyBin {
        float frequency;      // Target frequency in Hz
        float coefficient;    // 2 * cos(2π * f / fs)
        float s1, s2;        // State variables
        float magnitude;     // Output magnitude
    };
    
    FrequencyBin bins[NUM_BINS];
    
    // Processing state
    float windowFunction[BLOCK_SIZE];
    float inputBuffer[BLOCK_SIZE];
    int sampleIndex = 0;
    bool blockReady = false;
    
    // Normalization
    float maxMagnitude = 1.0f;
    float smoothingFactor = 0.85f;
    
public:
    Goertzel96() {
        initializeFrequencies();
        initializeWindow();
    }
    
    /**
     * Process a single audio sample
     * @return true if a complete block was processed
     */
    bool processSample(float sample) {
        inputBuffer[sampleIndex++] = sample;
        
        if (sampleIndex >= BLOCK_SIZE) {
            processBlock();
            sampleIndex = 0;
            blockReady = true;
            return true;
        }
        return false;
    }
    
    /**
     * Process a buffer of samples
     * @return number of complete blocks processed
     */
    int processBuffer(float* samples, int count) {
        int blocksProcessed = 0;
        
        for (int i = 0; i < count; i++) {
            if (processSample(samples[i])) {
                blocksProcessed++;
            }
        }
        
        return blocksProcessed;
    }
    
    /**
     * Get magnitude for a specific bin
     */
    float getMagnitude(int bin) const {
        if (bin >= 0 && bin < NUM_BINS) {
            return bins[bin].magnitude;
        }
        return 0.0f;
    }
    
    /**
     * Get all magnitudes (normalized 0-1)
     */
    void getMagnitudes(float* output) const {
        for (int i = 0; i < NUM_BINS; i++) {
            output[i] = bins[i].magnitude;
        }
    }
    
    /**
     * Get frequency for a specific bin
     */
    float getFrequency(int bin) const {
        if (bin >= 0 && bin < NUM_BINS) {
            return bins[bin].frequency;
        }
        return 0.0f;
    }
    
    /**
     * Reset all state
     */
    void reset() {
        for (int i = 0; i < NUM_BINS; i++) {
            bins[i].s1 = 0.0f;
            bins[i].s2 = 0.0f;
            bins[i].magnitude = 0.0f;
        }
        sampleIndex = 0;
        blockReady = false;
        maxMagnitude = 1.0f;
    }
    
    /**
     * Check if new spectral data is available
     */
    bool isReady() const { return blockReady; }
    
    /**
     * Clear ready flag
     */
    void clearReady() { blockReady = false; }
    
private:
    void initializeFrequencies() {
        // Logarithmic frequency spacing from 20Hz to 20kHz
        float minFreq = 20.0f;
        float maxFreq = 20000.0f;
        float logMin = log10f(minFreq);
        float logMax = log10f(maxFreq);
        
        for (int i = 0; i < NUM_BINS; i++) {
            // Calculate frequency for this bin
            float logFreq = logMin + (logMax - logMin) * i / (NUM_BINS - 1);
            float freq = powf(10.0f, logFreq);
            
            // Limit to Nyquist frequency
            if (freq > SAMPLE_RATE / 2.0f) {
                freq = SAMPLE_RATE / 2.0f;
            }
            
            bins[i].frequency = freq;
            
            // Calculate Goertzel coefficient
            float omega = 2.0f * PI * freq / SAMPLE_RATE;
            bins[i].coefficient = 2.0f * cosf(omega);
            
            // Initialize state
            bins[i].s1 = 0.0f;
            bins[i].s2 = 0.0f;
            bins[i].magnitude = 0.0f;
        }
        
        // Print frequency distribution for debugging
        Serial.println("[Goertzel96] Frequency bins initialized:");
        for (int i = 0; i < NUM_BINS; i += 12) {
            Serial.printf("  Bin %2d: %.1f Hz\n", i, bins[i].frequency);
        }
    }
    
    void initializeWindow() {
        // Hamming window for better frequency resolution
        for (int i = 0; i < BLOCK_SIZE; i++) {
            windowFunction[i] = 0.54f - 0.46f * cosf(2.0f * PI * i / (BLOCK_SIZE - 1));
        }
    }
    
    void processBlock() {
        // Apply window function (reuse inputBuffer to avoid stack allocation)
        for (int i = 0; i < BLOCK_SIZE; i++) {
            inputBuffer[i] = inputBuffer[i] * windowFunction[i];
        }
        
        // Process each frequency bin
        for (int bin = 0; bin < NUM_BINS; bin++) {
            // Reset state for this bin
            bins[bin].s1 = 0.0f;
            bins[bin].s2 = 0.0f;
            
            // Goertzel algorithm
            for (int i = 0; i < BLOCK_SIZE; i++) {
                float s0 = inputBuffer[i] + bins[bin].coefficient * bins[bin].s1 - bins[bin].s2;
                bins[bin].s2 = bins[bin].s1;
                bins[bin].s1 = s0;
            }
            
            // Calculate magnitude (squared for efficiency)
            float real = bins[bin].s1 - bins[bin].s2 * cosf(2.0f * PI * bins[bin].frequency / SAMPLE_RATE);
            float imag = bins[bin].s2 * sinf(2.0f * PI * bins[bin].frequency / SAMPLE_RATE);
            float magnitude = sqrtf(real * real + imag * imag) / (BLOCK_SIZE / 2);
            
            // Smooth the magnitude
            bins[bin].magnitude = smoothingFactor * bins[bin].magnitude + 
                                 (1.0f - smoothingFactor) * magnitude;
            
            // Track maximum for normalization
            if (bins[bin].magnitude > maxMagnitude) {
                maxMagnitude = bins[bin].magnitude;
            }
        }
        
        // Normalize all magnitudes
        float normFactor = 1.0f / maxMagnitude;
        for (int i = 0; i < NUM_BINS; i++) {
            bins[i].magnitude *= normFactor;
            bins[i].magnitude = constrain(bins[i].magnitude, 0.0f, 1.0f);
        }
        
        // Slowly decay max magnitude for adaptive normalization
        maxMagnitude *= 0.995f;
        if (maxMagnitude < 0.1f) maxMagnitude = 0.1f;
    }
};

#endif // GOERTZEL96_H