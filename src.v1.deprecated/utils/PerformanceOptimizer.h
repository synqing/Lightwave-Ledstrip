#ifndef PERFORMANCE_OPTIMIZER_H
#define PERFORMANCE_OPTIMIZER_H

#include <Arduino.h>
#include "config/features.h"
#include "config/hardware_config.h"

#if FEATURE_PERFORMANCE_MONITOR

// Performance Level Enumeration
enum class PerformanceLevel : uint8_t {
    PERFORMANCE_FULL = 0,      // 120 FPS, full CENTER ORIGIN calculations
    PERFORMANCE_MEDIUM = 1,    // 60 FPS, reduced CENTER ORIGIN precision
    PERFORMANCE_LOW = 2,       // 30 FPS, minimal CENTER ORIGIN calculations
    PERFORMANCE_MINIMAL = 3    // 15 FPS, basic CENTER ORIGIN effects only
};

// CENTER ORIGIN Performance Statistics
struct CenterOriginStats {
    uint32_t total_calculations = 0;
    uint32_t frame_time_microseconds = 0;
    uint32_t max_frame_time = 0;
    uint32_t min_frame_time = UINT32_MAX;
    float avg_frame_time = 0.0f;
    uint32_t fps_target = 120;
    uint32_t fps_actual = 0;
    bool adaptive_mode = true;
};

// Performance Optimizer Class for CENTER ORIGIN Effects
class PerformanceOptimizer {
private:
    // Performance tracking
    uint32_t frame_times[10] = {0};
    uint8_t frame_index = 0;
    uint32_t last_frame_time = 0;
    uint32_t frame_start_time = 0;
    
    // Performance state
    PerformanceLevel current_level = PerformanceLevel::PERFORMANCE_FULL;
    CenterOriginStats stats;
    
    // Thresholds (microseconds)
    static constexpr uint32_t TARGET_FRAME_TIME_120FPS = 8333;   // 120 FPS = 8.33ms
    static constexpr uint32_t TARGET_FRAME_TIME_60FPS = 16666;   // 60 FPS = 16.66ms
    static constexpr uint32_t TARGET_FRAME_TIME_30FPS = 33333;   // 30 FPS = 33.33ms
    static constexpr uint32_t TARGET_FRAME_TIME_15FPS = 66666;   // 15 FPS = 66.66ms
    
    // Adaptive thresholds
    uint32_t performance_degradation_threshold = TARGET_FRAME_TIME_120FPS + 2000; // 10.33ms
    uint32_t performance_recovery_threshold = TARGET_FRAME_TIME_120FPS - 1000;    // 7.33ms
    
public:
    PerformanceOptimizer() {
        stats.fps_target = 120;
        stats.adaptive_mode = true;
    }
    
    // Begin frame timing for CENTER ORIGIN effects
    void beginFrame() {
        frame_start_time = micros();
    }
    
    // End frame timing and update performance statistics
    void endFrame() {
        uint32_t frame_duration = micros() - frame_start_time;
        
        // Update frame time history
        frame_times[frame_index] = frame_duration;
        frame_index = (frame_index + 1) % 10;
        
        // Update statistics
        stats.total_calculations++;
        stats.frame_time_microseconds = frame_duration;
        
        if (frame_duration > stats.max_frame_time) {
            stats.max_frame_time = frame_duration;
        }
        if (frame_duration < stats.min_frame_time) {
            stats.min_frame_time = frame_duration;
        }
        
        // Calculate average frame time
        uint32_t total_time = 0;
        for (uint8_t i = 0; i < 10; i++) {
            total_time += frame_times[i];
        }
        stats.avg_frame_time = (float)total_time / 10.0f;
        
        // Calculate actual FPS
        if (stats.avg_frame_time > 0) {
            stats.fps_actual = 1000000 / stats.avg_frame_time;
        }
        
        // Adaptive performance adjustment
        if (stats.adaptive_mode) {
            adjustPerformanceLevel();
        }
    }
    
    // Adaptive Performance Level Adjustment
    void adjustPerformanceLevel() {
        uint32_t avg_time = (uint32_t)stats.avg_frame_time;
        
        // Only adjust if we have enough samples
        if (stats.total_calculations < 10) return;
        
        switch (current_level) {
            case PerformanceLevel::PERFORMANCE_FULL:
                if (avg_time > performance_degradation_threshold) {
                    setPerformanceLevel(PerformanceLevel::PERFORMANCE_MEDIUM);
                    Serial.println("Performance: Degraded to MEDIUM due to slow CENTER ORIGIN calculations");
                }
                break;
                
            case PerformanceLevel::PERFORMANCE_MEDIUM:
                if (avg_time > TARGET_FRAME_TIME_60FPS + 5000) { // 21.66ms
                    setPerformanceLevel(PerformanceLevel::PERFORMANCE_LOW);
                    Serial.println("Performance: Degraded to LOW due to continued slowdown");
                } else if (avg_time < performance_recovery_threshold) {
                    setPerformanceLevel(PerformanceLevel::PERFORMANCE_FULL);
                    Serial.println("Performance: Restored to FULL - CENTER ORIGIN calculations optimized");
                }
                break;
                
            case PerformanceLevel::PERFORMANCE_LOW:
                if (avg_time > TARGET_FRAME_TIME_30FPS + 10000) { // 43.33ms
                    setPerformanceLevel(PerformanceLevel::PERFORMANCE_MINIMAL);
                    Serial.println("Performance: Degraded to MINIMAL due to severe slowdown");
                } else if (avg_time < TARGET_FRAME_TIME_60FPS - 2000) { // 14.66ms
                    setPerformanceLevel(PerformanceLevel::PERFORMANCE_MEDIUM);
                    Serial.println("Performance: Improved to MEDIUM");
                }
                break;
                
            case PerformanceLevel::PERFORMANCE_MINIMAL:
                if (avg_time < TARGET_FRAME_TIME_30FPS - 5000) { // 28.33ms
                    setPerformanceLevel(PerformanceLevel::PERFORMANCE_LOW);
                    Serial.println("Performance: Improved to LOW");
                }
                break;
        }
    }
    
    // Set Performance Level Manually
    void setPerformanceLevel(PerformanceLevel level) {
        current_level = level;
        
        // Update target FPS based on performance level
        switch (level) {
            case PerformanceLevel::PERFORMANCE_FULL:
                stats.fps_target = 120;
                break;
            case PerformanceLevel::PERFORMANCE_MEDIUM:
                stats.fps_target = 60;
                break;
            case PerformanceLevel::PERFORMANCE_LOW:
                stats.fps_target = 30;
                break;
            case PerformanceLevel::PERFORMANCE_MINIMAL:
                stats.fps_target = 15;
                break;
        }
    }
    
    // CENTER ORIGIN Calculation Control
    bool shouldCalculateCenterOrigin() const {
        switch (current_level) {
            case PerformanceLevel::PERFORMANCE_FULL:
                return true;
            case PerformanceLevel::PERFORMANCE_MEDIUM:
                return (millis() % 32) == 0; // Every 32ms (30Hz)
            case PerformanceLevel::PERFORMANCE_LOW:
                return (millis() % 64) == 0; // Every 64ms (15Hz)
            case PerformanceLevel::PERFORMANCE_MINIMAL:
                return (millis() % 128) == 0; // Every 128ms (8Hz)
        }
        return true;
    }
    
    // CENTER ORIGIN Detail Level Control
    uint8_t getCenterOriginDetail() const {
        switch (current_level) {
            case PerformanceLevel::PERFORMANCE_FULL:
                return 100; // Full 160-LED precision
            case PerformanceLevel::PERFORMANCE_MEDIUM:
                return 75;  // 120-LED precision with interpolation
            case PerformanceLevel::PERFORMANCE_LOW:
                return 50;  // 80-LED precision with interpolation
            case PerformanceLevel::PERFORMANCE_MINIMAL:
                return 25;  // 40-LED precision with interpolation
        }
        return 100;
    }
    
    // Frame Rate Limiting for CENTER ORIGIN Effects
    bool shouldRenderFrame() const {
        uint32_t target_interval = 1000000 / stats.fps_target; // microseconds
        return (micros() - last_frame_time) >= target_interval;
    }
    
    // Update last frame time
    void updateFrameTime() {
        last_frame_time = micros();
    }
    
    // CENTER ORIGIN Distance Calculation Optimization
    float getOptimizedDistanceFromCenter(uint16_t led_index) const {
        // Pre-calculated distance for performance
        static bool distances_calculated = false;
        static float distances[HardwareConfig::NUM_LEDS];
        
        if (!distances_calculated) {
            for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
                distances[i] = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
            }
            distances_calculated = true;
        }
        
        return (led_index < HardwareConfig::NUM_LEDS) ? distances[led_index] : 0.0f;
    }
    
    // CENTER ORIGIN Intensity Calculation with Performance Scaling
    uint8_t getOptimizedCenterOriginIntensity(uint16_t led_index, uint8_t base_intensity) const {
        float distance = getOptimizedDistanceFromCenter(led_index);
        uint8_t detail_level = getCenterOriginDetail();
        
        // Scale calculation complexity based on performance level
        float intensity_factor = 1.0f - (distance / HardwareConfig::STRIP_HALF_LENGTH);
        intensity_factor *= (detail_level / 100.0f);
        
        return (uint8_t)(base_intensity * max(intensity_factor, 0.1f));
    }
    
    // Performance Statistics
    const CenterOriginStats& getStats() const {
        return stats;
    }
    
    PerformanceLevel getPerformanceLevel() const {
        return current_level;
    }
    
    // Enable/Disable Adaptive Mode
    void setAdaptiveMode(bool enabled) {
        stats.adaptive_mode = enabled;
    }
    
    // Generate Performance Report
    void generatePerformanceReport() const {
        Serial.println("\\n=== CENTER ORIGIN Performance Report ===");
        Serial.printf("Performance Level: %d\\n", (int)current_level);
        Serial.printf("Target FPS: %d, Actual FPS: %d\\n", stats.fps_target, stats.fps_actual);
        Serial.printf("Avg Frame Time: %.2f μs\\n", stats.avg_frame_time);
        Serial.printf("Min/Max Frame Time: %d / %d μs\\n", stats.min_frame_time, stats.max_frame_time);
        Serial.printf("Total Calculations: %d\\n", stats.total_calculations);
        Serial.printf("CENTER ORIGIN Detail: %d%%\\n", getCenterOriginDetail());
        Serial.printf("Adaptive Mode: %s\\n", stats.adaptive_mode ? "ENABLED" : "DISABLED");
        Serial.println("=======================================");
    }
};

// Global performance optimizer instance
extern PerformanceOptimizer g_performanceOptimizer;

// Performance optimization macros for CENTER ORIGIN effects
#define CENTER_ORIGIN_BEGIN_FRAME() g_performanceOptimizer.beginFrame()
#define CENTER_ORIGIN_END_FRAME() g_performanceOptimizer.endFrame()

#define CENTER_ORIGIN_SHOULD_CALCULATE() g_performanceOptimizer.shouldCalculateCenterOrigin()
#define CENTER_ORIGIN_GET_DETAIL() g_performanceOptimizer.getCenterOriginDetail()

#define CENTER_ORIGIN_DISTANCE(led) g_performanceOptimizer.getOptimizedDistanceFromCenter(led)
#define CENTER_ORIGIN_INTENSITY(led, base) g_performanceOptimizer.getOptimizedCenterOriginIntensity(led, base)

#define CENTER_ORIGIN_SHOULD_RENDER() g_performanceOptimizer.shouldRenderFrame()
#define CENTER_ORIGIN_UPDATE_FRAME_TIME() g_performanceOptimizer.updateFrameTime()

#else // !FEATURE_PERFORMANCE_MONITOR

// Stub definitions when performance monitoring is disabled
class PerformanceOptimizer {
public:
    void beginFrame() {}
    void endFrame() {}
    bool shouldCalculateCenterOrigin() const { return true; }
    uint8_t getCenterOriginDetail() const { return 100; }
    bool shouldRenderFrame() const { return true; }
    void updateFrameTime() {}
    float getOptimizedDistanceFromCenter(uint16_t led_index) const { 
        return abs((float)led_index - HardwareConfig::STRIP_CENTER_POINT); 
    }
    uint8_t getOptimizedCenterOriginIntensity(uint16_t led_index, uint8_t base_intensity) const { 
        return base_intensity; 
    }
    void generatePerformanceReport() const {}
};

extern PerformanceOptimizer g_performanceOptimizer;

#define CENTER_ORIGIN_BEGIN_FRAME()
#define CENTER_ORIGIN_END_FRAME()
#define CENTER_ORIGIN_SHOULD_CALCULATE() true
#define CENTER_ORIGIN_GET_DETAIL() 100
#define CENTER_ORIGIN_DISTANCE(led) (abs((float)(led) - HardwareConfig::STRIP_CENTER_POINT))
#define CENTER_ORIGIN_INTENSITY(led, base) (base)
#define CENTER_ORIGIN_SHOULD_RENDER() true
#define CENTER_ORIGIN_UPDATE_FRAME_TIME()

#endif // FEATURE_PERFORMANCE_MONITOR

#endif // PERFORMANCE_OPTIMIZER_H