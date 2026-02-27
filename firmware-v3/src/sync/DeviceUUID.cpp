/**
 * @file DeviceUUID.cpp
 * @brief Device identity implementation
 */

#include "DeviceUUID.h"
#include <cstring>
#include <cstdio>

#ifdef NATIVE_BUILD
// Native build - use mock MAC
#include <cstdlib>
#else
// ESP32 build - use WiFi driver for MAC
#include <WiFi.h>
#endif

namespace lightwaveos {
namespace sync {

DeviceUUID& DeviceUUID::instance() {
    static DeviceUUID instance;
    if (!instance.m_initialized) {
        instance.initialize();
    }
    return instance;
}

DeviceUUID::DeviceUUID()
    : m_mac{0}
    , m_uuidStr{0}
    , m_initialized(false)
{
}

void DeviceUUID::initialize() {
    if (m_initialized) return;

#ifdef NATIVE_BUILD
    // Native build: Use random or fixed test MAC
    // This allows unit tests to run without WiFi hardware
    const char* testMac = getenv("LIGHTWAVE_TEST_MAC");
    if (testMac) {
        // Parse environment variable if set: "AA:BB:CC:DD:EE:FF"
        sscanf(testMac, "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx",
               &m_mac[0], &m_mac[1], &m_mac[2],
               &m_mac[3], &m_mac[4], &m_mac[5]);
    } else {
        // Default test MAC (unique per test run would be better)
        m_mac[0] = 0xDE;
        m_mac[1] = 0xAD;
        m_mac[2] = 0xBE;
        m_mac[3] = 0xEF;
        m_mac[4] = 0x00;
        m_mac[5] = 0x01;
    }
#else
    // ESP32: Read actual MAC address from WiFi driver
    // WiFi.mode() must have been called before this works
    WiFi.macAddress(m_mac);
#endif

    formatUUID();
    m_initialized = true;
}

void DeviceUUID::formatUUID() {
    // Format: "LW-AABBCCDDEEFF"
    snprintf(m_uuidStr, sizeof(m_uuidStr),
             "LW-%02X%02X%02X%02X%02X%02X",
             m_mac[0], m_mac[1], m_mac[2],
             m_mac[3], m_mac[4], m_mac[5]);
}

bool DeviceUUID::isHigherThan(const uint8_t* other) const {
    if (!other) return true;

    // Compare byte-by-byte, MSB first (big-endian)
    for (int i = 0; i < 6; i++) {
        if (m_mac[i] > other[i]) return true;
        if (m_mac[i] < other[i]) return false;
    }
    return false; // Equal means not higher
}

bool DeviceUUID::isHigherThan(const char* otherUuidStr) const {
    if (!otherUuidStr) return true;

    uint8_t otherMac[6];
    if (!parseUUID(otherUuidStr, otherMac)) {
        return true; // Invalid UUID, we're higher by default
    }

    return isHigherThan(otherMac);
}

bool DeviceUUID::matches(const char* uuidStr) const {
    if (!uuidStr) return false;
    return strcmp(m_uuidStr, uuidStr) == 0;
}

bool DeviceUUID::parseUUID(const char* uuidStr, uint8_t* outMac) {
    if (!uuidStr || !outMac) return false;

    // Validate prefix "LW-"
    if (strncmp(uuidStr, "LW-", 3) != 0) return false;

    // Parse 12 hex characters
    const char* hex = uuidStr + 3;
    if (strlen(hex) != 12) return false;

    for (int i = 0; i < 6; i++) {
        char byte[3] = { hex[i*2], hex[i*2 + 1], '\0' };
        char* endPtr;
        long value = strtol(byte, &endPtr, 16);
        if (*endPtr != '\0' || value < 0 || value > 255) {
            return false;
        }
        outMac[i] = static_cast<uint8_t>(value);
    }

    return true;
}

} // namespace sync
} // namespace lightwaveos
