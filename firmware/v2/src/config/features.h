/**
 * @file features.h
 * @brief Compile-time feature flags for LightwaveOS v2
 *
 * These flags control which optional features are compiled into the firmware.
 * Most flags can be overridden via platformio.ini build_flags.
 */

#pragma once

// ============================================================================
// Core Features (defined in platformio.ini)
// ============================================================================

// Actor System - always enabled in v2
#ifndef FEATURE_ACTOR_SYSTEM
#define FEATURE_ACTOR_SYSTEM 1
#endif

// Plugin Runtime - effect plugin system
#ifndef FEATURE_PLUGIN_RUNTIME
#define FEATURE_PLUGIN_RUNTIME 1
#endif

// CQRS State Management
#ifndef FEATURE_CQRS_STATE
#define FEATURE_CQRS_STATE 1
#endif

// HAL Abstraction Layer
#ifndef FEATURE_HAL_ABSTRACTION
#define FEATURE_HAL_ABSTRACTION 1
#endif

// ============================================================================
// Network Features (enabled by default in esp32dev_audio environment)
// ============================================================================

// Web Server with REST API and WebSocket
#ifndef FEATURE_WEB_SERVER
#define FEATURE_WEB_SERVER 0
#endif

// Multi-Device Synchronization
#ifndef FEATURE_MULTI_DEVICE
#define FEATURE_MULTI_DEVICE 0
#endif

// ============================================================================
// Optional Features
// ============================================================================

// Serial Menu Interface
#ifndef FEATURE_SERIAL_MENU
#define FEATURE_SERIAL_MENU 1
#endif

// Performance Monitoring
#ifndef FEATURE_PERFORMANCE_MONITOR
#define FEATURE_PERFORMANCE_MONITOR 1
#endif

// Zone System
#ifndef FEATURE_ZONE_SYSTEM
#define FEATURE_ZONE_SYSTEM 1
#endif

// Transition Engine
#ifndef FEATURE_TRANSITIONS
#define FEATURE_TRANSITIONS 1
#endif

// Audio Reactive Effects (requires I2S microphone)
#ifndef FEATURE_AUDIO_SYNC
#define FEATURE_AUDIO_SYNC 0
#endif

// Musical Saliency Analysis - Computes harmonic/rhythmic/timbral/dynamic novelty
// CPU cost: ~80 µs per hop. Only 3/76 effects currently use this.
// Disable if no effects use ctx.audio.harmonicSaliency(), rhythmicSaliency(), etc.
#ifndef FEATURE_MUSICAL_SALIENCY
#define FEATURE_MUSICAL_SALIENCY FEATURE_AUDIO_SYNC
#endif

// Music Style Detection - Classifies as rhythmic/harmonic/melodic/texture/dynamic
// CPU cost: ~60 µs per hop (amortized over 4-second windows). Only 2/76 effects use this.
// Disable if no effects use ctx.audio.musicStyle(), isRhythmicMusic(), etc.
#ifndef FEATURE_STYLE_DETECTION
#define FEATURE_STYLE_DETECTION FEATURE_AUDIO_SYNC
#endif

// Audio Overlap-Add sliding window for bands/chroma
#ifndef FEATURE_AUDIO_OA
#define FEATURE_AUDIO_OA 1
#endif

// Optional 75% overlap (128-sample advance within 256-sample hop)
#ifndef FEATURE_AUDIO_OA_75
#define FEATURE_AUDIO_OA_75 0
#endif

// Audio Pipeline Benchmarking - Enables metrics collection and A/B testing
// Use with native_test environment for validation framework
#ifndef FEATURE_AUDIO_BENCHMARK
#define FEATURE_AUDIO_BENCHMARK 0
#endif

// Effect Validation Framework - Enables real-time effect validation streaming
// Captures phase, speed, audio metrics for debugging jog-dial behavior
#ifndef FEATURE_EFFECT_VALIDATION
#define FEATURE_EFFECT_VALIDATION 0
#endif

// OTA Updates - Enabled by default for development
// SECURITY: Set a unique token per device in production:
//   -D OTA_TOKEN=\"your-unique-secret-token\"
#ifndef FEATURE_OTA_UPDATE
#define FEATURE_OTA_UPDATE 1
#endif

// API Key Authentication - Requires X-API-Key header on protected endpoints
// Enable: -D FEATURE_API_AUTH=1 -D API_KEY=\"your-secret-key\"
#ifndef FEATURE_API_AUTH
#define FEATURE_API_AUTH 0
#endif

// ESP-NOW Wireless Encoders
#ifndef FEATURE_WIRELESS_ENCODERS
#define FEATURE_WIRELESS_ENCODERS 0
#endif

// M5Stack ROTATE8 Encoder Support
#ifndef FEATURE_ROTATE8_ENCODER
#define FEATURE_ROTATE8_ENCODER 0
#endif

// ============================================================================
// Enhancement Engines (optional color and motion enhancements)
// ============================================================================

// Enhancement Engines - Combines ColorEngine and MotionEngine
#ifndef FEATURE_ENHANCEMENT_ENGINES
#define FEATURE_ENHANCEMENT_ENGINES 1
#endif

// ColorEngine - Cross-palette blending, diffusion, temporal rotation
#ifndef FEATURE_COLOR_ENGINE
#define FEATURE_COLOR_ENGINE 1
#endif

// MotionEngine - Phase offset, auto-rotation, particle physics
#ifndef FEATURE_MOTION_ENGINE
#define FEATURE_MOTION_ENGINE 1
#endif

// Pattern Registry - Pattern taxonomy and metadata system
#ifndef FEATURE_PATTERN_REGISTRY
#define FEATURE_PATTERN_REGISTRY 1
#endif

// ============================================================================
// Debug Features
// ============================================================================

// Unified Logging System - Consistent colored output with timestamps
// Enables LW_LOGI, LW_LOGW, LW_LOGE, LW_LOGD macros from utils/Log.h
// Log levels: 0=None, 1=Error, 2=Warn, 3=Info, 4=Debug
// Set level via: -D LW_LOG_LEVEL=3
#ifndef FEATURE_UNIFIED_LOGGING
#define FEATURE_UNIFIED_LOGGING 1
#endif

// Debug Mode
#ifndef DEBUG
#define DEBUG 0
#endif

// Memory Debug (tracks allocations)
#ifndef FEATURE_MEMORY_DEBUG
#define FEATURE_MEMORY_DEBUG 0
#endif

// Effect Profiling
#ifndef FEATURE_EFFECT_PROFILER
#define FEATURE_EFFECT_PROFILER 0
#endif

// MabuTrace Integration - Perfetto timeline visualization for audio pipeline
// Enables detailed trace events viewable in Perfetto UI (ui.perfetto.dev)
// Build: pio run -e esp32dev_audio_trace
// See: docs/debugging/MABUTRACE_GUIDE.md
#ifndef FEATURE_MABUTRACE
#define FEATURE_MABUTRACE 0
#endif

// Note: K1 beat tracker has been replaced by TempoTracker
// Use 'tempo' serial command for tempo debugging
