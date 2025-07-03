#ifndef FEATURES_H
#define FEATURES_H

// Feature flags to enable/disable functionality at compile time
// This helps reduce binary size and memory usage

// Core features
#define FEATURE_SERIAL_MENU 0           // Serial command interface - DISABLED (conflicts)
#define FEATURE_PERFORMANCE_MONITOR 1    // Performance tracking and reporting
#define FEATURE_BUTTON_CONTROL 0         // Physical button support (NO BUTTON ON BOARD)

// Network features
#ifndef FEATURE_WEB_SERVER
#define FEATURE_WEB_SERVER 1            // Web interface enabled by default (can override via build flags)
#endif
#define FEATURE_WEBSOCKET 0             // WebSocket support
#define FEATURE_OTA_UPDATE 0            // Over-the-air updates

// LED Layout Configuration
// All matrix code has been surgically removed - strips mode is now permanent

// Effect categories
#define FEATURE_BASIC_EFFECTS 1         // Gradient, wave, pulse
#define FEATURE_ADVANCED_EFFECTS 1      // HDR, supersampling, etc.
#define FEATURE_PIPELINE_EFFECTS 1      // Modular pipeline system
#define FEATURE_AUDIO_EFFECTS 0         // Audio-reactive effects (future)

// LED Strips features (now permanent after matrix removal)
#define FEATURE_STRIP_EFFECTS 1         // Strip-specific effects
#define FEATURE_DUAL_STRIP 1            // Enable dual strip support
#define FEATURE_STRIP_PROPAGATION 1     // Bidirectional propagation
#define FEATURE_STRIP_SYNC 1            // Cross-strip synchronization
#define FEATURE_STRIP_SEGMENTS 1        // Strip segmentation
#define FEATURE_STRIP_POWER_MGMT 1      // Power management for 320 LEDs

// Light Guide Plate specific features (extension of strips mode)
#define FEATURE_LIGHT_GUIDE_MODE 0      // LGP implementation removed - awaiting Dual-Strip Wave Engine
#define FEATURE_INTERFERENCE_CALC 1     // Wave interference calculations
#define FEATURE_INTERACTIVE_SENSING 0   // Proximity/touch sensing (future)
#define FEATURE_PHYSICS_SIMULATION 1    // Physics-based effects
#define FEATURE_HOLOGRAPHIC_PATTERNS 1  // Holographic interference effects
#define FEATURE_DEPTH_ILLUSION 1        // 3D depth illusion effects

// Hardware optimization
#define FEATURE_HARDWARE_OPTIMIZATION 1  // ESP32-specific optimizations
#define FEATURE_FASTLED_OPTIMIZATION 1   // FastLED advanced features

// Debug features
#define FEATURE_DEBUG_OUTPUT 1          // Serial debug messages
#define FEATURE_MEMORY_DEBUG 1          // Heap tracking and analysis
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