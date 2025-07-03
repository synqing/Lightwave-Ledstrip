#pragma once

#include <Arduino.h>
#include <vector>
#include <FS.h>
#include <SPIFFS.h>
#include "audio_frame.h"
#include "frequency_bin_optimizer.h"

/**
 * Musical Data Point from JSON
 */
struct MusicalDataPoint {
    float time_ms;
    float intensity;
};

/**
 * Streaming parser configuration for large JSON files
 */
struct StreamingConfig {
    static const size_t STREAM_CHUNK_SIZE = 4096;      // Read 4KB chunks from file
    static const size_t MAX_DATA_POINTS = 10000;       // Limit in-memory points per band
    static const size_t PARSER_BUFFER = 8192;          // JSON parser buffer size
    static constexpr float TIME_WINDOW_MS = 30000.0f;  // 30s sliding window
};

/**
 * Circular buffer for data points with time-based sliding window
 * Keeps only recent data points in memory for large files
 */
template<typename T>
class SlidingDataBuffer {
private:
    std::vector<T> buffer;
    size_t max_size;
    float time_window_ms;
    
public:
    SlidingDataBuffer(size_t max_sz, float window_ms) 
        : max_size(max_sz), time_window_ms(window_ms) {
        buffer.reserve(max_size);
    }
    
    void addPoint(const T& point) {
        buffer.push_back(point);
        
        // Remove old points outside time window
        float current_time = point.time_ms;
        while (!buffer.empty() && 
               (current_time - buffer.front().time_ms) > time_window_ms) {
            buffer.erase(buffer.begin());
        }
        
        // Enforce size limit to prevent memory overflow
        while (buffer.size() > max_size) {
            buffer.erase(buffer.begin());
        }
    }
    
    const std::vector<T>& getData() const { return buffer; }
    void clear() { buffer.clear(); }
    size_t size() const { return buffer.size(); }
    bool empty() const { return buffer.empty(); }
};

/**
 * Enhanced Musical Data with streaming support for large files
 */
struct MusicalData {
    // Metadata
    float duration_ms;
    int bpm;
    
    // Beat grid (kept in full memory - usually small)
    std::vector<float> beat_grid_ms;
    
    // Streaming frequency data with sliding windows
    SlidingDataBuffer<MusicalDataPoint> bass;
    SlidingDataBuffer<MusicalDataPoint> mid;
    SlidingDataBuffer<MusicalDataPoint> high;
    
    MusicalData() : bass(StreamingConfig::MAX_DATA_POINTS, StreamingConfig::TIME_WINDOW_MS),
                    mid(StreamingConfig::MAX_DATA_POINTS, StreamingConfig::TIME_WINDOW_MS),
                    high(StreamingConfig::MAX_DATA_POINTS, StreamingConfig::TIME_WINDOW_MS) {}
};

/**
 * VP Decoder - Transforms JSON musical data into AudioFrame structures
 * 
 * Enhanced version supporting large JSON files (15-20MB) through streaming parsing
 * This class acts as a "musical data interpreter" that reads pre-processed
 * audio intelligence and feeds it to the Visual Pipeline in real-time.
 * 
 * IMPORTANT: This decoder generates synthetic frequency bins to match the
 * AudioFrame interface expected by the visual pipeline.
 */
class VPDecoder {
private:
    // Synthetic frequency bins for AudioFrame compatibility
    float synthetic_frequency_bins[FFT_BIN_COUNT];
    
    // Memory optimization: Use external frequency optimizer
    FrequencyBinOptimizer* freq_optimizer;
    
public:
    VPDecoder();
    
    /**
     * Load musical data from JSON string (legacy - limited to small files)
     */
    bool loadFromJson(const String& json_data);
    
    /**
     * NEW: Load from large JSON file with streaming parser
     * Handles 15-20MB files by loading data progressively
     */
    bool loadFromFile(const String& file_path);
    
    /**
     * Start playback from the beginning
     */
    void startPlayback();
    
    /**
     * Stop playback
     */
    void stopPlayback();
    
    /**
     * Get current AudioFrame based on playback time
     * This is called every frame to get the current musical state
     */
    AudioFrame getCurrentFrame();
    
    /**
     * Check if playback is active
     */
    bool isPlaying() const { return playing; }
    
    /**
     * Get current playback position in milliseconds
     */
    float getCurrentTime() const;
    
    /**
     * Get total duration
     */
    float getDuration() const { return musical_data.duration_ms; }
    
    /**
     * Get BPM
     */
    int getBPM() const { return musical_data.bpm; }
    
    /**
     * Check if we're on a beat
     */
    bool isOnBeat() const;
    
    /**
     * Memory management for large files
     */
    void clearData();
    size_t getMemoryUsage() const;
    
private:
    MusicalData musical_data;
    bool playing;
    unsigned long start_time_ms;
    
    // File streaming state
    String current_file_path;
    bool file_mode;
    float last_load_time_ms;
    
    // Helper methods for frequency synthesis
    void synthesizeFrequencyBins(float bass, float mid, float high);
    
    // Legacy methods (small JSON strings)
    bool parseJson(const String& json_data);
    
    // NEW: Streaming parser methods (large JSON files)
    bool streamParseFile(const String& file_path);
    bool loadDataWindow(float target_time_ms);
    bool needsDataRefresh(float current_time_ms);
    void processJsonChunk(const String& chunk, float start_time_ms, float end_time_ms);
    
    // Interpolation helpers (updated for sliding buffers)
    float interpolateIntensity(const std::vector<MusicalDataPoint>& data, float time_ms);
    float interpolateFromBuffer(const SlidingDataBuffer<MusicalDataPoint>& buffer, float time_ms);
    void synthesizeFrequencyBands();
};