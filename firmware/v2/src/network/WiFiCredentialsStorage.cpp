/**
 * @file WiFiCredentialsStorage.cpp
 * @brief NVS-based storage implementation for WiFi network credentials
 */

#include "WiFiCredentialsStorage.h"

#if FEATURE_WEB_SERVER

#include "../config/network_config.h"

#define LW_LOG_TAG "WiFiCreds"
#include "utils/Log.h"

namespace lightwaveos {
namespace network {

WiFiCredentialsStorage::WiFiCredentialsStorage()
    : m_initialized(false)
    , m_networkCount(0)
{
}

WiFiCredentialsStorage::~WiFiCredentialsStorage() {
    end();
}

bool WiFiCredentialsStorage::begin() {
    if (m_initialized) {
        LW_LOGW("WiFiCredentialsStorage already initialized");
        return true;
    }

    // Open NVS namespace (read-write mode)
    if (!m_prefs.begin("wifi_creds", false)) {
        LW_LOGE("Failed to open NVS namespace 'wifi_creds'");
        return false;
    }

    // Load network count (defaults to 0 if not set)
    m_networkCount = m_prefs.getUChar("count", 0);
    
    // Validate count (should not exceed MAX_NETWORKS)
    if (m_networkCount > MAX_NETWORKS) {
        LW_LOGW("Invalid network count in NVS (%d > %d), resetting", m_networkCount, MAX_NETWORKS);
        m_networkCount = 0;
        m_prefs.putUChar("count", 0);
    }

    m_initialized = true;

    // Sanity check: verify count matches actual stored entries
    uint8_t actualCount = 0;
    for (uint8_t i = 0; i < MAX_NETWORKS; i++) {
        String key = getNetworkKey(i);
        String jsonStr = m_prefs.getString(key.c_str(), "");
        if (jsonStr.length() == 0) {
            continue;
        }

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, jsonStr);
        if (error) {
            continue;
        }

        const char* ssid = doc["ssid"];
        if (ssid && strlen(ssid) > 0) {
            actualCount++;
        }
    }

    if (actualCount != m_networkCount) {
        LW_LOGW("NVS count mismatch: stored=%d actual=%d - compacting", m_networkCount, actualCount);
        compactStorage();
    }

    LW_LOGI("WiFiCredentialsStorage initialized - %d networks saved", m_networkCount);
    return m_initialized;
}

void WiFiCredentialsStorage::end() {
    if (m_initialized) {
        m_prefs.end();
        m_initialized = false;
    }
}

String WiFiCredentialsStorage::getNetworkKey(uint8_t index) const {
    char key[16];
    snprintf(key, sizeof(key), "net_%d", index);
    return String(key);
}

bool WiFiCredentialsStorage::saveNetwork(const String& ssid, const String& password) {
    if (!m_initialized) {
        LW_LOGE("WiFiCredentialsStorage not initialized");
        return false;
    }

    // Validate SSID length (WiFi standard: max 32 chars)
    if (ssid.length() == 0 || ssid.length() > 32) {
        LW_LOGE("Invalid SSID length: %d (must be 1-32 chars)", ssid.length());
        return false;
    }

    // Validate password length (WPA2 standard: min 8, max 64 chars)
    if (password.length() > 0 && password.length() < 8) {
        LW_LOGE("Invalid password length: %d (must be 0 or >=8 chars)", password.length());
        return false;
    }
    if (password.length() > 64) {
        LW_LOGE("Invalid password length: %d (max 64 chars)", password.length());
        return false;
    }

    // Check if network already exists - update instead of creating duplicate
    uint8_t existingIndex = findNetworkIndex(ssid);
    if (existingIndex != 255) {
        // Network exists - update it
        LW_LOGI("Updating existing network: %s (index %d)", ssid.c_str(), existingIndex);
        String key = getNetworkKey(existingIndex);
        
        // Create JSON document
        JsonDocument doc;
        doc["ssid"] = ssid;
        doc["password"] = password;
        
        String jsonStr;
        serializeJson(doc, jsonStr);
        
        if (m_prefs.putString(key.c_str(), jsonStr)) {
            LW_LOGI("Network updated successfully");
            return true;
        } else {
            LW_LOGE("Failed to update network in NVS");
            return false;
        }
    }

    // Network doesn't exist - check if we have space
    if (m_networkCount >= MAX_NETWORKS) {
        LW_LOGE("Cannot save network - storage full (%d/%d networks)", m_networkCount, MAX_NETWORKS);
        return false;
    }

    // Save new network at next available index
    String key = getNetworkKey(m_networkCount);
    
    // Create JSON document
    JsonDocument doc;
    doc["ssid"] = ssid;
    doc["password"] = password;
    
    String jsonStr;
    serializeJson(doc, jsonStr);
    
    // Estimate storage size (rough check)
    size_t estimatedSize = jsonStr.length() + key.length() + 10;  // Key + value + overhead
    if (estimatedSize > 400) {  // Conservative limit (NVS namespace ~4000 bytes total)
        LW_LOGE("Network credential too large (estimated %d bytes) - cannot save", estimatedSize);
        return false;
    }
    
    if (m_prefs.putString(key.c_str(), jsonStr)) {
        m_networkCount++;
        updateNetworkCount();
        LW_LOGI("Network saved successfully: %s (index %d, total: %d)", 
                ssid.c_str(), m_networkCount - 1, m_networkCount);
        return true;
    } else {
        LW_LOGE("Failed to save network to NVS (storage full or error)");
        return false;
    }
}

uint8_t WiFiCredentialsStorage::loadNetworks(NetworkCredential* networks, uint8_t maxNetworks) {
    if (!m_initialized) {
        LW_LOGE("WiFiCredentialsStorage not initialized");
        return 0;
    }

    if (networks == nullptr || maxNetworks == 0) {
        return 0;
    }

    uint8_t loaded = 0;
    for (uint8_t i = 0; i < m_networkCount && loaded < maxNetworks; i++) {
        String key = getNetworkKey(i);
        String jsonStr = m_prefs.getString(key.c_str(), "");
        
        if (jsonStr.length() == 0) {
            // Gap in storage (network was deleted but not compacted)
            continue;
        }
        
        // Parse JSON
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, jsonStr);
        
        if (error) {
            LW_LOGW("Failed to parse network at index %d: %s", i, error.c_str());
            continue;
        }
        
        const char* ssid = doc["ssid"];
        const char* password = doc["password"];
        
        if (ssid && strlen(ssid) > 0) {
            networks[loaded].ssid = String(ssid);
            networks[loaded].password = password ? String(password) : String("");
            loaded++;
        }
    }

    if (loaded > 0) {
        LW_LOGD("Loaded %d networks from NVS", loaded);
    }
    return loaded;
}

bool WiFiCredentialsStorage::deleteNetwork(const String& ssid) {
    if (!m_initialized) {
        LW_LOGE("WiFiCredentialsStorage not initialized");
        return false;
    }

    uint8_t index = findNetworkIndex(ssid);
    if (index == 255) {
        LW_LOGW("Network not found: %s", ssid.c_str());
        return false;
    }

    // Delete the network by removing its key
    String key = getNetworkKey(index);
    if (m_prefs.remove(key.c_str())) {
        m_networkCount--;
        updateNetworkCount();
        LW_LOGI("Network deleted: %s (index %d, remaining: %d)", 
                ssid.c_str(), index, m_networkCount);
        
        // Compact storage to remove gaps (keeps storage clean)
        if (m_networkCount > 0) {
            compactStorage();
        }
        return true;
    } else {
        LW_LOGE("Failed to delete network from NVS");
        return false;
    }
}

uint8_t WiFiCredentialsStorage::getNetworkCount() const {
    return m_networkCount;
}

bool WiFiCredentialsStorage::getNetwork(uint8_t index, NetworkCredential& network) {
    if (!m_initialized) {
        return false;
    }

    if (index >= m_networkCount) {
        return false;
    }

    String key = getNetworkKey(index);
    String jsonStr = m_prefs.getString(key.c_str(), "");
    
    if (jsonStr.length() == 0) {
        return false;  // Gap in storage
    }
    
    // Parse JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    
    if (error) {
        return false;
    }
    
    const char* ssid = doc["ssid"];
    const char* password = doc["password"];
    
    if (ssid && strlen(ssid) > 0) {
        network.ssid = String(ssid);
        network.password = password ? String(password) : String("");
        return true;
    }
    
    return false;
}

bool WiFiCredentialsStorage::hasNetwork(const String& ssid) {
    return findNetworkIndex(ssid) != 255;
}

bool WiFiCredentialsStorage::clearAll() {
    if (!m_initialized) {
        LW_LOGE("WiFiCredentialsStorage not initialized");
        return false;
    }

    // Delete all network keys
    for (uint8_t i = 0; i < MAX_NETWORKS; i++) {
        String key = getNetworkKey(i);
        m_prefs.remove(key.c_str());
    }

    // Reset count
    m_networkCount = 0;
    updateNetworkCount();
    
    LW_LOGI("All networks cleared");
    return true;
}

size_t WiFiCredentialsStorage::getAvailableSpace() const {
    // NVS namespace size is ~4000 bytes, but we can only estimate
    // Each network entry: ~100-200 bytes (key + JSON value + overhead)
    size_t used = m_networkCount * 150;  // Rough estimate
    size_t available = 4000 - used;  // Conservative estimate
    return available > 0 ? available : 0;
}

uint8_t WiFiCredentialsStorage::findNetworkIndex(const String& ssid) {
    if (!m_initialized) {
        return 255;
    }

    NetworkCredential cred;
    for (uint8_t i = 0; i < m_networkCount; i++) {
        if (getNetwork(i, cred)) {
            if (cred.ssid == ssid) {
                return i;
            }
        }
    }

    return 255;  // Not found
}

void WiFiCredentialsStorage::compactStorage() {
    // Compact storage: remove gaps left by deleted networks
    // Reorder networks sequentially (net_0, net_1, ..., net_N)
    
    if (!m_initialized) {
        return;
    }

    // Load all networks into a temporary array
    NetworkCredential networks[MAX_NETWORKS];
    uint8_t validCount = 0;

    for (uint8_t i = 0; i < MAX_NETWORKS; i++) {
        String key = getNetworkKey(i);
        String jsonStr = m_prefs.getString(key.c_str(), "");
        
        if (jsonStr.length() > 0) {
            // Parse and store valid network
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, jsonStr);
            
            if (!error) {
                const char* ssid = doc["ssid"];
                const char* password = doc["password"];
                
                if (ssid && strlen(ssid) > 0 && validCount < MAX_NETWORKS) {
                    networks[validCount].ssid = String(ssid);
                    networks[validCount].password = password ? String(password) : String("");
                    validCount++;
                }
            }
        }
    }

    // Clear all keys
    for (uint8_t i = 0; i < MAX_NETWORKS; i++) {
        String key = getNetworkKey(i);
        m_prefs.remove(key.c_str());
    }

    // Rewrite networks sequentially (removes gaps)
    m_networkCount = 0;
    for (uint8_t i = 0; i < validCount; i++) {
        String key = getNetworkKey(i);
        
        JsonDocument doc;
        doc["ssid"] = networks[i].ssid;
        doc["password"] = networks[i].password;
        
        String jsonStr;
        serializeJson(doc, jsonStr);
        
        m_prefs.putString(key.c_str(), jsonStr);
        m_networkCount++;
    }

    updateNetworkCount();
    LW_LOGI("Storage compacted - %d networks reordered", m_networkCount);
}

void WiFiCredentialsStorage::updateNetworkCount() {
    if (m_initialized) {
        m_prefs.putUChar("count", m_networkCount);
    }
}

void WiFiCredentialsStorage::setLastConnectedSSID(const String& ssid) {
    if (!m_initialized) {
        LW_LOGE("WiFiCredentialsStorage not initialized");
        return;
    }

    if (ssid.length() == 0 || ssid.length() > 32) {
        LW_LOGW("Invalid SSID for last connected: length %d", ssid.length());
        return;
    }

    m_prefs.putString("last_ssid", ssid);
    LW_LOGI("Last connected SSID saved: %s", ssid.c_str());
}

String WiFiCredentialsStorage::getLastConnectedSSID() const {
    if (!m_initialized) {
        return String("");
    }

    // Need to cast away const for Preferences (it's safe, getString doesn't modify state)
    Preferences& prefs = const_cast<Preferences&>(m_prefs);
    return prefs.getString("last_ssid", "");
}

bool WiFiCredentialsStorage::getCredentialsForSSID(const String& ssid, String& outPassword) {
    if (!m_initialized) {
        return false;
    }

    uint8_t index = findNetworkIndex(ssid);
    if (index == 255) {
        return false;  // Network not found
    }

    NetworkCredential cred;
    if (getNetwork(index, cred)) {
        outPassword = cred.password;
        return true;
    }

    return false;
}

} // namespace network
} // namespace lightwaveos

#endif // FEATURE_WEB_SERVER
