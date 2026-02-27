// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WiFiCredentialManager.cpp
 * @brief Secure WiFi credential storage implementation
 *
 * LightwaveOS v2 - Network Subsystem
 */

#include "WiFiCredentialManager.h"

#if FEATURE_WEB_SERVER

#include "../core/persistence/NVSManager.h"
#include <WiFi.h>
#include <cstring>

using namespace lightwaveos::persistence;

namespace lightwaveos {
namespace network {

// ============================================================================
// CRC32 Lookup Table (IEEE 802.3 polynomial)
// ============================================================================

static const uint32_t CRC32_TABLE[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD706B3,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

// ============================================================================
// Lifecycle
// ============================================================================

bool WiFiCredentialManager::begin() {
    // Create mutex if not already created
    if (m_mutex == nullptr) {
        m_mutex = xSemaphoreCreateMutex();
        if (m_mutex == nullptr) {
            Serial.println("[WiFiCred] ERROR: Failed to create mutex");
            return false;
        }
    }

    // Initialize cache to clean state
    memset(&m_cache, 0, sizeof(m_cache));
    m_cache.version = SCHEMA_VERSION;
    m_cache.count = 0;

    // Load from NVS
    if (loadFromNVS()) {
        Serial.printf("[WiFiCred] Loaded %d saved networks from NVS\n", m_cache.count);
    } else {
        Serial.println("[WiFiCred] No saved networks found, starting fresh");
    }

    m_loaded = true;
    return true;
}

// ============================================================================
// Public API
// ============================================================================

std::vector<String> WiFiCredentialManager::getSavedSSIDs() {
    std::vector<String> result;

    if (m_mutex == nullptr) {
        return result;
    }

    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        Serial.println("[WiFiCred] WARNING: Mutex timeout in getSavedSSIDs");
        return result;
    }

    result.reserve(m_cache.count);
    for (uint8_t i = 0; i < m_cache.count && i < MAX_SAVED_NETWORKS; i++) {
        result.emplace_back(m_cache.networks[i].ssid);
    }

    xSemaphoreGive(m_mutex);
    return result;
}

bool WiFiCredentialManager::addNetwork(const char* ssid, const char* password) {
    if (ssid == nullptr || strlen(ssid) == 0 || strlen(ssid) > MAX_SSID_LENGTH) {
        Serial.println("[WiFiCred] ERROR: Invalid SSID");
        return false;
    }

    // Password can be empty for open networks
    size_t passLen = password ? strlen(password) : 0;
    if (passLen > MAX_PASSWORD_LENGTH) {
        Serial.println("[WiFiCred] ERROR: Password too long");
        return false;
    }

    if (m_mutex == nullptr) {
        Serial.println("[WiFiCred] ERROR: Not initialized");
        return false;
    }

    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(500)) != pdTRUE) {
        Serial.println("[WiFiCred] ERROR: Mutex timeout in addNetwork");
        return false;
    }

    bool success = false;

    // Check if network already exists (update case)
    int idx = findNetwork(ssid);
    if (idx >= 0) {
        // Update existing entry
        Serial.printf("[WiFiCred] Updating existing network: %s\n", ssid);
        obfuscatePassword(password ? password : "", m_cache.networks[idx].passwordObf);
        success = saveToNVS();
    } else {
        // Add new entry
        if (m_cache.count >= MAX_SAVED_NETWORKS) {
            Serial.println("[WiFiCred] ERROR: Network list full (max 8)");
            xSemaphoreGive(m_mutex);
            return false;
        }

        Serial.printf("[WiFiCred] Adding new network: %s\n", ssid);
        SavedNetworkNVS& entry = m_cache.networks[m_cache.count];
        entry.version = SCHEMA_VERSION;
        strncpy(entry.ssid, ssid, MAX_SSID_LENGTH);
        entry.ssid[MAX_SSID_LENGTH] = '\0';
        obfuscatePassword(password ? password : "", entry.passwordObf);
        entry.lastConnected = 0;

        m_cache.count++;
        success = saveToNVS();
    }

    xSemaphoreGive(m_mutex);
    return success;
}

bool WiFiCredentialManager::removeNetwork(const char* ssid) {
    if (ssid == nullptr || strlen(ssid) == 0) {
        return false;
    }

    if (m_mutex == nullptr) {
        return false;
    }

    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(500)) != pdTRUE) {
        Serial.println("[WiFiCred] ERROR: Mutex timeout in removeNetwork");
        return false;
    }

    int idx = findNetwork(ssid);
    if (idx < 0) {
        Serial.printf("[WiFiCred] Network not found: %s\n", ssid);
        xSemaphoreGive(m_mutex);
        return false;
    }

    Serial.printf("[WiFiCred] Removing network: %s (index %d)\n", ssid, idx);

    // Shift remaining entries down
    for (int i = idx; i < (int)m_cache.count - 1; i++) {
        m_cache.networks[i] = m_cache.networks[i + 1];
    }

    // Clear last entry
    memset(&m_cache.networks[m_cache.count - 1], 0, sizeof(SavedNetworkNVS));
    m_cache.count--;

    bool success = saveToNVS();

    xSemaphoreGive(m_mutex);
    return success;
}

bool WiFiCredentialManager::getCredentials(const char* ssid, char* passwordOut, size_t maxLen) {
    if (ssid == nullptr || passwordOut == nullptr || maxLen == 0) {
        return false;
    }

    if (m_mutex == nullptr) {
        return false;
    }

    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        Serial.println("[WiFiCred] WARNING: Mutex timeout in getCredentials");
        return false;
    }

    int idx = findNetwork(ssid);
    if (idx < 0) {
        xSemaphoreGive(m_mutex);
        return false;
    }

    // De-obfuscate password
    char plainPassword[MAX_PASSWORD_LENGTH + 1];
    deobfuscatePassword(m_cache.networks[idx].passwordObf, plainPassword);

    // Copy to output buffer
    strncpy(passwordOut, plainPassword, maxLen - 1);
    passwordOut[maxLen - 1] = '\0';

    // Clear temporary buffer
    memset(plainPassword, 0, sizeof(plainPassword));

    xSemaphoreGive(m_mutex);
    return true;
}

void WiFiCredentialManager::updateLastConnected(const char* ssid) {
    if (ssid == nullptr || m_mutex == nullptr) {
        return;
    }

    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }

    int idx = findNetwork(ssid);
    if (idx >= 0) {
        // Use millis/1000 as a pseudo-timestamp (relative to boot)
        // For a true Unix timestamp, we'd need NTP synchronization
        m_cache.networks[idx].lastConnected = millis() / 1000;
        saveToNVS();
        Serial.printf("[WiFiCred] Updated lastConnected for: %s\n", ssid);
    }

    xSemaphoreGive(m_mutex);
}

size_t WiFiCredentialManager::getNetworkCount() {
    if (m_mutex == nullptr) {
        return 0;
    }

    size_t count = 0;
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        count = m_cache.count;
        xSemaphoreGive(m_mutex);
    }
    return count;
}

bool WiFiCredentialManager::hasNetwork(const char* ssid) {
    if (ssid == nullptr || m_mutex == nullptr) {
        return false;
    }

    bool found = false;
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        found = (findNetwork(ssid) >= 0);
        xSemaphoreGive(m_mutex);
    }
    return found;
}

// ============================================================================
// NVS Operations
// ============================================================================

bool WiFiCredentialManager::loadFromNVS() {
    // Calculate expected size (excluding CRC field)
    size_t dataSize = sizeof(NetworkListNVS);

    NVSResult result = NVSManager::instance().loadBlob(
        NVS_NAMESPACE, NVS_KEY,
        &m_cache, dataSize
    );

    if (result == NVSResult::NOT_FOUND) {
        // Not an error - just no saved data
        return false;
    }

    if (result != NVSResult::OK) {
        Serial.printf("[WiFiCred] NVS load error: %s\n",
                      NVSManager::resultToString(result));
        return false;
    }

    // Validate version
    if (m_cache.version != SCHEMA_VERSION) {
        Serial.printf("[WiFiCred] Schema version mismatch: got %d, expected %d\n",
                      m_cache.version, SCHEMA_VERSION);
        memset(&m_cache, 0, sizeof(m_cache));
        m_cache.version = SCHEMA_VERSION;
        return false;
    }

    // Validate CRC
    size_t crcDataSize = offsetof(NetworkListNVS, crc32);
    uint32_t expectedCRC = calculateCRC32(&m_cache, crcDataSize);
    if (m_cache.crc32 != expectedCRC) {
        Serial.printf("[WiFiCred] CRC mismatch: got 0x%08X, expected 0x%08X\n",
                      m_cache.crc32, expectedCRC);
        memset(&m_cache, 0, sizeof(m_cache));
        m_cache.version = SCHEMA_VERSION;
        return false;
    }

    // Validate count
    if (m_cache.count > MAX_SAVED_NETWORKS) {
        Serial.printf("[WiFiCred] Invalid count: %d\n", m_cache.count);
        m_cache.count = 0;
        return false;
    }

    return true;
}

bool WiFiCredentialManager::saveToNVS() {
    // Update CRC before saving
    size_t crcDataSize = offsetof(NetworkListNVS, crc32);
    m_cache.crc32 = calculateCRC32(&m_cache, crcDataSize);

    NVSResult result = NVSManager::instance().saveBlob(
        NVS_NAMESPACE, NVS_KEY,
        &m_cache, sizeof(NetworkListNVS)
    );

    if (result != NVSResult::OK) {
        Serial.printf("[WiFiCred] NVS save error: %s\n",
                      NVSManager::resultToString(result));
        return false;
    }

    return true;
}

// ============================================================================
// Password Obfuscation
// ============================================================================

void WiFiCredentialManager::getXorKey(uint8_t* key) {
    // Use WiFi MAC address as XOR key (6 bytes)
    uint8_t mac[6];
    WiFi.macAddress(mac);

    // Copy MAC bytes as key
    memcpy(key, mac, 6);
}

void WiFiCredentialManager::obfuscatePassword(const char* plain, char* obfuscated) {
    uint8_t key[6];
    getXorKey(key);

    size_t len = strlen(plain);
    if (len > MAX_PASSWORD_LENGTH) {
        len = MAX_PASSWORD_LENGTH;
    }

    // XOR each character with rotating key
    for (size_t i = 0; i < len; i++) {
        obfuscated[i] = plain[i] ^ key[i % 6];
    }
    obfuscated[len] = '\0';

    // Fill remainder with key-derived noise (to mask password length)
    for (size_t i = len + 1; i <= MAX_PASSWORD_LENGTH; i++) {
        obfuscated[i] = key[i % 6];
    }
}

void WiFiCredentialManager::deobfuscatePassword(const char* obfuscated, char* plain) {
    uint8_t key[6];
    getXorKey(key);

    // XOR is symmetric, but we need to detect actual length
    // Look for null terminator in original (which was XORed)
    size_t len = 0;
    for (size_t i = 0; i <= MAX_PASSWORD_LENGTH; i++) {
        char c = obfuscated[i] ^ key[i % 6];
        plain[i] = c;
        if (c == '\0') {
            len = i;
            break;
        }
        len = i + 1;
    }
    plain[MAX_PASSWORD_LENGTH] = '\0';
}

// ============================================================================
// Utility
// ============================================================================

int WiFiCredentialManager::findNetwork(const char* ssid) {
    // Note: Must be called with mutex held
    for (uint8_t i = 0; i < m_cache.count && i < MAX_SAVED_NETWORKS; i++) {
        if (strcmp(m_cache.networks[i].ssid, ssid) == 0) {
            return i;
        }
    }
    return -1;
}

uint32_t WiFiCredentialManager::calculateCRC32(const void* data, size_t len) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < len; i++) {
        crc = CRC32_TABLE[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
    }

    return crc ^ 0xFFFFFFFF;
}

} // namespace network
} // namespace lightwaveos

#endif // FEATURE_WEB_SERVER
