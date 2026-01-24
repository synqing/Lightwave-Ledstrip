/**
 * @file WiFiCredentialsStorage.h
 * @brief NVS-based storage for WiFi network credentials
 *
 * Stores multiple WiFi networks (SSID + password pairs) in NVS.
 * Provides methods to save, load, delete, and query saved networks.
 *
 * Storage format:
 * - Namespace: "wifi_creds"
 * - Keys: "net_0", "net_1", ... "net_N" (JSON format: {"ssid":"...","password":"..."})
 * - Metadata key: "count" (number of saved networks)
 *
 * Limitations:
 * - Maximum 10 networks (ESP32-S3 NVS namespace size ~4000 bytes)
 * - SSID max length: 32 chars (WiFi standard)
 * - Password max length: 64 chars (WPA2 standard)
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "../config/features.h"

#if FEATURE_WEB_SERVER

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>

namespace lightwaveos {
namespace network {

/**
 * @brief WiFi network credential storage in NVS
 */
class WiFiCredentialsStorage {
public:
    /**
     * @brief Network credential structure
     */
    struct NetworkCredential {
        String ssid;
        String password;
        bool isValid() const { return ssid.length() > 0 && ssid.length() <= 32; }
    };

    /**
     * @brief Maximum number of networks we can store
     */
    static constexpr uint8_t MAX_NETWORKS = 10;

    /**
     * @brief Constructor
     */
    WiFiCredentialsStorage();

    /**
     * @brief Destructor
     */
    ~WiFiCredentialsStorage();

    // Prevent copying
    WiFiCredentialsStorage(const WiFiCredentialsStorage&) = delete;
    WiFiCredentialsStorage& operator=(const WiFiCredentialsStorage&) = delete;

    /**
     * @brief Initialize NVS storage
     * @return true if initialization succeeded
     */
    bool begin();

    /**
     * @brief Close NVS storage
     */
    void end();

    /**
     * @brief Save a network credential to NVS
     * @param ssid Network SSID
     * @param password Network password
     * @return true if saved successfully, false if storage full or invalid
     */
    bool saveNetwork(const String& ssid, const String& password);

    /**
     * @brief Load all saved networks from NVS
     * @param networks Output array to fill
     * @param maxNetworks Maximum number of networks to load (array size)
     * @return Number of networks actually loaded
     */
    uint8_t loadNetworks(NetworkCredential* networks, uint8_t maxNetworks);

    /**
     * @brief Delete a network from NVS by SSID
     * @param ssid Network SSID to delete
     * @return true if deleted successfully, false if not found
     */
    bool deleteNetwork(const String& ssid);

    /**
     * @brief Get the number of saved networks
     * @return Number of saved networks (0-MAX_NETWORKS)
     */
    uint8_t getNetworkCount() const;

    /**
     * @brief Get a specific network by index
     * @param index Network index (0-based)
     * @param network Output network credential structure
     * @return true if network found and loaded, false if index invalid
     */
    bool getNetwork(uint8_t index, NetworkCredential& network);

    /**
     * @brief Check if a network with given SSID exists
     * @param ssid Network SSID to check
     * @return true if network exists, false otherwise
     */
    bool hasNetwork(const String& ssid);

    /**
     * @brief Clear all saved networks
     * @return true if cleared successfully
     */
    bool clearAll();

    /**
     * @brief Get available storage space (rough estimate)
     * @return Approximate bytes remaining in namespace
     */
    size_t getAvailableSpace() const;

    /**
     * @brief Set the last successfully connected SSID
     * @param ssid The SSID that was successfully connected
     */
    void setLastConnectedSSID(const String& ssid);

    /**
     * @brief Get the last successfully connected SSID
     * @return The last connected SSID, or empty string if none
     */
    String getLastConnectedSSID() const;

private:
    Preferences m_prefs;
    bool m_initialized;
    uint8_t m_networkCount;

    /**
     * @brief Internal: Get NVS key for network index
     * @param index Network index (0-based)
     * @return Key string (e.g., "net_0")
     */
    String getNetworkKey(uint8_t index) const;

    /**
     * @brief Internal: Compact storage after deletion (remove gaps)
     */
    void compactStorage();

    /**
     * @brief Internal: Find network index by SSID
     * @param ssid Network SSID to find
     * @return Network index (0-based) or 255 if not found
     */
    uint8_t findNetworkIndex(const String& ssid);

    /**
     * @brief Internal: Update network count in NVS
     */
    void updateNetworkCount();
};

} // namespace network
} // namespace lightwaveos

#endif // FEATURE_WEB_SERVER
