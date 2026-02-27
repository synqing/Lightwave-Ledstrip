// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WiFiCredentialManager.h
 * @brief Secure WiFi credential storage for LightwaveOS v2
 *
 * Features:
 * - NVS-backed persistent storage via NVSManager
 * - XOR obfuscation with device MAC (not encryption, but prevents casual viewing)
 * - CRC32 integrity checking
 * - Thread-safe with mutex protection
 * - Tracks last connected timestamp per network
 *
 * Security Notes:
 * - Passwords are XOR-obfuscated, NOT encrypted
 * - Never expose passwords via API (getSavedSSIDs returns names only)
 * - Rate limiting should be applied at the HTTP layer
 *
 * Usage:
 * @code
 * // Initialize at startup
 * wifiCredentialMgr.begin();
 *
 * // Save a network
 * wifiCredentialMgr.addNetwork("MySSID", "MyPassword");
 *
 * // Get list of saved SSIDs (no passwords)
 * auto ssids = wifiCredentialMgr.getSavedSSIDs();
 *
 * // Get credentials for connection
 * char password[65];
 * if (wifiCredentialMgr.getCredentials("MySSID", password, sizeof(password))) {
 *     // Connect with password
 * }
 * @endcode
 *
 * @author LightwaveOS Team
 * @version 1.0.0
 */

#pragma once

#include "../config/features.h"

#if FEATURE_WEB_SERVER

#include <Arduino.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace lightwaveos {
namespace network {

// ============================================================================
// Constants
// ============================================================================

/// Maximum number of saved networks
static constexpr size_t MAX_SAVED_NETWORKS = 8;

/// Maximum SSID length (WiFi spec is 32 bytes)
static constexpr size_t MAX_SSID_LENGTH = 32;

/// Maximum password length (WPA2 max is 63 chars + null)
static constexpr size_t MAX_PASSWORD_LENGTH = 64;

// ============================================================================
// Data Structures (NVS-compatible, packed)
// ============================================================================

/**
 * @brief Single saved network entry (NVS format)
 *
 * Uses packed struct for consistent NVS storage across builds.
 */
struct __attribute__((packed)) SavedNetworkNVS {
    uint8_t version;                        // Schema version (1)
    char ssid[MAX_SSID_LENGTH + 1];         // SSID (null-terminated)
    char passwordObf[MAX_PASSWORD_LENGTH + 1]; // XOR-obfuscated password
    uint32_t lastConnected;                 // Unix timestamp of last successful connection
    uint32_t crc32;                         // CRC32 of preceding fields
};

/**
 * @brief Network list container (NVS format)
 */
struct __attribute__((packed)) NetworkListNVS {
    uint8_t version;                        // Schema version (1)
    uint8_t count;                          // Number of saved networks
    SavedNetworkNVS networks[MAX_SAVED_NETWORKS];
    uint32_t crc32;                         // CRC32 of preceding fields
};

// ============================================================================
// WiFiCredentialManager Class
// ============================================================================

/**
 * @brief Thread-safe WiFi credential storage manager
 *
 * Singleton pattern for consistent access across the system.
 */
class WiFiCredentialManager {
public:
    // ========================================================================
    // Singleton Access
    // ========================================================================

    /**
     * @brief Get singleton instance
     * @return Reference to the WiFiCredentialManager
     */
    static WiFiCredentialManager& getInstance() {
        static WiFiCredentialManager instance;
        return instance;
    }

    // Delete copy/move operations
    WiFiCredentialManager(const WiFiCredentialManager&) = delete;
    WiFiCredentialManager& operator=(const WiFiCredentialManager&) = delete;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    /**
     * @brief Initialize credential manager
     *
     * Creates mutex and loads cached data from NVS.
     *
     * @return true if initialization successful
     */
    bool begin();

    // ========================================================================
    // Public API
    // ========================================================================

    /**
     * @brief Get list of saved SSIDs (NO passwords)
     *
     * Thread-safe. Returns a copy of the SSID list.
     *
     * @return Vector of saved SSID strings
     */
    std::vector<String> getSavedSSIDs();

    /**
     * @brief Add or update a network
     *
     * If SSID already exists, updates the password.
     * If list is full, returns false.
     *
     * @param ssid Network SSID (1-32 chars)
     * @param password Network password (8-64 chars for WPA2, empty for open)
     * @return true if saved successfully
     */
    bool addNetwork(const char* ssid, const char* password);

    /**
     * @brief Remove a saved network
     *
     * @param ssid SSID to remove
     * @return true if network was found and removed
     */
    bool removeNetwork(const char* ssid);

    /**
     * @brief Get credentials for a specific network
     *
     * Used internally for WiFi connection. De-obfuscates password.
     *
     * @param ssid SSID to look up
     * @param passwordOut Buffer to receive plaintext password
     * @param maxLen Size of passwordOut buffer (should be MAX_PASSWORD_LENGTH+1)
     * @return true if network found and password copied
     */
    bool getCredentials(const char* ssid, char* passwordOut, size_t maxLen);

    /**
     * @brief Update last connected timestamp
     *
     * Call this after successful WiFi connection.
     *
     * @param ssid SSID that connected successfully
     */
    void updateLastConnected(const char* ssid);

    /**
     * @brief Get count of saved networks
     * @return Number of saved networks (0-8)
     */
    size_t getNetworkCount();

    /**
     * @brief Check if a network is saved
     * @param ssid SSID to check
     * @return true if network exists in saved list
     */
    bool hasNetwork(const char* ssid);

private:
    // ========================================================================
    // Private Constructor (Singleton)
    // ========================================================================

    WiFiCredentialManager() = default;

    // ========================================================================
    // NVS Operations
    // ========================================================================

    /**
     * @brief Load network list from NVS
     * @return true if loaded successfully
     */
    bool loadFromNVS();

    /**
     * @brief Save network list to NVS
     * @return true if saved successfully
     */
    bool saveToNVS();

    // ========================================================================
    // Password Obfuscation
    // ========================================================================

    /**
     * @brief Obfuscate password using XOR with device MAC
     * @param plain Plaintext password
     * @param obfuscated Output buffer for obfuscated password (must be MAX_PASSWORD_LENGTH+1)
     */
    void obfuscatePassword(const char* plain, char* obfuscated);

    /**
     * @brief De-obfuscate password (same operation as obfuscate, XOR is symmetric)
     * @param obfuscated Obfuscated password
     * @param plain Output buffer for plaintext password (must be MAX_PASSWORD_LENGTH+1)
     */
    void deobfuscatePassword(const char* obfuscated, char* plain);

    /**
     * @brief Get XOR key derived from device MAC
     * @param key Output buffer (6 bytes minimum)
     */
    void getXorKey(uint8_t* key);

    // ========================================================================
    // Utility
    // ========================================================================

    /**
     * @brief Find network index by SSID
     * @param ssid SSID to find
     * @return Index (0-7) or -1 if not found
     */
    int findNetwork(const char* ssid);

    /**
     * @brief Calculate CRC32 for data integrity
     * @param data Pointer to data
     * @param len Length in bytes
     * @return CRC32 checksum
     */
    uint32_t calculateCRC32(const void* data, size_t len);

    // ========================================================================
    // State
    // ========================================================================

    SemaphoreHandle_t m_mutex = nullptr;    ///< Mutex for thread safety
    NetworkListNVS m_cache;                 ///< In-memory cache
    bool m_loaded = false;                  ///< True if cache loaded from NVS

    // ========================================================================
    // NVS Configuration
    // ========================================================================

    static constexpr const char* NVS_NAMESPACE = "wificred";
    static constexpr const char* NVS_KEY = "netlist";
    static constexpr uint8_t SCHEMA_VERSION = 1;
};

// ============================================================================
// Convenience Macro
// ============================================================================

/**
 * @brief Convenience macro to access WiFiCredentialManager singleton
 */
#define WIFI_CREDENTIALS lightwaveos::network::WiFiCredentialManager::getInstance()

} // namespace network
} // namespace lightwaveos

#endif // FEATURE_WEB_SERVER
