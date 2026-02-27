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

// MabuTrace library -- provides TRACE_SCOPE, TRACE_COUNTER, TRACE_INSTANT
// directly as macros. We do NOT redefine those three.
#include <mabutrace.h>

// --- Manual span pair (for non-RAII patterns with early returns) ----------
// TRACE_BEGIN/TRACE_END use a file-local static handle. Safe because:
//   - All BEGIN/END pairs in the codebase are sequential (never nested)
//   - Each .cpp gets its own static copy via the header
//   - AudioActor (Core 0) and RendererActor (Core 1) are in separate files
static profiler_duration_handle_t _lw_trace_h;

#define TRACE_BEGIN(name) (_lw_trace_h = trace_begin(name, COLOR_UNDEFINED))
#define TRACE_END()       trace_end(&_lw_trace_h)

// --- System lifecycle -----------------------------------------------------
// mabutrace_init() uses a hardcoded 64 KB ring buffer; parameter is ignored.
#define TRACE_INIT(buffer_kb) mabutrace_init()
#define TRACE_FLUSH()         ((void)0)
#define TRACE_IS_ENABLED()    (true)

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
