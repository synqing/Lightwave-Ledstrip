#include "i2s_mic.h"
#include <math.h>

// Global instance
I2SMic i2sMic;

I2SMic::~I2SMic() {
    if (initialized) {
        stopCapture();
        i2s_driver_uninstall((i2s_port_t)I2S_NUM);
    }
    
    if (sampleBuffer) {
        free(sampleBuffer);
        sampleBuffer = nullptr;
    }
    if (fftInput) {
        free(fftInput);
        fftInput = nullptr;
    }
}

bool I2SMic::begin() {
    Serial.println("[I2SMic] Initializing SPH0645 (EXACT AP_SOT COPY)...");
    
    // Allocate buffers - using uint32_t like AP_SOT
    sampleBuffer = (int32_t*)malloc(SAMPLE_BUFFER_SIZE * sizeof(uint32_t));
    fftInput = (float*)malloc(SAMPLE_BUFFER_SIZE * sizeof(float));
    
    if (!sampleBuffer || !fftInput) {
        Serial.println("[I2SMic] Failed to allocate buffers!");
        return false;
    }
    
    // EXACT AP_SOT SPH0645-specific I2S configuration
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = 16000,  // EXACT AP_SOT: 16kHz for SPH0645
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,    // SPH0645 is on LEFT channel!
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S | I2S_COMM_FORMAT_STAND_MSB),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 128 / 4,  // EXACT AP_SOT: 128/4 = 32, matches working version
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        .bits_per_chan = I2S_BITS_PER_CHAN_32BIT
    };
    
    // EXACT AP_SOT pin configuration
    i2s_pin_config_t pin_config = {
        .bck_io_num = 16,      // EXACT AP_SOT: BCLK = GPIO 16
        .ws_io_num = 4,        // EXACT AP_SOT: LRCLK/WS = GPIO 4
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = 10      // EXACT AP_SOT: DIN = GPIO 10
    };
    
    // Install driver
    esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[I2SMic] Failed to install I2S driver: %s\n", esp_err_to_name(err));
        return false;
    }
    Serial.println("[I2SMic] I2S driver installed successfully");
    
    // Set pins
    err = i2s_set_pin(I2S_NUM_0, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("[I2SMic] Failed to set I2S pins: %s\n", esp_err_to_name(err));
        i2s_driver_uninstall(I2S_NUM_0);
        return false;
    }
    Serial.printf("[I2SMic] I2S pins configured: BCLK=%d, LRCLK=%d, DIN=%d\n", 16, 4, 10);
    
    // Clear DMA buffer (EXACT AP_SOT)
    i2s_zero_dma_buffer(I2S_NUM_0);
    
    // SPH0645 startup time (reduced from AP_SOT reference)
    delay(50);
    
    initialized = true;
    Serial.println("[I2SMic] SPH0645 ready with AP_SOT configuration");
    return true;
}

void I2SMic::startCapture() {
    if (!initialized) return;
    
    // Restart I2S if it was stopped
    i2s_start((i2s_port_t)I2S_NUM);
    i2s_zero_dma_buffer((i2s_port_t)I2S_NUM);
    
    // Reset DC calibrator for fresh start
    dcCalibrator.reset();
    
    capturing = true;
    Serial.println("[I2SMic] Started capture with I2S restart");
}

void I2SMic::stopCapture() {
    if (!initialized) return;
    
    capturing = false;
    i2s_stop((i2s_port_t)I2S_NUM);
    
    // Clear energy values
    bassEnergy = 0;
    midEnergy = 0;
    highEnergy = 0;
    overallEnergy = 0;
    beatDetected = false;
    
    Serial.println("[I2SMic] Stopped capture");
}

void I2SMic::update() {
    if (!capturing) return;
    
    // EXACT AP_SOT I2S reading with detailed debugging
    size_t bytes_read = 0;
    uint32_t* i2s_buffer = (uint32_t*)sampleBuffer;  // Cast to uint32_t like AP_SOT
    
    esp_err_t err = i2s_read(I2S_NUM_0, i2s_buffer, 
                            128 * sizeof(uint32_t),  // EXACT AP_SOT: 128 samples
                            &bytes_read, 100 / portTICK_PERIOD_MS);
    
    if (err != ESP_OK || bytes_read == 0) {
        static int failure_count = 0;
        if (++failure_count % 100 == 0) {
            Serial.printf("[I2SMic] I2S read failed: %s, bytes_read=%d\n", 
                         esp_err_to_name(err), bytes_read);
        }
        return;
    }
    
    // Convert 32-bit I2S data like AP_SOT
    size_t samples_read = bytes_read / sizeof(uint32_t);
    
    // EXACT AP_SOT: Debug raw I2S data (reduced frequency for cleaner output)
    static int raw_debug = 0;
    if (raw_debug++ % 10000 == 0 && samples_read > 4) {  // Reduced from 2000 to 10000
        Serial.printf("[I2SMic] RAW I2S data (hex): ");
        for (int j = 0; j < 4; j++) {
            Serial.printf("0x%08X ", i2s_buffer[j]);
        }
        Serial.printf("\n");
        
        // EXACT AP_SOT: Try different extraction methods
        Serial.printf("[I2SMic] Extraction tests on first sample 0x%08X:\n", i2s_buffer[0]);
        int32_t test1 = (int32_t)i2s_buffer[0] >> 14;  // Current method
        int32_t test2 = (int32_t)i2s_buffer[0] >> 16;  // Alternative: 16-bit in upper half
        int32_t test3 = (int32_t)(i2s_buffer[0] & 0xFFFFC000) >> 14; // Mask then shift
        Serial.printf("[I2SMic]   >>14: %d, >>16: %d, mask>>14: %d\n", test1, test2, test3);
    }
    
    int32_t min_val = INT32_MAX, max_val = INT32_MIN;
    float sum = 0.0f;
    
    // EXACT AP_SOT: Process each sample
    for (size_t i = 0; i < samples_read && i < SAMPLE_BUFFER_SIZE; i++) {
        // EXACT AP_SOT: SPH0645 outputs 18-bit data left-justified in 32-bit word
        // The data is in bits 31:14, with bits 13:0 typically zeros
        
        int32_t raw32 = (int32_t)i2s_buffer[i];
        
        // EXACT AP_SOT: Extract 18-bit signed value from upper bits
        // Method: Arithmetic shift right by 14 to get 18-bit value with sign extension
        int32_t raw = raw32 >> 14;
        
        // Process through DC offset calibrator
        float output = dcCalibrator.processSample(raw);
        
        if (raw > max_val) max_val = raw;
        if (raw < min_val) min_val = raw;
        sum += raw;
        
        // Store DC-blocked output for frequency analysis
        fftInput[i] = output;
        
        // Feed to Goertzel analyzer
        spectralAnalyzer.processSample(output);
    }
    
    // EXACT AP_SOT: Calculate and display statistics (reduced frequency)
    static int stats_debug = 0;
    if (stats_debug++ % 5000 == 0) {  // Reduced from 1000 to 5000
        float avg = sum / samples_read;
        Serial.printf("[I2SMic] I2S Stats: samples=%d, range=[%d,%d], avg=%.1f\n", 
                     samples_read, min_val, max_val, avg);
    }
    
    // Process the converted samples
    analyzeFrequencyBands(nullptr, samples_read);  // Use fftInput instead
    detectBeat();
    generateFFTBins();
    
    // Update global audio snapshot
    AudioFrame currentFrame = getCurrentFrame();
    float spectralData[Goertzel96::NUM_BINS];
    spectralAnalyzer.getMagnitudes(spectralData);
    audioSnapshot.update(currentFrame, spectralData, fftBins);
}

void I2SMic::processSamples() {
    // SPH0645 outputs 18-bit data in 32-bit words
    // Need to extract and normalize
    for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
        // Extract 18-bit value (SPH0645 specific)
        int32_t sample = sampleBuffer[i] >> 14; // Shift to get 18-bit value
        
        // Convert to float and normalize
        fftInput[i] = (float)sample / 131072.0f; // 2^17 for 18-bit
    }
}

void I2SMic::analyzeFrequencyBands(int32_t* unused, size_t count) {
    // AP_SOT-style frequency band analysis using the converted float data
    // Using zero-crossing and energy methods on RAW audio values
    
    float bassSum = 0;
    float midSum = 0;
    float highSum = 0;
    
    // Energy calculation with frequency weighting on RAW values
    for (size_t i = 1; i < count && i < SAMPLE_BUFFER_SIZE; i++) {
        float current = fftInput[i];      // RAW audio value
        float previous = fftInput[i-1];   // RAW audio value
        float delta = current - previous;
        
        // Approximate frequency from rate of change (using RAW 18-bit values)
        float freq_indicator = fabsf(delta);
        
        // Energy calculation using RAW values
        float energy = current * current;
        
        // Weighted distribution based on frequency indicator (for 18-bit RAW values)
        if (freq_indicator < 10.0f) {  // Low frequency - bass (small changes in RAW values)
            bassSum += energy * 1.5f;
        } else if (freq_indicator < 100.0f) {  // Mid frequency (medium changes)
            midSum += energy;
        } else {  // High frequency (large changes)
            highSum += energy * 0.7f;
        }
    }
    
    // Normalize and apply smoothing (for RAW 18-bit values)
    // Use empirical scale factor for SPH0645 microphone instead of theoretical max
    // This provides much better dynamic range and sensitivity
    float scale_factor = 1.0f / (count * 1000000.0f);  // Empirical value for SPH0645
    float newBass = sqrtf(bassSum * scale_factor) * 2.0f;   // Reduced multiplier for better range
    float newMid = sqrtf(midSum * scale_factor) * 2.5f;     // Reduced multiplier for better range
    float newHigh = sqrtf(highSum * scale_factor) * 3.0f;   // Reduced multiplier for better range
    
    // Smooth transitions
    bassEnergy = bassEnergy * 0.7f + newBass * 0.3f;
    midEnergy = midEnergy * 0.7f + newMid * 0.3f;
    highEnergy = highEnergy * 0.7f + newHigh * 0.3f;
    
    // Constrain to 0-1 range
    bassEnergy = constrain(bassEnergy, 0.0f, 1.0f);
    midEnergy = constrain(midEnergy, 0.0f, 1.0f);
    highEnergy = constrain(highEnergy, 0.0f, 1.0f);
    
    // Overall energy
    overallEnergy = (bassEnergy + midEnergy + highEnergy) / 3.0f;
    
    // Debug energy levels (reduced frequency)
    static int energy_debug = 0;
    if (energy_debug++ % 10000 == 0 && overallEnergy > 0.001f) {  // Reduced from 2000 to 10000
        Serial.printf("[I2SMic] Energy - Bass:%.3f Mid:%.3f High:%.3f Overall:%.3f\n", 
                     bassEnergy, midEnergy, highEnergy, overallEnergy);
    }
}

void I2SMic::detectBeat() {
    // Enhanced beat detection using spectral flux and energy variance
    static float previousBassEnergy = 0.0f;
    static float previousMidEnergy = 0.0f;
    
    // Calculate spectral flux (positive differences in energy)
    float bassFlux = fmaxf(0.0f, bassEnergy - previousBassEnergy);
    float midFlux = fmaxf(0.0f, midEnergy - previousMidEnergy);
    
    // Update history
    previousBassEnergy = bassEnergy;
    previousMidEnergy = midEnergy;
    
    // Store overall energy history for variance calculation
    energyHistory[historyIndex] = overallEnergy;
    historyIndex = (historyIndex + 1) % 43;
    
    // Calculate average energy
    float avgEnergy = 0;
    for (int i = 0; i < 43; i++) {
        avgEnergy += energyHistory[i];
    }
    avgEnergy /= 43.0f;
    
    // Combined detection: spectral flux + energy variance
    float variance = overallEnergy / (avgEnergy + 0.0001f);
    float spectralFluxScore = (bassFlux * 2.0f + midFlux) / 3.0f;  // Weight bass more heavily
    
    // Beat detected if either method triggers (with different thresholds)
    bool fluxBeat = spectralFluxScore > 0.15f && bassEnergy > 0.05f;  // Lower thresholds
    bool energyBeat = (variance > beatThreshold) && (bassEnergy > 0.1f);
    
    // Apply temporal gating to prevent false positives
    unsigned long now = millis();
    unsigned long minBeatInterval = 60000 / (estimatedBPM * 2);  // Min interval = half the beat period
    
    if ((fluxBeat || energyBeat) && (now - lastBeatTime) > minBeatInterval) {
        beatDetected = true;
        
        // Simple BPM estimation based on beat intervals
        if (lastBeatTime > 0) {
            unsigned long interval = now - lastBeatTime;
            int newBPM = 60000 / interval;
            // Smooth BPM estimation
            if (newBPM > 60 && newBPM < 200) {
                estimatedBPM = (estimatedBPM * 3 + newBPM) / 4;
            }
        }
        lastBeatTime = now;
        
        // Debug output (reduced frequency)
        static int beat_debug = 0;
        if (beat_debug++ % 10 == 0) {
            Serial.printf("[I2SMic] Beat! BPM:%d Flux:%.3f Var:%.3f\n", 
                         estimatedBPM, spectralFluxScore, variance);
        }
    } else {
        beatDetected = false;
    }
}

void I2SMic::generateFFTBins() {
    // Use real spectral data from Goertzel96 analyzer
    if (spectralAnalyzer.isReady()) {
        // Map 96 Goertzel bins to 16 display bins
        float goertzelMagnitudes[Goertzel96::NUM_BINS];
        spectralAnalyzer.getMagnitudes(goertzelMagnitudes);
        
        // Average multiple Goertzel bins per display bin
        int binsPerFFT = Goertzel96::NUM_BINS / FFT_BINS;
        
        for (int i = 0; i < FFT_BINS; i++) {
            float sum = 0.0f;
            int startBin = i * binsPerFFT;
            int endBin = min((i + 1) * binsPerFFT, Goertzel96::NUM_BINS);
            
            for (int j = startBin; j < endBin; j++) {
                sum += goertzelMagnitudes[j];
            }
            
            fftBins[i] = sum / (endBin - startBin);
            
            // Apply slight smoothing with previous value
            static float prevBins[FFT_BINS] = {0};
            fftBins[i] = 0.7f * fftBins[i] + 0.3f * prevBins[i];
            prevBins[i] = fftBins[i];
            
            fftBins[i] = constrain(fftBins[i], 0.0f, 1.0f);
        }
        
        spectralAnalyzer.clearReady();
    }
}

AudioFrame I2SMic::getCurrentFrame() const {
    AudioFrame frame;
    
    // Set frequency bins (const pointer to our internal buffer)
    frame.frequency_bins = fftBins;
    
    // Energy values
    frame.bass_energy = bassEnergy;
    frame.mid_energy = midEnergy;
    frame.high_energy = highEnergy;
    frame.total_energy = overallEnergy;
    
    // Detection flags
    frame.silence = overallEnergy < 0.01f;
    frame.transient_detected = beatDetected; // Simplified - transient detection same as beat
    frame.beat_detected = beatDetected;
    frame.beat_confidence = beatDetected ? 0.8f : 0.0f;
    frame.bpm_estimate = estimatedBPM; // Now with actual BPM estimation
    
    return frame;
}