// ============================================================================
// EspHal - ESP32 Hardware Implementation
// ============================================================================

#include "EspHal.h"

#ifndef SIMULATOR_BUILD
// ESP32 build - use real hardware APIs
#include <ESP.h>
#include <M5Unified.h>
#include <Arduino.h>

namespace EspHal {

uint32_t getFreeHeap() {
    return ESP.getFreeHeap();
}

uint32_t getMinFreeHeap() {
    return ESP.getMinFreeHeap();
}

uint32_t getMaxAllocHeap() {
    return ESP.getMaxAllocHeap();
}

int8_t getBatteryLevel() {
    return M5.Power.getBatteryLevel();
}

bool isCharging() {
    return M5.Power.isCharging();
}

float getBatteryVoltage() {
    // M5.Power.getBatteryVoltage() returns millivolts, convert to volts
    return M5.Power.getBatteryVoltage() / 1000.0f;
}

uint32_t millis() {
    return ::millis();
}

void log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    Serial.vprintf(format, args);
    va_end(args);
}

void logV(const char* format, va_list args) {
    Serial.vprintf(format, args);
}

}  // namespace EspHal

#else
// Simulator build - this file should not be compiled in simulator
// Use SimHal.cpp instead
#error "EspHal.cpp should not be compiled in SIMULATOR_BUILD mode"
#endif
