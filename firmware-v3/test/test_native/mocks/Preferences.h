// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once

#ifdef NATIVE_BUILD
class Preferences {
public:
    bool begin(const char*, bool = false) { return true; }
    void end() {}

    uint8_t getUChar(const char*, uint8_t defaultValue = 0) { return defaultValue; }
    uint16_t getUShort(const char*, uint16_t defaultValue = 0) { return defaultValue; }
    uint32_t getUInt(const char*, uint32_t defaultValue = 0) { return defaultValue; }
    float getFloat(const char*, float defaultValue = 0.0f) { return defaultValue; }
    bool getBool(const char*, bool defaultValue = false) { return defaultValue; }

    void putUChar(const char*, uint8_t) {}
    void putUShort(const char*, uint16_t) {}
    void putUInt(const char*, uint32_t) {}
    void putFloat(const char*, float) {}
    void putBool(const char*, bool) {}
};
#else
#include <Preferences.h>
#endif
