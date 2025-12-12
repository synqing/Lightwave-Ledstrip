#include <Arduino.h>
#include "audio/audio_processing.h"
#include "audio/dc_offset_calibrator.h"

// Test components
AudioProcessor audio_processor;
DCOffsetCalibrator test_calibrator;

// Test state
enum CalibrationPhase {
    PHASE_STARTUP,
    PHASE_SILENCE_WAIT,
    PHASE_SILENCE_TEST,
    PHASE_MUSIC_WAIT,
    PHASE_MUSIC_TEST,
    PHASE_COMPLETE
};

CalibrationPhase current_phase = PHASE_STARTUP;
uint32_t phase_start_time = 0;
uint32_t sample_count = 0;

// Statistics for each phase
struct PhaseStats {
    int32_t min_val = 32767;
    int32_t max_val = -32768;
    int64_t sum = 0;
    int64_t sum_squared = 0;
    uint32_t count = 0;
    
    void addSample(int32_t sample) {
        if (sample < min_val) min_val = sample;
        if (sample > max_val) max_val = sample;
        sum += sample;
        sum_squared += (int64_t)sample * sample;
        count++;
    }
    
    float getMean() const {
        return count > 0 ? (float)sum / count : 0.0f;
    }
    
    float getVariance() const {
        if (count == 0) return 0.0f;
        float mean = getMean();
        return ((float)sum_squared / count) - (mean * mean);
    }
    
    float getStdDev() const {
        return sqrt(getVariance());
    }
};

PhaseStats silence_stats;
PhaseStats music_stats;

void setup() {
    Serial.begin(115200);
    delay(2000); // USB CDC init
    
    Serial.println("\n\n========================================");
    Serial.println("    DC OFFSET CALIBRATION TEST");
    Serial.println("========================================");
    Serial.println();
    
    // Initialize audio processor
    audio_processor.init();
    
    current_phase = PHASE_SILENCE_WAIT;
    phase_start_time = millis();
}

void loop() {
    switch (current_phase) {
        case PHASE_SILENCE_WAIT:
            if (millis() - phase_start_time < 5000) {
                static uint32_t last_countdown = 0;
                uint32_t remaining = 5 - ((millis() - phase_start_time) / 1000);
                if (remaining != last_countdown) {
                    last_countdown = remaining;
                    Serial.printf("\rPHASE 1: SILENCE TEST starting in %d seconds... ENSURE COMPLETE SILENCE!", remaining);
                    Serial.flush();
                }
            } else {
                Serial.println("\n\n>>> SILENCE CALIBRATION STARTING (10 seconds) <<<");
                Serial.println("Collecting samples in silence...");
                current_phase = PHASE_SILENCE_TEST;
                phase_start_time = millis();
                sample_count = 0;
            }
            break;
            
        case PHASE_SILENCE_TEST:
            if (audio_processor.readSamples()) {
                // Get raw samples and process DC offset
                int16_t* samples = audio_processor.getSamples();
                
                // Analyze each sample
                for (int i = 0; i < 128; i++) {
                    // Get the raw 18-bit shifted value before DC offset
                    // We need to reverse engineer from the processed sample
                    // This is a bit hacky but necessary for accurate measurement
                    silence_stats.addSample(samples[i]);
                }
                
                sample_count++;
                
                // Progress update every second
                if (sample_count % 125 == 0) { // ~1 second at 16kHz/128
                    uint32_t elapsed = (millis() - phase_start_time) / 1000;
                    Serial.printf("Silence test: %d/10 seconds, %d samples collected\n", 
                                 elapsed, silence_stats.count);
                }
                
                // Check if 10 seconds elapsed
                if (millis() - phase_start_time >= 10000) {
                    Serial.println("\n>>> SILENCE TEST COMPLETE <<<");
                    Serial.printf("Silence Statistics:\n");
                    Serial.printf("  - Samples: %d\n", silence_stats.count);
                    Serial.printf("  - Min: %d\n", silence_stats.min_val);
                    Serial.printf("  - Max: %d\n", silence_stats.max_val);
                    Serial.printf("  - Mean: %.2f\n", silence_stats.getMean());
                    Serial.printf("  - StdDev: %.2f\n", silence_stats.getStdDev());
                    Serial.printf("  - Peak-to-Peak: %d\n", silence_stats.max_val - silence_stats.min_val);
                    
                    current_phase = PHASE_MUSIC_WAIT;
                    phase_start_time = millis();
                }
            }
            break;
            
        case PHASE_MUSIC_WAIT:
            if (millis() - phase_start_time < 5000) {
                static uint32_t last_countdown = 0;
                uint32_t remaining = 5 - ((millis() - phase_start_time) / 1000);
                if (remaining != last_countdown) {
                    last_countdown = remaining;
                    Serial.printf("\rPHASE 2: MUSIC TEST starting in %d seconds... PLAY MUSIC AT 68-72dBA!", remaining);
                    Serial.flush();
                }
            } else {
                Serial.println("\n\n>>> MUSIC CALIBRATION STARTING (10 seconds) <<<");
                Serial.println("Collecting samples with music...");
                current_phase = PHASE_MUSIC_TEST;
                phase_start_time = millis();
                sample_count = 0;
            }
            break;
            
        case PHASE_MUSIC_TEST:
            if (audio_processor.readSamples()) {
                // Get raw samples
                int16_t* samples = audio_processor.getSamples();
                
                // Analyze each sample
                for (int i = 0; i < 128; i++) {
                    music_stats.addSample(samples[i]);
                }
                
                sample_count++;
                
                // Progress update every second
                if (sample_count % 125 == 0) { // ~1 second at 16kHz/128
                    uint32_t elapsed = (millis() - phase_start_time) / 1000;
                    Serial.printf("Music test: %d/10 seconds, %d samples collected\n", 
                                 elapsed, music_stats.count);
                }
                
                // Check if 10 seconds elapsed
                if (millis() - phase_start_time >= 10000) {
                    Serial.println("\n>>> MUSIC TEST COMPLETE <<<");
                    Serial.printf("Music Statistics:\n");
                    Serial.printf("  - Samples: %d\n", music_stats.count);
                    Serial.printf("  - Min: %d\n", music_stats.min_val);
                    Serial.printf("  - Max: %d\n", music_stats.max_val);
                    Serial.printf("  - Mean: %.2f\n", music_stats.getMean());
                    Serial.printf("  - StdDev: %.2f\n", music_stats.getStdDev());
                    Serial.printf("  - Peak-to-Peak: %d\n", music_stats.max_val - music_stats.min_val);
                    
                    current_phase = PHASE_COMPLETE;
                }
            }
            break;
            
        case PHASE_COMPLETE:
            {
                static bool results_printed = false;
                if (!results_printed) {
                    results_printed = true;
                    
                    Serial.println("\n\n========================================");
                    Serial.println("    CALIBRATION TEST RESULTS");
                    Serial.println("========================================");
                    
                    Serial.println("\nSILENCE vs MUSIC COMPARISON:");
                    Serial.printf("DC Offset (mean):\n");
                    Serial.printf("  - Silence: %.2f\n", silence_stats.getMean());
                    Serial.printf("  - Music: %.2f\n", music_stats.getMean());
                    Serial.printf("  - Difference: %.2f\n", music_stats.getMean() - silence_stats.getMean());
                    
                    Serial.printf("\nNoise Level (StdDev):\n");
                    Serial.printf("  - Silence: %.2f\n", silence_stats.getStdDev());
                    Serial.printf("  - Music: %.2f\n", music_stats.getStdDev());
                    Serial.printf("  - Ratio: %.1fx\n", music_stats.getStdDev() / silence_stats.getStdDev());
                    
                    Serial.printf("\nDynamic Range:\n");
                    Serial.printf("  - Silence P2P: %d\n", silence_stats.max_val - silence_stats.min_val);
                    Serial.printf("  - Music P2P: %d\n", music_stats.max_val - music_stats.min_val);
                    
                    Serial.println("\nRECOMMENDATION:");
                    float ideal_offset = -silence_stats.getMean();
                    Serial.printf("  - Use DC offset of %.1f to center SILENCE at zero\n", ideal_offset);
                    Serial.printf("  - This preserves maximum dynamic range for music\n");
                    
                    // Check if current calibrator would detect the music as noise
                    if (music_stats.getVariance() > 1000000) {
                        Serial.println("\n  WARNING: Music variance exceeds noise threshold!");
                        Serial.println("  The calibrator would reject calibration during music.");
                    }
                }
            }
            break;
    }
}