#ifndef FEATURES_H
#define FEATURES_H

// Feature flags to enable/disable functionality at compile time
// This helps reduce binary size and memory usage

// Core features
#ifdef FEATURE_SERIAL_MENU
#undef FEATURE_SERIAL_MENU
#endif
#define FEATURE_SERIAL_MENU 1           // Serial command interface - ENABLED
#ifdef FEATURE_PERFORMANCE_MONITOR
#undef FEATURE_PERFORMANCE_MONITOR
#endif
#define FEATURE_PERFORMANCE_MONITOR 1    // Performance tracking and reporting
// HMI REMOVED - No encoders or buttons on this hardware configuration
#define FEATURE_BUTTON_CONTROL 0         // Physical button support - DISABLED

// Network features - DISABLED BY DEFAULT because WiFi is fucking worthless
// To enable WiFi/WebServer, either:
// 1. Change FEATURE_WEB_SERVER to 1 below
// 2. Or add to platformio.ini: build_flags = -D FEATURE_WEB_SERVER=1
#ifndef FEATURE_WEB_SERVER
#define FEATURE_WEB_SERVER 1            // Web interface ENABLED for webapp
#endif
#define FEATURE_WEBSOCKET 1             // WebSocket support
#define FEATURE_OTA_UPDATE 1            // Over-the-air updates (enabled)

// LED Layout Configuration
// All matrix code has been surgically removed - strips mode is now permanent

// Effect categories
#ifndef FEATURE_BASIC_EFFECTS
#define FEATURE_BASIC_EFFECTS 1         // Gradient, wave, pulse
#endif
#ifndef FEATURE_ADVANCED_EFFECTS
#define FEATURE_ADVANCED_EFFECTS 1      // HDR, supersampling, etc.
#endif
#ifndef FEATURE_PIPELINE_EFFECTS
#define FEATURE_PIPELINE_EFFECTS 1      // Modular pipeline system
#endif
// M5Stack 8-encoder support
#define FEATURE_SCROLL_ENCODER 0        // M5Unit-Scroll - REMOVED
#define FEATURE_ROTATE8_ENCODER 1       // M5Stack 8-encoder - ENABLED
#ifdef FEATURE_AUDIO_EFFECTS
#undef FEATURE_AUDIO_EFFECTS
#endif
#define FEATURE_AUDIO_EFFECTS 0         // Audio-reactive effects (future)
#ifdef FEATURE_AUDIO_SYNC
#undef FEATURE_AUDIO_SYNC
#endif
#define FEATURE_AUDIO_SYNC 0            // Audio Synq system for VP_DECODER integration - DISABLED (no mic)

// Visual Enhancement Infrastructure (ColorEngine, MotionEngine, BlendingEngine)
// DISABLED by default for backward compatibility - Enable via platformio.ini build flag
#ifndef FEATURE_ENHANCEMENT_ENGINES
#define FEATURE_ENHANCEMENT_ENGINES 0   // Visual enhancement engines - DISABLED by default
#endif

// Individual engine flags (controlled by master flag)
#if FEATURE_ENHANCEMENT_ENGINES
  #define FEATURE_COLOR_ENGINE 1        // Cross-palette blending, diffusion, temporal rotation
  #define FEATURE_MOTION_ENGINE 1       // Phase control, easing curves, momentum physics
  #define FEATURE_BLENDING_ENGINE 1     // Zone blend modes, layer ordering, dual-strip coordination
#else
  #define FEATURE_COLOR_ENGINE 0
  #define FEATURE_MOTION_ENGINE 0
  #define FEATURE_BLENDING_ENGINE 0
#endif

// LED Strips features (now permanent after matrix removal)
#ifndef FEATURE_STRIP_EFFECTS
#define FEATURE_STRIP_EFFECTS 1         // Strip-specific effects
#endif
#ifndef FEATURE_DUAL_STRIP
#define FEATURE_DUAL_STRIP 1            // Enable dual strip support
#endif
#ifndef FEATURE_STRIP_PROPAGATION
#define FEATURE_STRIP_PROPAGATION 1     // Bidirectional propagation
#endif
#ifndef FEATURE_STRIP_SYNC
#define FEATURE_STRIP_SYNC 1            // Cross-strip synchronization
#endif
#ifndef FEATURE_STRIP_SEGMENTS
#define FEATURE_STRIP_SEGMENTS 1        // Strip segmentation
#endif
#ifndef FEATURE_STRIP_POWER_MGMT
#define FEATURE_STRIP_POWER_MGMT 1      // Power management for 320 LEDs
#endif

// Light Guide Plate specific features (extension of strips mode)
#ifndef FEATURE_LIGHT_GUIDE_MODE
#define FEATURE_LIGHT_GUIDE_MODE 0      // LGP implementation removed - awaiting Dual-Strip Wave Engine
#endif
#ifndef FEATURE_INTERFERENCE_CALC
#define FEATURE_INTERFERENCE_CALC 1     // Wave interference calculations
#endif
#ifndef FEATURE_INTERACTIVE_SENSING
#define FEATURE_INTERACTIVE_SENSING 0   // Proximity/touch sensing (future)
#endif
#ifndef FEATURE_PHYSICS_SIMULATION
#define FEATURE_PHYSICS_SIMULATION 1    // Physics-based effects
#endif
#ifndef FEATURE_HOLOGRAPHIC_PATTERNS
#define FEATURE_HOLOGRAPHIC_PATTERNS 1  // Holographic interference effects
#endif
#ifndef FEATURE_DEPTH_ILLUSION
#define FEATURE_DEPTH_ILLUSION 1        // 3D depth illusion effects
#endif

// Hardware optimization
#ifndef FEATURE_HARDWARE_OPTIMIZATION
#define FEATURE_HARDWARE_OPTIMIZATION 1  // ESP32-specific optimizations
#endif
#ifndef FEATURE_FASTLED_OPTIMIZATION
#define FEATURE_FASTLED_OPTIMIZATION 1   // FastLED advanced features
#endif

// Debug features
#ifndef FEATURE_DEBUG_OUTPUT
#define FEATURE_DEBUG_OUTPUT 0          // Reduced serial debug messages
#endif
#ifndef FEATURE_MEMORY_DEBUG
#define FEATURE_MEMORY_DEBUG 0          // Disable heap tracking to reduce noise
#endif
#ifndef FEATURE_TIMING_DEBUG
#define FEATURE_TIMING_DEBUG 0          // Disable microsecond timing
#endif

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
