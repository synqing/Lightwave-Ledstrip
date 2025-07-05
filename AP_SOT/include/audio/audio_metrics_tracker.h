#pragma once

#include <Arduino.h>
#include <cstring>
#include "config.h"

// Comprehensive Audio Metrics Tracking System
class AudioMetricsTracker {
public:
    // Raw audio metrics
    struct RawMetrics {
        int16_t min_sample;
        int16_t max_sample;
        int32_t dc_offset;
        float rms;
        float peak_amplitude;
        uint32_t zero_crossings;
        uint32_t clipping_count;
        float dynamic_range_db;
    };

    // Frequency Analysis Metrics
    struct FrequencyMetrics {
        float total_energy;
        float bass_energy;
        float mid_energy;
        float high_energy;
        float spectral_centroid;
        float spectral_spread;
        float spectral_flux;
        float spectral_rolloff;
        uint8_t dominant_bin;
        float dominant_frequency;
        float harmonic_ratio;
        float max_magnitude;
    };

    // Beat detection metrics
    struct BeatMetrics {
        bool beat_detected;
        float beat_strength;
        float beat_confidence;
        float tempo_bpm;
        uint32_t beat_count;
        uint32_t last_beat_time;
        float beat_variance;
        float onset_strength;
    };

    // AGC metrics
    struct AGCMetrics {
        float current_gain;
        float target_gain;
        float noise_floor;
        float signal_presence;
        const char* state;  // "SILENT", "NORMAL", "LOUD"
        uint32_t state_changes;
        float compression_ratio;
    };

    // Performance metrics
    struct PerformanceMetrics {
        uint32_t processing_time_us;
        uint32_t analysis_time_us;
        uint32_t feature_time_us;
        float cpu_usage_percent;
        uint32_t buffer_overruns;
        uint32_t frame_drops;
        float fps;
    };

    // History tracking
    static constexpr int HISTORY_SIZE = 256;
    
    struct HistoryBuffer {
        float energy_history[HISTORY_SIZE];
        float tempo_history[32];
        float spectral_flux_history[HISTORY_SIZE];
        int write_index = 0;
        int tempo_index = 0;
        
        void add_energy(float energy) {
            energy_history[write_index] = energy;
            write_index = (write_index + 1) % HISTORY_SIZE;
        }
        
        void add_tempo(float tempo) {
            tempo_history[tempo_index] = tempo;
            tempo_index = (tempo_index + 1) % 32;
        }
        
        float get_average_energy(int samples = 32) {
            float sum = 0;
            int start = (write_index - samples + HISTORY_SIZE) % HISTORY_SIZE;
            for (int i = 0; i < samples; i++) {
                sum += energy_history[(start + i) % HISTORY_SIZE];
            }
            return sum / samples;
        }
    };

    // Complete metrics package
    struct CompleteMetrics {
        RawMetrics raw;
        FrequencyMetrics freq;
        BeatMetrics beat;
        AGCMetrics agc;
        PerformanceMetrics perf;
        HistoryBuffer history;
        uint32_t frame_number;
        uint32_t timestamp;
    };

private:
    CompleteMetrics metrics;
    uint32_t frame_counter = 0;
    
    // Moving averages
    float avg_energy = 0;
    float avg_tempo = 0;
    float avg_cpu = 0;

public:
    AudioMetricsTracker() {
        reset();
    }

    void reset() {
        memset(&metrics, 0, sizeof(metrics));
        metrics.agc.state = "SILENT";
        frame_counter = 0;
    }

    void startFrame() {
        metrics.frame_number = frame_counter++;
        metrics.timestamp = millis();
        metrics.perf.processing_time_us = micros();
    }

    void endFrame() {
        uint32_t total_time = micros() - metrics.perf.processing_time_us;
        metrics.perf.processing_time_us = total_time;
        metrics.perf.cpu_usage_percent = (total_time / 10000.0f) * 100.0f; // Assuming 10ms frame time
        
        // Update FPS
        static uint32_t last_fps_update = 0;
        static uint32_t fps_frames = 0;
        fps_frames++;
        
        if (millis() - last_fps_update >= 1000) {
            metrics.perf.fps = fps_frames;
            fps_frames = 0;
            last_fps_update = millis();
        }
        
        // Update history
        metrics.history.add_energy(metrics.freq.total_energy);
        if (metrics.beat.beat_detected) {
            metrics.history.add_tempo(metrics.beat.tempo_bpm);
        }
        
        // Update moving averages
        avg_energy = avg_energy * 0.95f + metrics.freq.total_energy * 0.05f;
        avg_cpu = avg_cpu * 0.9f + metrics.perf.cpu_usage_percent * 0.1f;
    }

    // Setters for metrics
    void updateRawMetrics(const int16_t* samples, size_t count) {
        int16_t min_val = 32767, max_val = -32768;
        int64_t sum = 0, sum_sq = 0;
        uint32_t zero_cross = 0, clips = 0;
        
        for (size_t i = 0; i < count; i++) {
            int16_t sample = samples[i];
            min_val = min(min_val, sample);
            max_val = max(max_val, sample);
            sum += sample;
            sum_sq += (int64_t)sample * sample;
            
            if (i > 0 && ((samples[i-1] < 0 && sample >= 0) || (samples[i-1] >= 0 && sample < 0))) {
                zero_cross++;
            }
            
            if (abs(sample) >= 32000) clips++;
        }
        
        metrics.raw.min_sample = min_val;
        metrics.raw.max_sample = max_val;
        metrics.raw.dc_offset = sum / count;
        metrics.raw.rms = sqrt((float)sum_sq / count);
        metrics.raw.peak_amplitude = max(abs(min_val), abs(max_val));
        metrics.raw.zero_crossings = zero_cross;
        metrics.raw.clipping_count = clips;
        metrics.raw.dynamic_range_db = 20.0f * log10f((float)max_val / (float)max((int16_t)1, min_val));
    }

    void updateFrequencyMetrics(const float* bins, size_t num_bins, float dominant_frequency) {
        float total = 0, bass = 0, mid = 0, high = 0;
        float weighted_sum = 0, freq_sum = 0;
        float max_magnitude = 0;
        int max_bin = 0;
        
        // This is a rough approximation for linear bins, not accurate for Goertzel
        // but okay for a general centroid.
        const float bin_width = (SAMPLE_RATE / 2.0f) / num_bins;
        
        for (size_t i = 1; i < num_bins; i++) {  // Skip DC
            float magnitude = bins[i];
            
            // This frequency calculation is NOT correct for Goertzel.
            // It should be looked up from a pre-calculated table.
            // For now, it's a placeholder.
            float frequency = i * bin_width;
            
            total += magnitude;
            weighted_sum += magnitude * frequency;
            freq_sum += magnitude;
            
            if (magnitude > max_magnitude) {
                max_magnitude = magnitude;
                max_bin = i;
            }
            
            if (i >= BASS_BINS_START && i <= BASS_BINS_END) bass += magnitude;
            else if (i >= MID_BINS_START && i <= MID_BINS_END) mid += magnitude;
            else high += magnitude;
        }
        
        metrics.freq.total_energy = total;
        metrics.freq.bass_energy = bass;
        metrics.freq.mid_energy = mid;
        metrics.freq.high_energy = high;
        metrics.freq.spectral_centroid = freq_sum > 0 ? weighted_sum / freq_sum : 0;
        metrics.freq.max_magnitude = max_magnitude;
        metrics.freq.dominant_bin = max_bin;
        metrics.freq.dominant_frequency = dominant_frequency;
        
        // Spectral flux (change from previous frame)
        static float prev_bins[FREQUENCY_BINS] = {0};
        float flux = 0;
        for (size_t i = 0; i < num_bins; i++) {
            float diff = bins[i] - prev_bins[i];
            if (diff > 0) flux += diff;
            prev_bins[i] = bins[i];
        }
        metrics.freq.spectral_flux = flux;
        metrics.history.spectral_flux_history[metrics.history.write_index] = flux;
    }

    void updateBeatMetrics(bool beat, float strength, float tempo, float onset) {
        metrics.beat.beat_detected = beat;
        metrics.beat.beat_strength = strength;
        metrics.beat.tempo_bpm = tempo;
        metrics.beat.onset_strength = onset;
        
        if (beat) {
            metrics.beat.beat_count++;
            uint32_t now = millis();
            if (metrics.beat.last_beat_time > 0) {
                float interval = now - metrics.beat.last_beat_time;
                float expected = 60000.0f / tempo;
                float variance = abs(interval - expected) / expected;
                metrics.beat.beat_variance = variance / (32 - 1);
            }
            metrics.beat.last_beat_time = now;
        }
    }

    void updateAGCMetrics(float gain, float target, float noise, const char* state) {
        metrics.agc.current_gain = gain;
        metrics.agc.target_gain = target;
        metrics.agc.noise_floor = noise;
        metrics.agc.state = state;
        
        static const char* last_state = "SILENT";
        if (strcmp(state, last_state) != 0) {
            metrics.agc.state_changes++;
            last_state = state;
        }
        
        metrics.agc.compression_ratio = target > 0 ? gain / target : 1.0f;
    }

    void updatePerformanceMetrics(uint32_t analysis_us, uint32_t feature_us) {
        metrics.perf.analysis_time_us = analysis_us;
        metrics.perf.feature_time_us = feature_us;
    }

    // Getters
    const CompleteMetrics& getMetrics() const { return metrics; }
    float getAverageEnergy() const { return avg_energy; }
    float getAverageTempo() const { return avg_tempo; }
    float getAverageCPU() const { return avg_cpu; }

    // Pretty print functions
    void printSummary() {
        Serial.println("\n--- AP DIAGNOSTIC PROBE ---");
        Serial.println("\n=== AUDIO METRICS SUMMARY ===");
        Serial.printf("Frame: %u, Time: %ums\n\n", metrics.frame_number, metrics.timestamp);

        Serial.println("[RAW AUDIO]");
        Serial.printf("  DC Offset: %d, RMS: %.1f, Peak: %.1f\n", 
            metrics.raw.dc_offset, metrics.raw.rms, metrics.raw.peak_amplitude);
        Serial.printf("  Dynamic Range: %.1f dB, Clips: %u\n\n", 
            metrics.raw.dynamic_range_db, metrics.raw.clipping_count);

        Serial.println("[FREQUENCY]");
        Serial.printf("  Total Energy: %.1f (avg: %.1f)\n", metrics.freq.total_energy, avg_energy);
        Serial.printf("  Bass: %.1f, Mid: %.1f, High: %.1f\n", 
            metrics.freq.bass_energy, metrics.freq.mid_energy, metrics.freq.high_energy);
        Serial.printf("  Dominant: %.1f Hz (bin %d), Centroid: %.1f Hz\n\n", 
            metrics.freq.dominant_frequency, metrics.freq.dominant_bin, metrics.freq.spectral_centroid);
            
        Serial.println("[BEAT DETECTION]");
        Serial.printf("  Beat: %s, Strength: %.2f, Tempo: %.1f BPM\n", 
            metrics.beat.beat_detected ? "YES" : "NO", metrics.beat.beat_strength, metrics.beat.tempo_bpm);
        Serial.printf("  Total Beats: %u, Variance: %.2f\n\n", 
            metrics.beat.beat_count, metrics.beat.beat_variance);

        Serial.println("[AGC]");
        Serial.printf("  State: %s, Gain: %.2f/%.2f, Noise: %.1f\n\n",
            metrics.agc.state, metrics.agc.current_gain, metrics.agc.target_gain, metrics.agc.noise_floor);
        
        Serial.println("[PERFORMANCE]");
        Serial.printf("  Processing: %u us (%.1f%% CPU), FPS: %.1f\n", 
            metrics.perf.processing_time_us, avg_cpu, metrics.perf.fps);
        Serial.printf("  Analysis: %u us, Features: %u us\n", 
            metrics.perf.analysis_time_us, metrics.perf.feature_time_us);
        Serial.println("=============================\n");
        Serial.println("---------------------------\n");
    }

    void printCompact() {
        Serial.printf("BPM: %5.1f (Conf: %.2f) | Onset: %6.4f | Energy: %5.1f | Gain: %.2f\n",
            metrics.beat.tempo_bpm,
            metrics.beat.beat_confidence,
            metrics.beat.onset_strength,
            metrics.freq.total_energy,
            metrics.agc.current_gain
        );
    }
};