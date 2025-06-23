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

// LED Layout Modes (mutually exclusive)
#define LED_STRIPS_MODE 0               // Dual 160-LED strips mode
#define LED_MATRIX_MODE 1               // 9x9 matrix mode (default)

// Effect categories
#define FEATURE_BASIC_EFFECTS 1         // Gradient, wave, pulse
#define FEATURE_ADVANCED_EFFECTS 1      // HDR, supersampling, etc.
#define FEATURE_PIPELINE_EFFECTS 1      // Modular pipeline system
#define FEATURE_AUDIO_EFFECTS 0         // Audio-reactive effects (future)

// LED Strips specific features (only when LED_STRIPS_MODE enabled)
#if LED_STRIPS_MODE
#define FEATURE_STRIP_EFFECTS 1         // Strip-specific effects
#define FEATURE_DUAL_STRIP 1            // Enable dual strip support
#define FEATURE_STRIP_PROPAGATION 1     // Bidirectional propagation
#define FEATURE_STRIP_SYNC 1            // Cross-strip synchronization
#define FEATURE_STRIP_SEGMENTS 1        // Strip segmentation
#define FEATURE_STRIP_POWER_MGMT 1      // Power management for 320 LEDs
#else
#define FEATURE_STRIP_EFFECTS 0
#define FEATURE_DUAL_STRIP 0
#define FEATURE_STRIP_PROPAGATION 0
#define FEATURE_STRIP_SYNC 0
#define FEATURE_STRIP_SEGMENTS 0
#define FEATURE_STRIP_POWER_MGMT 0
#endif

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