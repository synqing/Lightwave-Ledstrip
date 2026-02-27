// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
// ============================================================================
// SimHal - Simulator Hardware Implementation
// ============================================================================
// Mock implementation for testing UI without hardware
// ============================================================================

#ifdef SIMULATOR_BUILD

#include "SimHal.h"
#include <cstdio>
#include <cstdarg>
#include <chrono>

namespace EspHal {

// Mock heap tracking
static uint32_t s_mockFreeHeap = 500000;  // Start with 500KB free
static uint32_t s_mockMinFreeHeap = 500000;
static uint32_t s_mockMaxAllocHeap = 500000;

// Mock battery state
static int8_t s_mockBatteryLevel = 85;  // 85% charged
static bool s_mockCharging = false;
static float s_mockBatteryVoltage = 4.1f;  // 4.1V

// Time tracking
static std::chrono::steady_clock::time_point s_startTime = std::chrono::steady_clock::now();

uint32_t getFreeHeap() {
    return s_mockFreeHeap;
}

uint32_t getMinFreeHeap() {
    return s_mockMinFreeHeap;
}

uint32_t getMaxAllocHeap() {
    return s_mockMaxAllocHeap;
}

int8_t getBatteryLevel() {
    return s_mockBatteryLevel;
}

bool isCharging() {
    return s_mockCharging;
}

float getBatteryVoltage() {
    return s_mockBatteryVoltage;
}

uint32_t millis() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_startTime);
    return static_cast<uint32_t>(elapsed.count());
}

void log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void logV(const char* format, va_list args) {
    vprintf(format, args);
}

// Simulator-specific functions for test control
namespace Simulator {
    void setMockHeap(uint32_t free, uint32_t minFree, uint32_t maxAlloc) {
        s_mockFreeHeap = free;
        s_mockMinFreeHeap = minFree;
        s_mockMaxAllocHeap = maxAlloc;
    }
    
    void setMockBattery(int8_t level, bool charging, float voltage) {
        s_mockBatteryLevel = level;
        s_mockCharging = charging;
        s_mockBatteryVoltage = voltage;
    }
    
    void resetTime() {
        s_startTime = std::chrono::steady_clock::now();
    }
}

}  // namespace EspHal

#endif  // SIMULATOR_BUILD

