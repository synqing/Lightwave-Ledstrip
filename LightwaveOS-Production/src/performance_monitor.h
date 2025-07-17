#ifndef PERFORMANCE_MONITOR_H
#define PERFORMANCE_MONITOR_H

#include "globals.h"
#include "constants.h"
#include <Arduino.h>

// Performance monitoring system for LightwaveOS
// Tracks real-time performance metrics for all effects and systems

// Performance metric types
enum MetricType {
    METRIC_FRAME_TIME,          // Time to render one frame (microseconds)
    METRIC_AUDIO_PROCESSING,    // Audio processing time (microseconds)
    METRIC_EFFECT_RENDER,       // Individual effect render time (microseconds)
    METRIC_BLEND_TIME,          // Effect blending time (microseconds)
    METRIC_LED_OUTPUT,          // LED output time (microseconds)
    METRIC_MEMORY_USAGE,        // Memory usage (bytes)
    METRIC_CPU_USAGE,           // CPU usage percentage
    METRIC_TASK_SWITCHES,       // FreeRTOS task context switches
    METRIC_INTERRUPT_COUNT,     // Interrupt service routine calls
    METRIC_CACHE_MISSES,        // Memory cache misses
    METRIC_AUDIO_DROPOUTS,      // Audio buffer underruns/overruns
    METRIC_FPS_ACTUAL,          // Actual achieved FPS
    METRIC_TEMPERATURE,         // MCU temperature (celsius)
    METRIC_POWER_CONSUMPTION    // Power consumption estimate (milliwatts)
};

// Performance alert levels
enum AlertLevel {
    ALERT_NONE,
    ALERT_INFO,
    ALERT_WARNING,
    ALERT_CRITICAL,
    ALERT_EMERGENCY
};

// Performance thresholds
struct PerformanceThresholds {
    uint32_t max_frame_time_us;         // Maximum frame time (8333us for 120fps)
    uint32_t max_audio_processing_us;   // Maximum audio processing time
    uint32_t max_memory_usage_bytes;    // Maximum memory usage
    float min_fps;                      // Minimum acceptable FPS
    float max_cpu_usage_percent;        // Maximum CPU usage
    uint32_t max_audio_dropouts;        // Maximum audio dropouts per second
    float max_temperature_celsius;      // Maximum operating temperature
    uint32_t max_power_consumption_mw;  // Maximum power consumption
};

// Performance sample structure
struct PerformanceSample {
    uint32_t timestamp;
    MetricType type;
    uint32_t value;
    const char* context;  // Effect name or system component
};

// Performance statistics
struct PerformanceStats {
    uint32_t min_value;
    uint32_t max_value;
    uint32_t avg_value;
    uint32_t current_value;
    uint32_t sample_count;
    uint64_t total_accumulator;
};

// Main performance monitor class
class PerformanceMonitor {
private:
    static const uint16_t MAX_SAMPLES = 1000;
    static const uint8_t MAX_METRICS = 14;
    static const uint8_t HISTORY_SIZE = 60;  // 60-second history
    
    PerformanceSample samples[MAX_SAMPLES];
    uint16_t sample_index;
    bool samples_full;
    
    PerformanceStats stats[MAX_METRICS];
    uint32_t metric_history[MAX_METRICS][HISTORY_SIZE];
    uint8_t history_index;
    
    PerformanceThresholds thresholds;
    
    uint32_t last_update_time;
    uint32_t update_interval_ms;
    
    bool monitoring_enabled;
    bool alert_system_enabled;
    bool detailed_logging;
    
    // Timing measurement
    uint32_t measurement_start_time;
    MetricType current_measurement;
    const char* current_context;
    
    // System monitoring
    uint32_t last_free_heap;
    uint32_t last_task_switches;
    uint32_t last_interrupt_count;
    
    // Audio performance tracking
    uint32_t audio_dropout_count;
    uint32_t last_audio_dropout_time;
    
    // FPS calculation
    uint32_t frame_count;
    uint32_t last_fps_calculation_time;
    float current_fps;
    
    // Temperature monitoring
    float mcu_temperature;
    uint32_t last_temperature_read;
    
    // Power consumption estimation
    uint32_t estimated_power_mw;
    
    void update_statistics(MetricType type, uint32_t value);
    void check_thresholds(MetricType type, uint32_t value);
    void log_alert(AlertLevel level, MetricType type, uint32_t value, const char* message);
    void update_system_metrics();
    void calculate_fps();
    float read_mcu_temperature();
    uint32_t estimate_power_consumption();
    
public:
    PerformanceMonitor();
    
    // Control functions
    void enable_monitoring(bool enabled);
    void enable_alerts(bool enabled);
    void enable_detailed_logging(bool enabled);
    void set_update_interval(uint32_t interval_ms);
    
    // Threshold configuration
    void set_thresholds(const PerformanceThresholds& new_thresholds);
    void reset_to_default_thresholds();
    
    // Measurement functions
    void start_measurement(MetricType type, const char* context = nullptr);
    void end_measurement();
    void record_metric(MetricType type, uint32_t value, const char* context = nullptr);
    
    // System update
    void update();
    
    // Statistics retrieval
    PerformanceStats get_stats(MetricType type) const;
    uint32_t get_current_value(MetricType type) const;
    float get_average_over_time(MetricType type, uint32_t seconds) const;
    
    // Performance analysis
    bool is_performance_degraded() const;
    AlertLevel get_overall_alert_level() const;
    uint32_t get_bottleneck_metric() const;
    
    // Reporting
    void print_performance_report() const;
    void print_real_time_stats() const;
    void print_alert_summary() const;
    
    // Memory analysis
    uint32_t get_free_heap() const;
    uint32_t get_largest_free_block() const;
    float get_heap_fragmentation() const;
    
    // Audio performance
    void record_audio_dropout();
    uint32_t get_audio_dropout_rate() const;
    
    // Effect performance tracking
    void track_effect_start(const char* effect_name);
    void track_effect_end();
    
    // Critical performance mode
    void enter_critical_performance_mode();
    void exit_critical_performance_mode();
    bool is_in_critical_mode() const;
};

// Global performance monitor instance
extern PerformanceMonitor g_performance_monitor;

// Convenience macros for performance measurement
#define PERF_START(type, context) g_performance_monitor.start_measurement(type, context)
#define PERF_END() g_performance_monitor.end_measurement()
#define PERF_RECORD(type, value, context) g_performance_monitor.record_metric(type, value, context)

// Effect tracking macros
#define PERF_EFFECT_START(name) g_performance_monitor.track_effect_start(name)
#define PERF_EFFECT_END() g_performance_monitor.track_effect_end()

// Scoped performance measurement
class ScopedPerformanceMeasurement {
private:
    bool active;
public:
    ScopedPerformanceMeasurement(MetricType type, const char* context = nullptr) {
        g_performance_monitor.start_measurement(type, context);
        active = true;
    }
    
    ~ScopedPerformanceMeasurement() {
        if (active) {
            g_performance_monitor.end_measurement();
        }
    }
};

#define PERF_SCOPE(type, context) ScopedPerformanceMeasurement _perf_measure(type, context)

// Performance optimization hints
namespace PerformanceHints {
    void optimize_for_battery_life();
    void optimize_for_maximum_fps();
    void optimize_for_audio_quality();
    void optimize_for_visual_quality();
    void balance_performance();
}

#endif // PERFORMANCE_MONITOR_H