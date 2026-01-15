/**
 * @file AudioRingBuffer.cpp
 * @brief Ring buffer implementation
 *
 * @author LightwaveOS Team
 * @version 1.0.0 - K1 Migration
 */

#include "AudioRingBuffer.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include "../AudioDebugConfig.h"

namespace lightwaveos {
namespace audio {
namespace k1 {

// #region agent log
// Native-safe debug logging using sample counter (not system timers)
// Rate-limited to prevent serial monitor spam (1-4 seconds depending on verbosity)
static void debug_log(uint8_t minVerbosity, const char* location, const char* message, const char* data_json, uint64_t t_samples) {
    auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
    if (dbgCfg.verbosity < minVerbosity) {
        return;  // Suppress if verbosity too low
    }
    // Rate limiting: only print if enough time has passed
    if (!dbgCfg.shouldPrint(minVerbosity)) {
        return;  // Suppress if too soon since last print
    }
    // Output JSON to serial with special prefix for parsing
    // Convert t_samples to microseconds for logging: t_us = (t_samples * 1000000ULL) / 16000
    uint64_t t_us = (t_samples * 1000000ULL) / 16000;
    printf("DEBUG_JSON:{\"location\":\"%s\",\"message\":\"%s\",\"data\":%s,\"timestamp\":%llu}\n",
           location, message, data_json, (unsigned long long)t_us);
}
// #endregion

#ifndef NATIVE_BUILD
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

AudioRingBuffer::AudioRingBuffer()
    : m_buffer(nullptr)
    , m_capacity(0)
    , m_writePos(0)
    , m_endCounter(0)
    , m_initialized(false)
{
}

AudioRingBuffer::~AudioRingBuffer() {
    reset();
}

bool AudioRingBuffer::init(size_t capacity_samples) {
    if (m_initialized) {
        reset();
    }

    // Allocate buffer
    m_buffer = static_cast<int16_t*>(malloc(capacity_samples * sizeof(int16_t)));
    if (m_buffer == nullptr) {
        return false;
    }

    m_capacity = capacity_samples;
    m_writePos = 0;
    m_endCounter = 0;
    m_initialized = true;

    // Zero buffer
    memset(m_buffer, 0, capacity_samples * sizeof(int16_t));

    return true;
}

void AudioRingBuffer::reset() {
    if (m_buffer != nullptr) {
        free(m_buffer);
        m_buffer = nullptr;
    }
    m_capacity = 0;
    m_writePos = 0;
    m_endCounter = 0;
    m_initialized = false;
}

void AudioRingBuffer::push(const int16_t* samples, size_t n, uint64_t end_sample_counter) {
    if (!m_initialized || samples == nullptr || n == 0) {
        return;
    }

    // Write samples with wrap handling
    for (size_t i = 0; i < n; i++) {
        m_buffer[m_writePos] = samples[i];
        m_writePos = (m_writePos + 1) % m_capacity;
    }

    m_endCounter = end_sample_counter;
}

bool AudioRingBuffer::copyLast(size_t N, int16_t* dst) const {
    // #region agent log
    static uint32_t ring_log_counter = 0;
    if ((ring_log_counter++ % 125) == 0) {
        char ring_data[256];
        snprintf(ring_data, sizeof(ring_data),
            "{\"N\":%zu,\"capacity\":%zu,\"writePos\":%zu,\"initialized\":%d,\"hypothesisId\":\"K\"}",
            N, m_capacity, m_writePos, m_initialized ? 1 : 0);
        debug_log(3, "AudioRingBuffer.cpp:copyLast", "ring_buffer_state", ring_data, 0);
    }
    // #endregion
    
    if (!m_initialized || dst == nullptr || N == 0 || N > m_capacity) {
        return false;
    }

    // Calculate read start position (N samples before write position)
    // Handle wrap: if writePos < N, we need to wrap around
    size_t readStart;
    if (m_writePos >= N) {
        readStart = m_writePos - N;
    } else {
        readStart = m_capacity - (N - m_writePos);
    }

    // Copy with wrap handling
    if (readStart + N <= m_capacity) {
        // Single contiguous copy
        memcpy(dst, &m_buffer[readStart], N * sizeof(int16_t));
    } else {
        // Two-part copy (wraps around)
        size_t firstPart = m_capacity - readStart;
        size_t secondPart = N - firstPart;
        memcpy(dst, &m_buffer[readStart], firstPart * sizeof(int16_t));
        memcpy(dst + firstPart, m_buffer, secondPart * sizeof(int16_t));
    }

    return true;
}

} // namespace k1
} // namespace audio
} // namespace lightwaveos

