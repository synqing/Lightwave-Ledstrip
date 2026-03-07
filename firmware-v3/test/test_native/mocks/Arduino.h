/**
 * @file Arduino.h
 * @brief Arduino core mock for native unit tests
 *
 * Provides minimal Arduino API compatibility (String, Serial, millis)
 * for testing ESP32 code without hardware dependencies.
 */

#pragma once

#ifdef NATIVE_BUILD

#include <string>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdarg>

// Arduino String compatibility — maps to std::string on native
using String = std::string;

// Serial mock — forwards to stdout for test visibility
struct MockSerial {
    void printf(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
    }
    void println(const char* msg = "") { ::printf("%s\n", msg); }
    void print(const char* msg) { ::printf("%s", msg); }
};

extern MockSerial Serial;

// Time functions (implemented in freertos_mock.cpp)
extern uint32_t millis();
extern void delay(uint32_t ms);

#endif // NATIVE_BUILD
