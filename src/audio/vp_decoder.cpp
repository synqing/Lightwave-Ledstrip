#include "../config/features.h"

#if FEATURE_AUDIO_SYNC
#include "vp_decoder.h"
#include <ArduinoJson.h>

// Global frequency optimizer instance
extern FrequencyBinOptimizer g_freq_bin_optimizer;

VPDecoder::VPDecoder() : playing(false), start_time_ms(0), file_mode(false), last_load_time_ms(0) {
    // Initialize empty musical data
    musical_data.duration_ms = 0;
    musical_data.bpm = 120;
    // Initialize synthetic frequency bins to zero
    memset(synthetic_frequency_bins, 0, sizeof(synthetic_frequency_bins));
    // Use global frequency optimizer for efficiency
    freq_optimizer = &g_freq_bin_optimizer;
    // SPIFFS will be initialized later in setup()
}

bool VPDecoder::loadFromJson(const String& json_data) {
    file_mode = false;
    return parseJson(json_data);
}

bool VPDecoder::loadFromFile(const String& file_path) {
    Serial.printf("🎵 VP Decoder: Loading large JSON file: %s\n", file_path.c_str());
    
    // Ensure SPIFFS is initialized
    if (!SPIFFS.begin(true)) {
        Serial.println("❌ VP Decoder: SPIFFS initialization failed");
        return false;
    }
    
    // Check if file exists
    if (!SPIFFS.exists(file_path)) {
        Serial.printf("❌ VP Decoder: File not found: %s\n", file_path.c_str());
        return false;
    }
    
    // Get file size
    File file = SPIFFS.open(file_path, "r");
    if (!file) {
        Serial.printf("❌ VP Decoder: Cannot open file: %s\n", file_path.c_str());
        return false;
    }
    
    size_t file_size = file.size();
    file.close();
    
    Serial.printf("📁 File size: %.2f MB\n", file_size / 1024.0f / 1024.0f);
    
    // Enable file mode and store path
    file_mode = true;
    current_file_path = file_path;
    last_load_time_ms = 0;
    
    // Clear existing data
    clearData();
    
    // Parse metadata and initial data window
    return streamParseFile(file_path);
}

void VPDecoder::startPlayback() {
    playing = true;
    start_time_ms = millis();
    Serial.println("🎵 VP Decoder: Playback started");
}

void VPDecoder::stopPlayback() {
    playing = false;
    Serial.println("⏹️ VP Decoder: Playback stopped");
}

float VPDecoder::getCurrentTime() const {
    if (!playing) return 0;
    return millis() - start_time_ms;
}

AudioFrame VPDecoder::getCurrentFrame() {
    AudioFrame frame;
    
    // Initialize frame with synthetic frequency bins
    frame.frequency_bins = synthetic_frequency_bins;
    
    if (!playing) {
        frame.silence = true;
        frame.bass_energy = 0;
        frame.mid_energy = 0;
        frame.high_energy = 0;
        frame.total_energy = 0;
        frame.transient_detected = false;
        // Clear frequency bins
        memset(synthetic_frequency_bins, 0, sizeof(synthetic_frequency_bins));
        return frame;
    }
    
    float current_time = getCurrentTime();
    
    // Check if we've reached the end
    if (current_time > musical_data.duration_ms) {
        frame.silence = true;
        frame.bass_energy = 0;
        frame.mid_energy = 0;
        frame.high_energy = 0;
        frame.total_energy = 0;
        frame.transient_detected = false;
        memset(synthetic_frequency_bins, 0, sizeof(synthetic_frequency_bins));
        return frame;
    }
    
    // For file mode, check if we need to load more data
    if (file_mode && needsDataRefresh(current_time)) {
        loadDataWindow(current_time);
    }
    
    // Interpolate intensity values for each frequency band
    float bass_intensity, mid_intensity, high_intensity;
    
    if (file_mode) {
        // Use sliding buffer interpolation for streaming data
        bass_intensity = interpolateFromBuffer(musical_data.bass, current_time);
        mid_intensity = interpolateFromBuffer(musical_data.mid, current_time);
        high_intensity = interpolateFromBuffer(musical_data.high, current_time);
    } else {
        // Use legacy vector interpolation for small JSON strings
        bass_intensity = interpolateIntensity(musical_data.bass.getData(), current_time);
        mid_intensity = interpolateIntensity(musical_data.mid.getData(), current_time);
        high_intensity = interpolateIntensity(musical_data.high.getData(), current_time);
    }
    
    // Synthesize frequency bins from the intensity values
    synthesizeFrequencyBins(bass_intensity, mid_intensity, high_intensity);
    
    // Map intensities to energy values (scale up for better visual impact)
    frame.bass_energy = bass_intensity * 1000.0f;
    frame.mid_energy = mid_intensity * 800.0f;
    frame.high_energy = high_intensity * 600.0f;
    frame.total_energy = frame.bass_energy + frame.mid_energy + frame.high_energy;
    
    // Detect transients (sudden increases in energy)
    static float last_total_energy = 0;
    float energy_delta = frame.total_energy - last_total_energy;
    frame.transient_detected = (energy_delta > 200.0f);
    last_total_energy = frame.total_energy;
    
    frame.silence = (frame.total_energy < 10.0f);
    
    return frame;
}

bool VPDecoder::isOnBeat() const {
    if (!playing) return false;
    
    float current_time = getCurrentTime();
    
    // Check if we're within 50ms of any beat
    for (float beat_time : musical_data.beat_grid_ms) {
        if (abs(current_time - beat_time) < 50.0f) {
            return true;
        }
    }
    return false;
}

float VPDecoder::interpolateIntensity(const std::vector<MusicalDataPoint>& data, float time_ms) {
    if (data.empty()) return 0.0f;
    
    // Find the two data points we're between
    for (size_t i = 0; i < data.size() - 1; i++) {
        if (time_ms >= data[i].time_ms && time_ms <= data[i + 1].time_ms) {
            // Linear interpolation
            float t = (time_ms - data[i].time_ms) / (data[i + 1].time_ms - data[i].time_ms);
            return data[i].intensity + t * (data[i + 1].intensity - data[i].intensity);
        }
    }
    
    // If before first point, return first intensity
    if (time_ms < data[0].time_ms) {
        return data[0].intensity;
    }
    
    // If after last point, return last intensity
    return data.back().intensity;
}

bool VPDecoder::parseJson(const String& json_data) {
    // For ESP32, we need to be careful with memory usage
    // Create a reasonable buffer size for our reduced JSON
    DynamicJsonDocument doc(8192);
    
    DeserializationError error = deserializeJson(doc, json_data);
    if (error) {
        Serial.printf("❌ VP Decoder: JSON parse failed: %s\n", error.c_str());
        return false;
    }
    
    // Parse global metrics
    if (doc["global_metrics"]) {
        musical_data.duration_ms = doc["global_metrics"]["duration_ms"];
        musical_data.bpm = doc["global_metrics"]["bpm"];
    }
    
    // Parse beat grid
    musical_data.beat_grid_ms.clear();
    if (doc["layers"]["rhythm"]["beat_grid_ms"]) {
        JsonArray beats = doc["layers"]["rhythm"]["beat_grid_ms"];
        for (JsonVariant beat : beats) {
            musical_data.beat_grid_ms.push_back(beat.as<float>());
        }
    }
    
    // Parse bass frequency data
    musical_data.bass.clear();
    if (doc["layers"]["frequency"]["bass"]) {
        JsonArray bass_array = doc["layers"]["frequency"]["bass"];
        for (JsonVariant point : bass_array) {
            MusicalDataPoint data_point;
            data_point.time_ms = point["time_ms"];
            data_point.intensity = point["intensity"];
            musical_data.bass.addPoint(data_point);
        }
    }
    
    // Synthesize mid and high frequency bands from bass data
    synthesizeFrequencyBands();
    
    Serial.printf("✅ VP Decoder: Loaded musical data\n");
    Serial.printf("   Duration: %.1fs, BPM: %d\n", musical_data.duration_ms / 1000.0f, musical_data.bpm);
    Serial.printf("   Beats: %d, Bass points: %d\n", 
                  musical_data.beat_grid_ms.size(), musical_data.bass.size());
    
    return true;
}

void VPDecoder::synthesizeFrequencyBands() {
    // Legacy method for small JSON strings
    if (!file_mode) {
        musical_data.mid.clear();
        musical_data.high.clear();
        
        for (const auto& bass_point : musical_data.bass.getData()) {
            // Mid frequencies: Complementary to bass (when bass is low, mids are higher)
            MusicalDataPoint mid_point;
            mid_point.time_ms = bass_point.time_ms;
            mid_point.intensity = 0.3f + (1.0f - bass_point.intensity) * 0.5f;
            musical_data.mid.addPoint(mid_point);
            
            // High frequencies: More erratic, based on bass but with noise
            MusicalDataPoint high_point;
            high_point.time_ms = bass_point.time_ms;
            float noise = (sin(bass_point.time_ms * 0.01f) + 1.0f) * 0.5f;
            high_point.intensity = bass_point.intensity * 0.7f + noise * 0.3f;
            musical_data.high.addPoint(high_point);
        }
        
        Serial.printf("   Synthesized: Mid points: %d, High points: %d\n",
                      musical_data.mid.size(), musical_data.high.size());
    }
}

    Serial.println("🔄 VP Decoder: Starting streaming parse...");
    
    File file = SPIFFS.open(file_path, "r");
    if (!file) {
        Serial.println("❌ Failed to open file for streaming");
        return false;
    }
    
    // First pass: Extract metadata only
    DynamicJsonDocument meta_doc(4096);
    String metadata_chunk = "";
    
    // Read first chunk to get metadata - need more for beat grid
    while (file.available() && metadata_chunk.length() < 8000) {
        metadata_chunk += char(file.read());
    }
    Serial.printf("📄 Read %d bytes for metadata\n", metadata_chunk.length());
    
    DeserializationError error = deserializeJson(meta_doc, metadata_chunk);
    if (error) {
        Serial.printf("❌ Metadata parse error: %s\n", error.c_str());
    } else {
        Serial.println("✅ Metadata parsed successfully");
        
        if (meta_doc["global_metrics"]) {
            musical_data.duration_ms = meta_doc["global_metrics"]["duration_ms"];
            musical_data.bpm = meta_doc["global_metrics"]["bpm"];
            
            Serial.printf("✅ Metadata extracted: Duration=%.1fs, BPM=%d\n", 
                          musical_data.duration_ms / 1000.0f, musical_data.bpm);
        } else {
            Serial.println("❌ No global_metrics found in metadata");
        }
        
        // Also load beat grid from metadata
        if (meta_doc["layers"]["rhythm"]["beat_grid_ms"]) {
            JsonArray beats = meta_doc["layers"]["rhythm"]["beat_grid_ms"];
            for (JsonVariant beat : beats) {
                musical_data.beat_grid_ms.push_back(beat.as<float>());
            }
            Serial.printf("✅ Loaded %d beats\n", musical_data.beat_grid_ms.size());
        }
    }
    
    file.close();
    
    // Load initial data window (first 30 seconds)
    return loadDataWindow(0);
}

bool VPDecoder::loadDataWindow(float target_time_ms) {
    if (!file_mode) return true;
    
    Serial.printf("📊 Loading data window around %.1fs\n", target_time_ms / 1000.0f);
    
    File file = SPIFFS.open(current_file_path, "r");
    if (!file) return false;
    
    char buffer[StreamingConfig::STREAM_CHUNK_SIZE];
    String current_chunk = "";
    bool in_data_section = false;
    float window_start = target_time_ms - StreamingConfig::TIME_WINDOW_MS / 2;
    float window_end = target_time_ms + StreamingConfig::TIME_WINDOW_MS / 2;
    
    // Clear existing data outside the window
    musical_data.bass.clear();
    musical_data.mid.clear();
    musical_data.high.clear();
    
    while (file.available()) {
        size_t bytes_read = file.readBytes(buffer, StreamingConfig::STREAM_CHUNK_SIZE);
        current_chunk += String(buffer).substring(0, bytes_read);
        
        // Process complete JSON objects in the chunk
        processJsonChunk(current_chunk, window_start, window_end);
        
        // Keep partial JSON at end of chunk for next iteration
        int last_brace = current_chunk.lastIndexOf('}');
        if (last_brace > 0) {
            current_chunk = current_chunk.substring(last_brace + 1);
        }
        
        // Yield to prevent watchdog timeout
        yield();
    }
    
    file.close();
    last_load_time_ms = target_time_ms;
    
    Serial.printf("✅ Loaded data: Bass=%d, Mid=%d, High=%d points\n",
                  musical_data.bass.size(), musical_data.mid.size(), musical_data.high.size());
    
    // If we don't have mid/high data, synthesize from bass
    if (musical_data.mid.size() == 0 && musical_data.bass.size() > 0) {
        synthesizeFrequencyBands();
    }
    
    return true;
}

bool VPDecoder::needsDataRefresh(float current_time_ms) {
    if (!file_mode) return false;
    
    // Refresh when we're getting close to the edge of our loaded window
    float time_since_load = abs(current_time_ms - last_load_time_ms);
    return time_since_load > (StreamingConfig::TIME_WINDOW_MS * 0.6f);
}

void VPDecoder::processJsonChunk(const String& chunk, float start_time_ms, float end_time_ms) {
    // Simple pattern matching for data points within our time window
    // This is a lightweight parser that doesn't require full JSON parsing
    
    int pos = 0;
    while (pos < chunk.length()) {
        // Look for time_ms pattern
        int time_pos = chunk.indexOf("\"time_ms\"", pos);
        if (time_pos == -1) break;
        
        // Extract time value
        int colon_pos = chunk.indexOf(':', time_pos);
        int comma_pos = chunk.indexOf(',', colon_pos);
        if (colon_pos == -1 || comma_pos == -1) {
            pos = time_pos + 1;
            continue;
        }
        
        float time_ms = chunk.substring(colon_pos + 1, comma_pos).toFloat();
        
        // Check if this point is in our window
        if (time_ms >= start_time_ms && time_ms <= end_time_ms) {
            // Look for intensity value
            int intensity_pos = chunk.indexOf("\"intensity\"", comma_pos);
            if (intensity_pos > 0 && intensity_pos < time_pos + 200) {
                int intensity_colon = chunk.indexOf(':', intensity_pos);
                int intensity_end = chunk.indexOf('\n', intensity_colon);
                if (intensity_end == -1) intensity_end = chunk.indexOf('}', intensity_colon);
                
                if (intensity_colon != -1 && intensity_end != -1) {
                    float intensity = chunk.substring(intensity_colon + 1, intensity_end).toFloat();
                    
                    // Determine which frequency band this belongs to
                    String context = chunk.substring(max(0, time_pos - 100), time_pos);
                    MusicalDataPoint point = {time_ms, intensity};
                    
                    if (context.indexOf("\"bass\"") >= 0) {
                        musical_data.bass.addPoint(point);
                    } else if (context.indexOf("\"mids\"") >= 0) {
                        musical_data.mid.addPoint(point);
                    } else if (context.indexOf("\"highs\"") >= 0) {
                        musical_data.high.addPoint(point);
                    }
                }
            }
        }
        
        pos = time_pos + 1;
    }
}

float VPDecoder::interpolateFromBuffer(const SlidingDataBuffer<MusicalDataPoint>& buffer, float time_ms) {
    const auto& data = buffer.getData();
    if (data.empty()) return 0.0f;
    
    // Find the two data points we're between
    for (size_t i = 0; i < data.size() - 1; i++) {
        if (time_ms >= data[i].time_ms && time_ms <= data[i + 1].time_ms) {
            // Linear interpolation
            float t = (time_ms - data[i].time_ms) / (data[i + 1].time_ms - data[i].time_ms);
            return data[i].intensity + t * (data[i + 1].intensity - data[i].intensity);
        }
    }
    
    // If before first point, return first intensity
    if (time_ms < data[0].time_ms) {
        return data[0].intensity;
    }
    
    // If after last point, return last intensity
    return data.back().intensity;
}

void VPDecoder::clearData() {
    musical_data.bass.clear();
    musical_data.mid.clear();
    musical_data.high.clear();
    musical_data.beat_grid_ms.clear();
}

size_t VPDecoder::getMemoryUsage() const {
    size_t usage = 0;
    usage += musical_data.bass.size() * sizeof(MusicalDataPoint);
    usage += musical_data.mid.size() * sizeof(MusicalDataPoint);
    usage += musical_data.high.size() * sizeof(MusicalDataPoint);
    usage += musical_data.beat_grid_ms.size() * sizeof(float);
    usage += sizeof(synthetic_frequency_bins);
    return usage;
}

void VPDecoder::synthesizeFrequencyBins(float bass, float mid, float high) {
    // Use optimized frequency bin generator for better performance
    if (freq_optimizer) {
        freq_optimizer->synthesizeFromIntensities(synthetic_frequency_bins, 
                                                 bass, mid, high, millis());
    } else {
        // Fallback to simple generation if optimizer not available
        for (int i = 0; i < FFT_BIN_COUNT; i++) {
            if (i < 32) {
                synthetic_frequency_bins[i] = bass * (1.0f - i / 64.0f);
            } else if (i < 64) {
                synthetic_frequency_bins[i] = mid * 0.8f;
            } else {
                synthetic_frequency_bins[i] = high * 0.6f;
            }
        }
    }
}

#endif // FEATURE_AUDIO_SYNC
