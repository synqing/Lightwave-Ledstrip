/**
 * @file EsV11Shim.h
 * @brief Minimal shim layer for vendored Emotiscope v1.1_320 DSP code.
 *
 * Goals:
 * - Keep ES maths and control flow intact.
 * - Provide only the dependencies required to compile inside LWLS v2.
 */

#pragma once

#include <cstdint>
#include <cstring>

// Arduino-like helpers (available on device; mocked on native)
#ifdef ARDUINO
#include <Arduino.h>
#endif

// ESP DSP (device). Native builds provide lightweight stubs below.
#if !defined(NATIVE_BUILD)
#include <esp_dsp.h>
#endif

// ES code expects these globals for timing.
extern uint32_t t_now_us;
extern uint32_t t_now_ms;

inline void esv11_set_time(uint64_t now_us_64, uint32_t now_ms_32) {
    t_now_us = static_cast<uint32_t>(now_us_64 & 0xFFFFFFFFu);
    t_now_ms = now_ms_32;
}

// ES uses profile_function([&]{...}, __func__) for instrumentation.
// In LWLS this backend runs without profiling overhead by default.
template <typename Fn>
inline void profile_function(Fn&& fn, const char* /*name*/) {
    fn();
}

// Native stubs for esp-dsp helpers used by ES.
#if defined(NATIVE_BUILD)
inline void dsps_mulc_f32(const float* src, float* dst, int len, float c, int src_stride, int dst_stride) {
    for (int i = 0; i < len; ++i) {
        dst[i * dst_stride] = src[i * src_stride] * c;
    }
}
#endif

