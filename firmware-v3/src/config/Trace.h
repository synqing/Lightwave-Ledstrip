// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file Trace.h
 * @brief MabuTrace integration for Perfetto timeline visualisation
 *
 * System-wide tracing wrapper for LightwaveOS. When FEATURE_MABUTRACE is
 * enabled, trace events are recorded and can be viewed in the Perfetto UI
 * for detailed timing analysis of any subsystem (audio, render, network, etc.).
 *
 * When disabled, all macros compile to nothing with zero runtime overhead.
 *
 * Usage:
 * @code
 *   #include "config/Trace.h"
 *
 *   void processAudio() {
 *       TRACE_SCOPE("audio_processing");
 *
 *       {
 *           TRACE_SCOPE("goertzel_analyze");
 *           // Goertzel analysis code
 *       }
 *
 *       TRACE_COUNTER("cpu_load", cpuPercent);
 *       TRACE_INSTANT("FALSE_TRIGGER");
 *   }
 * @endcode
 *
 * Build with: pio run -e esp32dev_audio_trace
 *
 * @see docs/debugging/MABUTRACE_GUIDE.md for capture and visualisation
 */

#pragma once

#include "features.h"

#if FEATURE_MABUTRACE

// Include MabuTrace when enabled
#include <mabutrace.h>

/**
 * @brief Begin a scoped trace event
 *
 * Creates a trace span that automatically ends when the scope exits.
 * Use for measuring function or block execution time.
 *
 * @param name Static string name for the trace event (e.g., "audio_pipeline")
 */
#define TRACE_SCOPE(name) MABU_TRACE_SCOPE(name)

/**
 * @brief Record a counter value
 *
 * Logs a numeric counter value that appears as a graph in Perfetto.
 * Use for tracking metrics like CPU load, memory usage, or signal levels.
 *
 * @param name Static string name for the counter (e.g., "cpu_load")
 * @param value Integer or float value to record
 */
#define TRACE_COUNTER(name, value) MABU_TRACE_COUNTER(name, value)

/**
 * @brief Record an instant event
 *
 * Logs a single point-in-time event marker in the timeline.
 * Use for marking discrete events like triggers or state changes.
 *
 * @param name Static string name for the instant event (e.g., "FALSE_TRIGGER")
 */
#define TRACE_INSTANT(name) MABU_TRACE_INSTANT(name)

/**
 * @brief Begin a named trace span (manual end required)
 *
 * For cases where TRACE_SCOPE cannot be used (non-RAII patterns).
 * Must be paired with TRACE_END.
 *
 * @param name Static string name for the span
 */
#define TRACE_BEGIN(name) MABU_TRACE_BEGIN(name)

/**
 * @brief End a previously started trace span
 *
 * Must match a prior TRACE_BEGIN call.
 */
#define TRACE_END() MABU_TRACE_END()

/**
 * @brief Initialise the MabuTrace system
 *
 * Call once during setup before any trace events.
 * Allocates the trace buffer and starts the background writer.
 *
 * @param buffer_kb Size of the trace ring buffer in KB (default: 64)
 */
#define TRACE_INIT(buffer_kb) MABU_TRACE_INIT(buffer_kb)

/**
 * @brief Flush trace buffer and prepare for capture
 *
 * Call before retrieving trace data to ensure all pending
 * events are written to the buffer.
 */
#define TRACE_FLUSH() MABU_TRACE_FLUSH()

/**
 * @brief Check if tracing is currently enabled
 *
 * @return true if tracing is active, false otherwise
 */
#define TRACE_IS_ENABLED() MABU_TRACE_IS_ENABLED()

#else // !FEATURE_MABUTRACE

//-----------------------------------------------------------------------------
// No-op stubs when MabuTrace is disabled
// These compile to nothing, ensuring zero runtime overhead.
//-----------------------------------------------------------------------------

#define TRACE_SCOPE(name)           do {} while(0)
#define TRACE_COUNTER(name, value)  do { (void)(value); } while(0)
#define TRACE_INSTANT(name)         do {} while(0)
#define TRACE_BEGIN(name)           do {} while(0)
#define TRACE_END()                 do {} while(0)
#define TRACE_INIT(buffer_kb)       do { (void)(buffer_kb); } while(0)
#define TRACE_FLUSH()               do {} while(0)
#define TRACE_IS_ENABLED()          (false)

#endif // FEATURE_MABUTRACE
