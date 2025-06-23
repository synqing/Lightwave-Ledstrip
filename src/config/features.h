#ifndef FEATURES_H
#define FEATURES_H

// Feature flags to enable/disable functionality at compile time
// This helps reduce binary size and memory usage

// Core features
#define FEATURE_SERIAL_MENU 1           // Serial command interface
#define FEATURE_PERFORMANCE_MONITOR 1    // Performance tracking and reporting
#define FEATURE_BUTTON_CONTROL 1         // Physical button support

// Network features
#define FEATURE_WEB_SERVER 0            // Web interface (currently disabled)
#define FEATURE_WEBSOCKET 0             // WebSocket support
#define FEATURE_OTA_UPDATE 0            // Over-the-air updates

// Effect categories
#define FEATURE_BASIC_EFFECTS 1         // Gradient, wave, pulse
#define FEATURE_ADVANCED_EFFECTS 1      // HDR, supersampling, etc.
#define FEATURE_PIPELINE_EFFECTS 1      // Modular pipeline system
#define FEATURE_AUDIO_EFFECTS 0         // Audio-reactive effects (future)

// Hardware optimization
#define FEATURE_HARDWARE_OPTIMIZATION 1  // ESP32-specific optimizations
#define FEATURE_FASTLED_OPTIMIZATION 1   // FastLED advanced features

// Debug features
#define FEATURE_DEBUG_OUTPUT 1          // Serial debug messages
#define FEATURE_MEMORY_DEBUG 0          // Heap tracking
#define FEATURE_TIMING_DEBUG 1          // Microsecond timing

// Derived flags
#if FEATURE_WEB_SERVER || FEATURE_WEBSOCKET
    #define FEATURE_NETWORK 1
#else
    #define FEATURE_NETWORK 0
#endif

#if FEATURE_PERFORMANCE_MONITOR || FEATURE_MEMORY_DEBUG || FEATURE_TIMING_DEBUG
    #define FEATURE_PROFILING 1
#else
    #define FEATURE_PROFILING 0
#endif

#endif // FEATURES_H