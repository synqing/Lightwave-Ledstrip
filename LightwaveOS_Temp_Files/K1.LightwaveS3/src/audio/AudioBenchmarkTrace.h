/**
 * @file AudioBenchmarkTrace.h
 * @brief MabuTrace integration for Perfetto timeline visualization
 *
 * This wrapper provides optional MabuTrace integration for the LightwaveOS
 * audio pipeline. When FEATURE_MABUTRACE is enabled, trace events are
 * recorded and can be viewed in the Perfetto UI for detailed timing analysis.
 *
 * When disabled, all macros compile to nothing with zero runtime overhead.
 *
 * Usage:
 * @code
 *   #include "AudioBenchmarkTrace.h"
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
 * @see docs/debugging/MABUTRACE_GUIDE.md for capture and visualization
 */

#pragma once

#include "../config/features.h"

#if FEATURE_MABUTRACE

// Include MabuTrace when enabled
#include <mabutrace.h>

/**
 * @brief Initialize the MabuTrace system
 *
 * Call once during setup before any trace events.
 * Uses MabuTrace's built-in ring buffer size.
 */
#define TRACE_INIT(buffer_kb) do { (void)(buffer_kb); (void)mabutrace_init(); } while(0)

/**
 * @brief Flush trace buffer and prepare for capture
 *
 * No explicit flush API in MabuTrace; this is a best-effort no-op.
 */
#define TRACE_FLUSH() do {} while(0)

/**
 * @brief Check if tracing is currently enabled
 *
 * @return true if tracing is active, false otherwise
 */
#define TRACE_IS_ENABLED() (true)

#else // !FEATURE_MABUTRACE

//-----------------------------------------------------------------------------
// No-op stubs when MabuTrace is disabled
// These compile to nothing, ensuring zero runtime overhead.
//-----------------------------------------------------------------------------

#define TRACE_SCOPE(...)            do {} while(0)
#define TRACE_COUNTER(...)          do {} while(0)
#define TRACE_INSTANT(...)          do {} while(0)
#define TRACE_INIT(buffer_kb)       do { (void)(buffer_kb); } while(0)
#define TRACE_FLUSH()               do {} while(0)
#define TRACE_IS_ENABLED()          (false)

#endif // FEATURE_MABUTRACE
