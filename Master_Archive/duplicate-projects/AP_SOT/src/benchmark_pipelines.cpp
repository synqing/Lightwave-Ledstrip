/*
 * Pipeline Performance Benchmark
 * ==============================
 * 
 * Compares legacy monolithic vs pluggable pipeline performance
 * Measures latency, throughput, and memory usage
 */

#include <Arduino.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>

// Legacy system
#include "audio/audio_processing.h"
#include "audio/audio_features.h"
#include "audio/goertzel_engine.h"

// Pluggable system
#include "audio/audio_pipeline.h"
#include "audio/audio_node_factory.h"

// Test configuration
const int TEST_FRAMES = 1000;
const int WARMUP_FRAMES = 100;

// Performance metrics
struct BenchmarkResults {
    const char* name;
    uint64_t total_time_us;
    uint64_t min_time_us;
    uint64_t max_time_us;
    float avg_time_ms;
    float percentile_95_ms;
    float percentile_99_ms;
    uint32_t memory_used;
    bool meets_target;  // <8ms
};

// Test buffers
float test_samples[128];
float freq_bins[96];

// Generate realistic audio signal
void generateRealisticAudio(float* buffer, int size, float time_sec) {
    // Simulate music with varying dynamics
    float beat_phase = fmodf(time_sec * 2.0f, 1.0f);  // 120 BPM
    float kick_env = (beat_phase < 0.1f) ? 1.0f : 0.1f;
    
    for (int i = 0; i < size; i++) {
        float t = time_sec + (float)i / 16000.0f;
        
        // Kick drum (60Hz with harmonics)
        float kick = kick_env * (
            0.8f * sinf(2.0f * M_PI * 60.0f * t) +
            0.3f * sinf(2.0f * M_PI * 120.0f * t)
        );
        
        // Bass line (varying 80-160Hz)
        float bass_freq = 80.0f + 80.0f * sinf(2.0f * M_PI * 0.5f * t);
        float bass = 0.4f * sinf(2.0f * M_PI * bass_freq * t);
        
        // Midrange content (200-2000Hz)
        float mid = 0.3f * sinf(2.0f * M_PI * 440.0f * t) +  // A4
                   0.2f * sinf(2.0f * M_PI * 554.0f * t) +   // C#5
                   0.2f * sinf(2.0f * M_PI * 659.0f * t);    // E5
        
        // High frequency content
        float high = 0.1f * sinf(2.0f * M_PI * 3000.0f * t) +
                    0.1f * sinf(2.0f * M_PI * 5000.0f * t);
        
        // White noise for realism
        float noise = 0.05f * ((float)random(1000) / 500.0f - 1.0f);
        
        // Mix and scale
        buffer[i] = (kick + bass + mid + high + noise) * 8192.0f;
    }
}

// Benchmark legacy pipeline
BenchmarkResults benchmarkLegacy() {
    Serial.println("\n=== BENCHMARKING LEGACY PIPELINE ===");
    
    BenchmarkResults results;
    results.name = "Legacy Monolithic";
    results.total_time_us = 0;
    results.min_time_us = UINT64_MAX;
    results.max_time_us = 0;
    
    // Measure initial memory
    uint32_t mem_before = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    
    // Initialize legacy components
    AudioProcessor processor;
    AudioFeatures features;
    GoertzelEngineGodTier goertzel;
    
    processor.init();
    goertzel.init();
    
    // Array to store all timings for percentile calculation
    uint64_t* timings = new uint64_t[TEST_FRAMES];
    
    // Warmup
    for (int i = 0; i < WARMUP_FRAMES; i++) {
        generateRealisticAudio(test_samples, 128, i * 0.008f);
        processor.process();
    }
    
    // Benchmark
    float time_sec = 0.0f;
    for (int frame = 0; frame < TEST_FRAMES; frame++) {
        generateRealisticAudio(test_samples, 128, time_sec);
        time_sec += 0.008f;
        
        uint64_t start = esp_timer_get_time();
        
        // Legacy processing chain
        processor.process();  // Captures audio, removes DC
        goertzel.processBlock(audio_state.raw_samples, freq_bins);
        features.extract(freq_bins, 96);
        
        uint64_t elapsed = esp_timer_get_time() - start;
        
        timings[frame] = elapsed;
        results.total_time_us += elapsed;
        if (elapsed < results.min_time_us) results.min_time_us = elapsed;
        if (elapsed > results.max_time_us) results.max_time_us = elapsed;
    }
    
    // Calculate statistics
    results.avg_time_ms = (float)results.total_time_us / TEST_FRAMES / 1000.0f;
    
    // Sort timings for percentile calculation
    std::sort(timings, timings + TEST_FRAMES);
    results.percentile_95_ms = timings[(int)(TEST_FRAMES * 0.95)] / 1000.0f;
    results.percentile_99_ms = timings[(int)(TEST_FRAMES * 0.99)] / 1000.0f;
    
    // Memory usage
    uint32_t mem_after = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    results.memory_used = mem_before - mem_after;
    
    results.meets_target = (results.avg_time_ms < 8.0f);
    
    delete[] timings;
    return results;
}

// Benchmark pluggable pipeline
BenchmarkResults benchmarkPluggable() {
    Serial.println("\n=== BENCHMARKING PLUGGABLE PIPELINE ===");
    
    BenchmarkResults results;
    results.name = "Pluggable Pipeline";
    results.total_time_us = 0;
    results.min_time_us = UINT64_MAX;
    results.max_time_us = 0;
    
    // Measure initial memory
    uint32_t mem_before = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    
    // Create pipelines
    AudioPipeline main_pipeline("Main");
    AudioPipeline beat_pipeline("Beat");
    
    // Build pipelines
    main_pipeline.addNode(std::make_shared<DCOffsetNode>());
    main_pipeline.addNode(std::make_shared<GoertzelNode>());
    main_pipeline.addNode(std::make_shared<MultibandAGCNode>());
    main_pipeline.addNode(std::make_shared<ZoneMapperNode>());
    
    beat_pipeline.addNode(std::make_shared<BeatDetectorNode>());
    
    // Configure for testing
    StaticJsonDocument<256> config;
    JsonObject dc_config = config.to<JsonObject>();
    dc_config["mode"] = "fixed";
    dc_config["fixed_offset"] = 0.0f;
    main_pipeline.findNode("DCOffset")->configure(dc_config);
    
    // Array for timings
    uint64_t* timings = new uint64_t[TEST_FRAMES];
    
    // Warmup
    for (int i = 0; i < WARMUP_FRAMES; i++) {
        generateRealisticAudio(test_samples, 128, i * 0.008f);
        main_pipeline.process(test_samples, 128);
    }
    
    // Benchmark
    float time_sec = 0.0f;
    for (int frame = 0; frame < TEST_FRAMES; frame++) {
        generateRealisticAudio(test_samples, 128, time_sec);
        time_sec += 0.008f;
        
        uint64_t start = esp_timer_get_time();
        
        // Pluggable processing chain
        PipelineError error = main_pipeline.process(test_samples, 128);
        
        if (error == PipelineError::NONE) {
            // Dual-path: get RAW data for beat detection
            AudioBuffer* goertzel_out = main_pipeline.getNodeOutput("Goertzel");
            if (goertzel_out) {
                beat_pipeline.process(goertzel_out->data, goertzel_out->size);
            }
        }
        
        uint64_t elapsed = esp_timer_get_time() - start;
        
        timings[frame] = elapsed;
        results.total_time_us += elapsed;
        if (elapsed < results.min_time_us) results.min_time_us = elapsed;
        if (elapsed > results.max_time_us) results.max_time_us = elapsed;
    }
    
    // Calculate statistics
    results.avg_time_ms = (float)results.total_time_us / TEST_FRAMES / 1000.0f;
    
    // Sort for percentiles
    std::sort(timings, timings + TEST_FRAMES);
    results.percentile_95_ms = timings[(int)(TEST_FRAMES * 0.95)] / 1000.0f;
    results.percentile_99_ms = timings[(int)(TEST_FRAMES * 0.99)] / 1000.0f;
    
    // Memory usage
    uint32_t mem_after = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    results.memory_used = mem_before - mem_after;
    
    results.meets_target = (results.avg_time_ms < 8.0f);
    
    delete[] timings;
    return results;
}

// Print comparison results
void printComparison(const BenchmarkResults& legacy, const BenchmarkResults& pluggable) {
    Serial.println("\n=== PERFORMANCE COMPARISON ===");
    Serial.println("                     Legacy    Pluggable   Difference");
    Serial.println("----------------------------------------------------");
    
    Serial.printf("Average (ms):      %7.3f    %7.3f    %+.1f%%\n",
                  legacy.avg_time_ms, pluggable.avg_time_ms,
                  ((pluggable.avg_time_ms - legacy.avg_time_ms) / legacy.avg_time_ms) * 100);
    
    Serial.printf("Minimum (ms):      %7.3f    %7.3f\n",
                  legacy.min_time_us / 1000.0f, pluggable.min_time_us / 1000.0f);
    
    Serial.printf("Maximum (ms):      %7.3f    %7.3f\n",
                  legacy.max_time_us / 1000.0f, pluggable.max_time_us / 1000.0f);
    
    Serial.printf("95th %ile (ms):    %7.3f    %7.3f\n",
                  legacy.percentile_95_ms, pluggable.percentile_95_ms);
    
    Serial.printf("99th %ile (ms):    %7.3f    %7.3f\n",
                  legacy.percentile_99_ms, pluggable.percentile_99_ms);
    
    Serial.printf("Memory (bytes):    %7d    %7d    %+d\n",
                  legacy.memory_used, pluggable.memory_used,
                  (int)pluggable.memory_used - (int)legacy.memory_used);
    
    Serial.println("\n=== TARGET COMPLIANCE (<8ms) ===");
    Serial.printf("Legacy:    %s\n", legacy.meets_target ? "✓ PASS" : "✗ FAIL");
    Serial.printf("Pluggable: %s\n", pluggable.meets_target ? "✓ PASS" : "✗ FAIL");
    
    if (pluggable.avg_time_ms <= legacy.avg_time_ms * 1.1f) {
        Serial.println("\n✓ Pluggable performance is acceptable (within 10% of legacy)");
    } else {
        Serial.println("\n✗ Pluggable performance needs optimization");
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n========================================");
    Serial.println("   AUDIO PIPELINE PERFORMANCE BENCHMARK");
    Serial.println("========================================");
    Serial.printf("Test frames: %d\n", TEST_FRAMES);
    Serial.printf("Target latency: <8ms\n");
    Serial.println("Signal: Realistic music simulation");
    
    // Run benchmarks
    BenchmarkResults legacy_results = benchmarkLegacy();
    delay(1000);  // Let system settle
    
    BenchmarkResults pluggable_results = benchmarkPluggable();
    
    // Print comparison
    printComparison(legacy_results, pluggable_results);
    
    Serial.println("\n=== BENCHMARK COMPLETE ===");
}

void loop() {
    // Nothing to do
    delay(1000);
}