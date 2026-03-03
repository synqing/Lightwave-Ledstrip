/**
 * @file features.h
 * @brief Compile-time feature flags for LightwaveOS v2
 *
 * These flags control which optional features are compiled into the firmware.
 * Most flags can be overridden via platformio.ini build_flags.
 */

#pragma once

// Chip detection for platform-specific feature gating
#include "chip_config.h"

// ============================================================================
// Core Features (always enabled, no flag gating needed)
// ============================================================================
// Actor System, Plugin Runtime, CQRS State, HAL Abstraction are always compiled in.

// ============================================================================
// Network Features (defined in common build flags)
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
// P4 Overrides (no network on P4)
// ============================================================================

#if defined(CHIP_ESP32_P4) && CHIP_ESP32_P4
    #undef FEATURE_WEB_SERVER
    #define FEATURE_WEB_SERVER 0

    #undef FEATURE_MULTI_DEVICE
    #define FEATURE_MULTI_DEVICE 0
#endif

// ============================================================================
// Optional Features
// ============================================================================


// Zone System
#ifndef FEATURE_ZONE_SYSTEM
#define FEATURE_ZONE_SYSTEM 1
#endif

// Transition Engine
#ifndef FEATURE_TRANSITIONS
#define FEATURE_TRANSITIONS 1
#endif

// Audio Reactive Effects + Trinity Protocol (PRISM Dashboard integration)
#ifndef FEATURE_AUDIO_SYNC
#define FEATURE_AUDIO_SYNC 1
#endif

// Alternate audio backend: Emotiscope v1.1_320 end-to-end pipeline.
// When enabled, LWLS audio analysis/tempo codepaths must not run.
#ifndef FEATURE_AUDIO_BACKEND_ESV11
#define FEATURE_AUDIO_BACKEND_ESV11 0
#endif

// ESV11 at 32kHz sample rate (SPH0645 overclocked, 125 Hz hop rate).
// Implies FEATURE_AUDIO_BACKEND_ESV11. All vendor constants overridden via shim.
#ifndef FEATURE_AUDIO_BACKEND_ESV11_32KHZ
#define FEATURE_AUDIO_BACKEND_ESV11_32KHZ 0
#endif

// Auto-enable ESV11 when 32kHz variant is selected
#if FEATURE_AUDIO_BACKEND_ESV11_32KHZ && !FEATURE_AUDIO_BACKEND_ESV11
    #undef FEATURE_AUDIO_BACKEND_ESV11
    #define FEATURE_AUDIO_BACKEND_ESV11 1
#endif

// Alternate audio backend: PipelineCore DSP engine (256-bin FFT + BeatTracker).
// When enabled, replaces GoertzelAnalyzer + ChromaAnalyzer + TempoTracker
// with PipelineCore + BeatTracker + PipelineAdapter.
#ifndef FEATURE_AUDIO_BACKEND_PIPELINECORE
#define FEATURE_AUDIO_BACKEND_PIPELINECORE 0
#endif

// Spine16k: PipelineCore engine at 16kHz/128-hop (DSP Spine v0.1).
// Implies FEATURE_AUDIO_BACKEND_PIPELINECORE. Only SAMPLE_RATE and HOP_SIZE differ.
#ifndef FEATURE_AUDIO_BACKEND_SPINE16K
#define FEATURE_AUDIO_BACKEND_SPINE16K 0
#endif

// Auto-enable PipelineCore when Spine16k variant is selected
#if FEATURE_AUDIO_BACKEND_SPINE16K && !FEATURE_AUDIO_BACKEND_PIPELINECORE
    #undef FEATURE_AUDIO_BACKEND_PIPELINECORE
    #define FEATURE_AUDIO_BACKEND_PIPELINECORE 1
#endif

// Mutual exclusion: only one audio backend may be active
#if FEATURE_AUDIO_BACKEND_ESV11 && FEATURE_AUDIO_BACKEND_PIPELINECORE
#error "Cannot enable both ESV11 and PipelineCore backends simultaneously"
#endif

// Auto-speed trim (audio-driven base speed) - disabled by default
#ifndef FEATURE_AUTO_SPEED
#define FEATURE_AUTO_SPEED 0
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

// Sensory Bridge parity side-car pipeline (waveform/chromagram/bloom payloads).
// Disable on performance builds when effects do not consume SB parity fields.
#ifndef FEATURE_SB_PARITY_SIDECAR
#define FEATURE_SB_PARITY_SIDECAR FEATURE_AUDIO_SYNC
#endif

// Run SB side-car update once every N hops (1 = every hop).
// Use values >1 to reduce side-car CPU while preserving last published payload.
#ifndef AUDIO_SB_SIDECAR_DECIMATION
#define AUDIO_SB_SIDECAR_DECIMATION 1
#endif

// Audio Overlap-Add sliding window for bands/chroma
#ifndef FEATURE_AUDIO_OA
#define FEATURE_AUDIO_OA 1
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

// M5Stack ROTATE8 Encoder Support
#ifndef FEATURE_ROTATE8_ENCODER
#define FEATURE_ROTATE8_ENCODER 0
#endif

// ============================================================================
// Enhancement Engines (optional color and motion enhancements)
// ============================================================================

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
// System Monitoring (can be disabled for RAM-constrained builds e.g. FH4)
// ============================================================================

#ifndef FEATURE_HEAP_MONITORING
#define FEATURE_HEAP_MONITORING 1
#endif

#ifndef FEATURE_MEMORY_LEAK_DETECTION
#define FEATURE_MEMORY_LEAK_DETECTION 1
#endif

#ifndef FEATURE_VALIDATION_PROFILING
#define FEATURE_VALIDATION_PROFILING 1
#endif

#ifndef FEATURE_STACK_PROFILING
#define FEATURE_STACK_PROFILING 1
#endif

// ============================================================================
// Debug Features
// ============================================================================

// Debug Mode
#ifndef DEBUG
#define DEBUG 0
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
