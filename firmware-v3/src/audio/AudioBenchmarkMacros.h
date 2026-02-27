/**
 * @file AudioBenchmarkMacros.h
 * @brief Zero-overhead timing macros for audio pipeline instrumentation
 *
 * When FEATURE_AUDIO_BENCHMARK is disabled, all macros expand to nothing
 * for zero runtime cost. When enabled, they provide microsecond-precision
 * timing with ~3.2us overhead per processHop() call.
 *
 * Usage pattern in AudioActor::processHop():
 * @code
 *   void AudioActor::processHop() {
 *       BENCH_DECL_TIMING();
 *       BENCH_START_FRAME();
 *
 *       BENCH_START_PHASE();
 *       // ... DC/AGC loop code ...
 *       BENCH_END_PHASE(dcAgcLoopUs);
 *
 *       BENCH_START_PHASE();
 *       // ... RMS compute code ...
 *       BENCH_END_PHASE(rmsComputeUs);
 *
 *       // ... more phases ...
 *
 *       BENCH_END_FRAME(&m_benchmarkRing);
 *   }
 * @endcode
 */

#pragma once

#include "../config/features.h"

#if FEATURE_AUDIO_BENCHMARK

// ESP32 high-resolution timer for microsecond precision
#ifdef ESP_PLATFORM
#include <esp_timer.h>
#define BENCH_GET_TIME_US() static_cast<uint64_t>(esp_timer_get_time())
#else
// Native test fallback using std::chrono
#include <chrono>
inline uint64_t _bench_get_time_us() {
    using namespace std::chrono;
    return duration_cast<microseconds>(
        steady_clock::now().time_since_epoch()
    ).count();
}
#define BENCH_GET_TIME_US() _bench_get_time_us()
#endif

#include "AudioBenchmarkMetrics.h"

/**
 * @brief Declare timing variables at function scope
 *
 * Must be called at the start of the function being instrumented.
 * Creates local variables for tracking phase timings.
 */
#define BENCH_DECL_TIMING() \
    uint64_t _bench_start = 0, _bench_phase_start = 0; \
    ::lightwaveos::audio::AudioBenchmarkSample _bench_sample = {}

/**
 * @brief Start timing for the entire frame
 *
 * Records the frame start timestamp. Call after BENCH_DECL_TIMING().
 */
#define BENCH_START_FRAME() \
    do { \
        _bench_start = BENCH_GET_TIME_US(); \
        _bench_sample.timestamp_us = static_cast<uint32_t>(_bench_start); \
    } while(0)

/**
 * @brief Start timing for a processing phase
 *
 * Call before each distinct processing section. The timing will be
 * captured when BENCH_END_PHASE is called.
 */
#define BENCH_START_PHASE() \
    _bench_phase_start = BENCH_GET_TIME_US()

/**
 * @brief End timing for a processing phase
 *
 * @param field The AudioBenchmarkSample field to store the elapsed time
 *              (e.g., dcAgcLoopUs, goertzelUs, etc.)
 */
#define BENCH_END_PHASE(field) \
    _bench_sample.field = static_cast<uint16_t>(BENCH_GET_TIME_US() - _bench_phase_start)

/**
 * @brief Set a flag or auxiliary field in the sample
 *
 * @param field The AudioBenchmarkSample field to set
 * @param val The value to assign
 */
#define BENCH_SET_FLAG(field, val) \
    _bench_sample.field = (val)

/**
 * @brief End timing for the frame and push sample to ring buffer
 *
 * Calculates total frame time and pushes the sample to the ring buffer.
 *
 * @param ring_ptr Pointer to the AudioBenchmarkRing to push to
 */
#define BENCH_END_FRAME(ring_ptr) \
    do { \
        _bench_sample.totalProcessUs = static_cast<uint16_t>(BENCH_GET_TIME_US() - _bench_start); \
        (ring_ptr)->push(_bench_sample); \
    } while(0)

/**
 * @brief Get the current sample for inspection (useful in tests)
 *
 * @return Reference to the in-progress sample
 */
#define BENCH_GET_SAMPLE() _bench_sample

#else // !FEATURE_AUDIO_BENCHMARK

//-----------------------------------------------------------------------------
// No-op stubs when benchmarking is disabled
// These compile to nothing, ensuring zero runtime overhead
//
// The macros use sizeof/decltype tricks to suppress "unused parameter"
// warnings while avoiding actual evaluation of potentially undefined symbols.
//-----------------------------------------------------------------------------

#define BENCH_DECL_TIMING()           do {} while(0)
#define BENCH_START_FRAME()           do {} while(0)
#define BENCH_START_PHASE()           do {} while(0)
#define BENCH_END_PHASE(...)          do {} while(0)
#define BENCH_SET_FLAG(...)           do {} while(0)
#define BENCH_END_FRAME(...)          do {} while(0)

// Forward declare an empty struct for BENCH_GET_SAMPLE when disabled
namespace lightwaveos::audio {
    struct AudioBenchmarkSample {
        uint32_t timestamp_us = 0;
        uint16_t totalProcessUs = 0;
    };
}
#define BENCH_GET_SAMPLE()            (::lightwaveos::audio::AudioBenchmarkSample{})

#endif // FEATURE_AUDIO_BENCHMARK
