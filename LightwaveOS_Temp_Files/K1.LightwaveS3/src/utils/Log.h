/**
 * @file Log.h
 * @brief Unified logging system for LightwaveOS v2
 *
 * Provides consistent, colored logging with automatic timestamps and component tags.
 * Preserves existing ASCII color coding conventions from the codebase.
 *
 * Usage:
 *   #define LW_LOG_TAG "MyComponent"
 *   #include "utils/Log.h"
 *
 *   LW_LOGI("Initialized with %d items", count);
 *   LW_LOGE("Failed: %s (code=%d)", msg, err);
 *   LW_LOGW("Memory low: %lu bytes", freeHeap);
 *   LW_LOGD("Debug value: %f", val);
 *
 * Output format:
 *   [12345][INFO][MyComponent] Initialized with 5 items
 *   [12346][ERROR][MyComponent] Failed: timeout (code=-1)
 *
 * Title-only coloring (for high-frequency logs):
 *   Serial.printf("Effect: " LW_CLR_GREEN "%s" LW_ANSI_RESET "\n", name);
 */

#pragma once

#include <cstdio>

// ============================================================================
// ANSI Color Constants (Preserving existing codebase palette)
// ============================================================================
// These match the exact colors already used throughout the codebase:
// - Green: Effect names (main.cpp, RendererActor.cpp)
// - Yellow: Hardware/DMA diagnostics, audio levels (AudioCapture.cpp, AudioNode.cpp)
// - Cyan: Audio spectral analysis (AudioNode.cpp)

#define LW_ANSI_RESET      "\033[0m"
#define LW_ANSI_BOLD       "\033[1m"

// Domain-specific colors (matching existing usage patterns)
#define LW_CLR_GREEN       "\033[1;32m"   // UI/effect selection feedback
#define LW_CLR_YELLOW      "\033[1;33m"   // Hardware/DMA diagnostics, audio dB levels
#define LW_CLR_CYAN        "\033[1;36m"   // Audio analysis (bold - 8-bin Goertzel)
#define LW_CLR_CYAN_DIM    "\033[36m"     // Audio analysis (normal - 64-bin Goertzel)
#define LW_CLR_RED         "\033[1;31m"   // Errors
#define LW_CLR_MAGENTA     "\033[1;35m"   // Warnings
#define LW_CLR_WHITE       "\033[1;37m"   // Info (bright)
#define LW_CLR_GRAY        "\033[0;37m"   // Debug (dim)
#define LW_CLR_BLUE        "\033[1;34m"   // Network/WebSocket

// Semantic aliases for log levels
#define LW_CLR_ERROR       LW_CLR_RED
#define LW_CLR_WARN        LW_CLR_MAGENTA
#define LW_CLR_INFO        LW_CLR_GREEN
#define LW_CLR_DEBUG       LW_CLR_GRAY

// ============================================================================
// Log Level Configuration
// ============================================================================
// Set via platformio.ini build flags:
//   -D LW_LOG_LEVEL=3   (0=None, 1=Error, 2=Warn, 3=Info, 4=Debug)
//
// Default: INFO level (shows Error, Warn, Info)

#ifndef LW_LOG_LEVEL
    #ifdef NDEBUG
        #define LW_LOG_LEVEL 2   // Release: Warn and above
    #else
        #define LW_LOG_LEVEL 3   // Debug: Info and above
    #endif
#endif

// Log level thresholds
#define LW_LOG_LEVEL_NONE  0
#define LW_LOG_LEVEL_ERROR 1
#define LW_LOG_LEVEL_WARN  2
#define LW_LOG_LEVEL_INFO  3
#define LW_LOG_LEVEL_DEBUG 4

// ============================================================================
// Platform Detection
// ============================================================================

#ifdef ARDUINO
    #include <Arduino.h>
    #define LW_LOG_MILLIS()    millis()
    #define LW_LOG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
    // Native build support (unit tests, simulation)
    #include <cstdio>
    #include <cstdint>
    static inline uint32_t _lw_mock_millis() {
        static uint32_t mock_time = 0;
        return mock_time++;
    }
    #define LW_LOG_MILLIS()    _lw_mock_millis()
    #define LW_LOG_PRINTF(...) printf(__VA_ARGS__)
#endif

// ============================================================================
// Log Callback System (for wireless serial monitoring)
// ============================================================================
// Allows external components (e.g., WebSocket broadcaster) to receive log output.
// The callback receives the fully formatted log line (with ANSI codes).

#include <functional>

namespace lightwaveos {
namespace logging {

// Callback type for log output interception
using LogCallback = std::function<void(const char* formattedLine)>;

// Global callback storage (defined inline to keep header-only)
inline LogCallback& getLogCallback() {
    static LogCallback callback = nullptr;
    return callback;
}

// Set the log callback (call from WebServer initialization)
inline void setLogCallback(LogCallback cb) {
    getLogCallback() = cb;
}

// Clear the log callback
inline void clearLogCallback() {
    getLogCallback() = nullptr;
}

// Check if callback is set
inline bool hasLogCallback() {
    return getLogCallback() != nullptr;
}

} // namespace logging
} // namespace lightwaveos

// Helper function that outputs to both Serial and callback
// Must be inline to work in header-only mode
inline void _lw_log_output(const char* formatted) {
    // Always output to Serial
    LW_LOG_PRINTF("%s", formatted);

    // Also send to callback if set
    if (lightwaveos::logging::hasLogCallback()) {
        lightwaveos::logging::getLogCallback()(formatted);
    }
}

// ============================================================================
// Core Logging Macros
// ============================================================================
// Format: [timestamp][LEVEL][TAG] message
// Colors: Level indicator is colored, values remain uncolored for readability

#ifndef LW_LOG_TAG
    #define LW_LOG_TAG "LW"
#endif

// Internal formatting macro
#define LW_LOG_FORMAT(level_str, level_color, fmt) \
    "[%lu]" level_color "[" level_str "]" LW_ANSI_RESET "[" LW_LOG_TAG "] " fmt "\n"

// Buffer size for formatted log messages
#define LW_LOG_BUFFER_SIZE 256

// Internal macro that formats and outputs to both Serial and callback
#define LW_LOG_IMPL(level_str, level_color, fmt, ...) \
    do { \
        char _lw_log_buf[LW_LOG_BUFFER_SIZE]; \
        snprintf(_lw_log_buf, sizeof(_lw_log_buf), \
                 LW_LOG_FORMAT(level_str, level_color, fmt), \
                 (unsigned long)LW_LOG_MILLIS(), ##__VA_ARGS__); \
        _lw_log_output(_lw_log_buf); \
    } while(0)

// Error: Always visible (level >= 1)
#if LW_LOG_LEVEL >= LW_LOG_LEVEL_ERROR
    #define LW_LOGE(fmt, ...) LW_LOG_IMPL("ERROR", LW_CLR_ERROR, fmt, ##__VA_ARGS__)
#else
    #define LW_LOGE(fmt, ...) ((void)0)
#endif

// Warning: Visible at level >= 2
#if LW_LOG_LEVEL >= LW_LOG_LEVEL_WARN
    #define LW_LOGW(fmt, ...) LW_LOG_IMPL("WARN", LW_CLR_WARN, fmt, ##__VA_ARGS__)
#else
    #define LW_LOGW(fmt, ...) ((void)0)
#endif

// Info: Visible at level >= 3
#if LW_LOG_LEVEL >= LW_LOG_LEVEL_INFO
    #define LW_LOGI(fmt, ...) LW_LOG_IMPL("INFO", LW_CLR_INFO, fmt, ##__VA_ARGS__)
#else
    #define LW_LOGI(fmt, ...) ((void)0)
#endif

// Debug: Visible at level >= 4
#if LW_LOG_LEVEL >= LW_LOG_LEVEL_DEBUG
    #define LW_LOGD(fmt, ...) LW_LOG_IMPL("DEBUG", LW_CLR_DEBUG, fmt, ##__VA_ARGS__)
#else
    #define LW_LOGD(fmt, ...) ((void)0)
#endif

// ============================================================================
// Title-Only Coloring Helpers
// ============================================================================
// For high-frequency logs (DMA @ 62.5Hz, Goertzel @ 100Hz), only the title
// should be colored while values remain uncolored for readability.
//
// Usage:
//   Serial.printf("Effect: " LW_TITLE_GREEN "%s" LW_ANSI_RESET "\n", name);
//   ESP_LOGI(TAG, LW_TITLE_YELLOW "DMA dbg:" LW_ANSI_RESET " hop=%lu", hop);

#define LW_TITLE_GREEN     LW_CLR_GREEN
#define LW_TITLE_YELLOW    LW_CLR_YELLOW
#define LW_TITLE_CYAN      LW_CLR_CYAN
#define LW_TITLE_CYAN_DIM  LW_CLR_CYAN_DIM
#define LW_TITLE_RED       LW_CLR_RED

// ============================================================================
// Conditional Logging (Throttled)
// ============================================================================
// For logs that should only appear occasionally (e.g., 1/second from 100Hz loop)
//
// Usage:
//   static uint32_t lastLog = 0;
//   LW_LOG_THROTTLE(lastLog, 1000, LW_LOGI("Status: %d", val));

#define LW_LOG_THROTTLE(last_var, interval_ms, log_statement) \
    do { \
        uint32_t _now = LW_LOG_MILLIS(); \
        if (_now - (last_var) >= (interval_ms)) { \
            (last_var) = _now; \
            log_statement; \
        } \
    } while(0)

// ============================================================================
// Error Context Helpers
// ============================================================================
// Add heap and function context to error messages
//
// Usage:
//   LW_LOGE_CTX("Allocation failed for buffer");
//   // Output: [12345][ERROR][TAG] Allocation failed for buffer (heap=45678, fn=setup)

#ifdef ARDUINO
    #define LW_HEAP_FREE() ESP.getFreeHeap()
#else
    #define LW_HEAP_FREE() 0UL
#endif

#define LW_LOGE_CTX(fmt, ...) \
    LW_LOGE(fmt " (heap=%lu, fn=%s)", ##__VA_ARGS__, (unsigned long)LW_HEAP_FREE(), __func__)

// ============================================================================
// Backward Compatibility
// ============================================================================
// Existing code using inline ANSI sequences continues to work unchanged.
// These aliases make migration easier:

// Legacy patterns still supported:
//   Serial.printf("Effect %d: \033[1;32m%s\033[0m\n", id, name);  // Still works
//   Serial.printf("Effect %d: " LW_CLR_GREEN "%s" LW_ANSI_RESET "\n", id, name);  // New style
