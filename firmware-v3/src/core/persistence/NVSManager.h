// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file NVSManager.h
 * @brief Generic NVS wrapper for persistent key-value storage
 *
 * LightwaveOS v2 - Persistence System
 *
 * Provides a thread-safe wrapper around ESP-IDF NVS for storing
 * configuration data that persists across reboots.
 *
 * Features:
 * - Automatic NVS initialization with error recovery
 * - Blob storage for arbitrary data structures
 * - Scalar storage for single values
 * - CRC32 checksum validation
 * - Thread-safe operations
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include <cstdint>
#include <cstddef>

namespace lightwaveos {
namespace persistence {

// ==================== Result Codes ====================

enum class NVSResult : uint8_t {
    OK = 0,                 // Operation successful
    NOT_INITIALIZED,        // NVS not initialized
    NOT_FOUND,              // Key not found
    INVALID_HANDLE,         // Failed to open namespace
    READ_ERROR,             // Failed to read data
    WRITE_ERROR,            // Failed to write data
    CHECKSUM_ERROR,         // Data checksum validation failed
    SIZE_MISMATCH,          // Stored size differs from expected
    COMMIT_FAILED,          // Failed to commit changes
    FLASH_ERROR             // Flash storage error
};

// ==================== NVSManager Class ====================

/**
 * @brief Singleton NVS wrapper for persistent storage
 *
 * Usage:
 *   NVSManager& nvs = NVSManager::instance();
 *   nvs.init();
 *
 *   // Save a struct
 *   MyConfig config;
 *   nvs.saveBlob("myapp", "config", &config, sizeof(config));
 *
 *   // Load a struct
 *   nvs.loadBlob("myapp", "config", &config, sizeof(config));
 *
 *   // Save a single value
 *   nvs.saveUint8("myapp", "brightness", 128);
 *
 *   // Load a single value (with default)
 *   uint8_t brightness = nvs.loadUint8("myapp", "brightness", 255);
 */
class NVSManager {
public:
    /**
     * @brief Get the singleton instance
     */
    static NVSManager& instance();

    // Prevent copying
    NVSManager(const NVSManager&) = delete;
    NVSManager& operator=(const NVSManager&) = delete;

    // ==================== Initialization ====================

    /**
     * @brief Initialize NVS flash storage
     *
     * Handles NVS_NO_FREE_PAGES and NVS_NEW_VERSION_FOUND errors
     * by erasing and reinitializing NVS.
     *
     * @return true if NVS is ready for use
     */
    bool init();

    /**
     * @brief Check if NVS is initialized
     */
    bool isInitialized() const { return m_initialized; }

    // ==================== Blob Operations ====================

    /**
     * @brief Save arbitrary data to NVS
     *
     * @param ns Namespace (max 15 chars)
     * @param key Key name (max 15 chars)
     * @param data Pointer to data buffer
     * @param size Size of data in bytes
     * @return NVSResult indicating success or failure
     */
    NVSResult saveBlob(const char* ns, const char* key, const void* data, size_t size);

    /**
     * @brief Load arbitrary data from NVS
     *
     * @param ns Namespace (max 15 chars)
     * @param key Key name (max 15 chars)
     * @param data Pointer to destination buffer
     * @param size Expected size of data
     * @return NVSResult indicating success or failure
     */
    NVSResult loadBlob(const char* ns, const char* key, void* data, size_t size);

    /**
     * @brief Get the size of a stored blob
     *
     * @param ns Namespace
     * @param key Key name
     * @param outSize Pointer to receive the size
     * @return NVSResult indicating success or failure
     */
    NVSResult getBlobSize(const char* ns, const char* key, size_t* outSize);

    /**
     * @brief Erase a key from NVS
     *
     * @param ns Namespace
     * @param key Key name
     * @return NVSResult indicating success or failure
     */
    NVSResult eraseKey(const char* ns, const char* key);

    // ==================== Scalar Operations ====================

    /**
     * @brief Save an 8-bit unsigned value
     *
     * @param ns Namespace
     * @param key Key name
     * @param value Value to save
     * @return NVSResult indicating success or failure
     */
    NVSResult saveUint8(const char* ns, const char* key, uint8_t value);

    /**
     * @brief Load an 8-bit unsigned value with default
     *
     * @param ns Namespace
     * @param key Key name
     * @param defaultVal Value to return if key not found
     * @return The loaded value, or defaultVal if not found
     */
    uint8_t loadUint8(const char* ns, const char* key, uint8_t defaultVal);

    /**
     * @brief Save a 16-bit unsigned value
     */
    NVSResult saveUint16(const char* ns, const char* key, uint16_t value);

    /**
     * @brief Load a 16-bit unsigned value with default
     */
    uint16_t loadUint16(const char* ns, const char* key, uint16_t defaultVal);

    /**
     * @brief Save a 32-bit unsigned value
     */
    NVSResult saveUint32(const char* ns, const char* key, uint32_t value);

    /**
     * @brief Load a 32-bit unsigned value with default
     */
    uint32_t loadUint32(const char* ns, const char* key, uint32_t defaultVal);

    // ==================== Utility ====================

    /**
     * @brief Calculate CRC32 checksum for data validation
     *
     * @param data Pointer to data
     * @param size Size of data in bytes
     * @return CRC32 checksum value
     */
    static uint32_t calculateCRC32(const void* data, size_t size);

    /**
     * @brief Convert NVSResult to human-readable string
     */
    static const char* resultToString(NVSResult result);

    /**
     * @brief Get NVS partition usage statistics
     *
     * @param usedEntries Output: number of used entries
     * @param freeEntries Output: number of free entries
     * @return true if stats retrieved successfully
     */
    bool getStats(size_t* usedEntries, size_t* freeEntries);

private:
    // Private constructor for singleton
    NVSManager();
    ~NVSManager() = default;

    bool m_initialized;

    // CRC32 lookup table (precomputed)
    static const uint32_t CRC32_TABLE[256];
};

// ==================== Global Access ====================

/**
 * @brief Quick access to NVSManager singleton
 */
#define NVS_MANAGER (::lightwaveos::persistence::NVSManager::instance())

} // namespace persistence
} // namespace lightwaveos
