/**
 * @file DeviceUUID.h
 * @brief Device identity for multi-device synchronization
 *
 * Generates a unique device identifier from the ESP32's MAC address.
 * Used for:
 * - Self-filtering in mDNS discovery (avoid connecting to self)
 * - Deterministic leader election (highest UUID wins)
 * - Peer identification in sync messages
 *
 * Format: "LW-AABBCCDDEEFF" (15 chars + null = 16 bytes)
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include <cstdint>

namespace lightwaveos {
namespace sync {

/**
 * @brief Device identity singleton
 *
 * Thread-safe after first initialization. The MAC address is read once
 * during first access and cached for the lifetime of the device.
 *
 * Usage:
 *   const char* myId = DeviceUUID::instance().toString();
 *   if (DeviceUUID::instance().isHigherThan(otherMac)) { ... }
 */
class DeviceUUID {
public:
    /**
     * @brief Get the singleton instance
     *
     * Thread-safe initialization using Meyer's singleton.
     * First call reads MAC address from WiFi driver.
     */
    static DeviceUUID& instance();

    // Delete copy/move constructors
    DeviceUUID(const DeviceUUID&) = delete;
    DeviceUUID& operator=(const DeviceUUID&) = delete;
    DeviceUUID(DeviceUUID&&) = delete;
    DeviceUUID& operator=(DeviceUUID&&) = delete;

    /**
     * @brief Get string representation
     * @return "LW-AABBCCDDEEFF" format (never null, always valid)
     */
    const char* toString() const { return m_uuidStr; }

    /**
     * @brief Get raw MAC address bytes
     * @return Pointer to 6-byte MAC address
     */
    const uint8_t* getBytes() const { return m_mac; }

    /**
     * @brief Compare with another MAC address for leader election
     *
     * Used by LeaderElection to determine which device should be leader.
     * Higher MAC address = higher priority = more likely to be leader.
     *
     * Comparison is done byte-by-byte, MSB first (big-endian order).
     *
     * @param other 6-byte MAC address to compare against
     * @return true if this device's MAC is higher (should be leader)
     */
    bool isHigherThan(const uint8_t* other) const;

    /**
     * @brief Compare with UUID string for leader election
     *
     * Parses the "LW-AABBCCDDEEFF" format and compares.
     *
     * @param otherUuidStr UUID string in "LW-XXXXXXXXXXXX" format
     * @return true if this device's UUID is higher
     */
    bool isHigherThan(const char* otherUuidStr) const;

    /**
     * @brief Check if a UUID string matches this device
     * @param uuidStr UUID string to compare
     * @return true if same device
     */
    bool matches(const char* uuidStr) const;

    /**
     * @brief Parse MAC address from UUID string
     *
     * Extracts the 6-byte MAC address from "LW-AABBCCDDEEFF" format.
     *
     * @param uuidStr Input UUID string
     * @param outMac Output buffer (6 bytes)
     * @return true if parsing succeeded
     */
    static bool parseUUID(const char* uuidStr, uint8_t* outMac);

private:
    DeviceUUID();
    ~DeviceUUID() = default;

    /**
     * @brief Initialize from hardware MAC address
     */
    void initialize();

    /**
     * @brief Convert MAC to hex string
     */
    void formatUUID();

    uint8_t m_mac[6];       // Raw MAC address bytes
    char m_uuidStr[16];     // "LW-AABBCCDDEEFF\0"
    bool m_initialized;     // Initialization flag
};

// Convenience macro for accessing the instance
#define DEVICE_UUID lightwaveos::sync::DeviceUUID::instance()

} // namespace sync
} // namespace lightwaveos
