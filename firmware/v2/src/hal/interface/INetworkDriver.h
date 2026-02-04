/**
 * @file INetworkDriver.h
 * @brief Hardware abstraction interface for network connectivity
 *
 * Provides a platform-agnostic interface for network operations,
 * supporting ESP32-S3 (WiFi) and ESP32-P4 (Ethernet or ESP-Hosted WiFi).
 */

#pragma once

#include <cstdint>
#include <functional>

// Forward declare IPAddress to avoid pulling in full WiFi headers
#ifndef IPAddress
class IPAddress;
#endif

namespace lightwaveos {
namespace hal {

/**
 * @brief Network connection type
 */
enum class NetworkType {
    None,           ///< No network configured
    WiFiStation,    ///< WiFi client mode
    WiFiAP,         ///< WiFi access point mode
    Ethernet,       ///< Wired Ethernet
    EspHosted       ///< WiFi via ESP-Hosted (P4 with companion chip)
};

/**
 * @brief Network connection state
 */
enum class NetworkState {
    Disconnected,   ///< Not connected
    Connecting,     ///< Connection in progress
    Connected,      ///< Connected, IP acquired
    Failed,         ///< Connection failed
    APMode          ///< Running as access point
};

/**
 * @brief Network configuration for station mode
 */
struct NetworkStationConfig {
    const char* ssid = nullptr;         ///< Network SSID
    const char* password = nullptr;     ///< Network password
    uint32_t timeoutMs = 20000;         ///< Connection timeout
    bool autoReconnect = true;          ///< Auto-reconnect on disconnect
};

/**
 * @brief Network configuration for AP mode
 */
struct NetworkAPConfig {
    const char* ssid = "LightwaveOS-AP";
    const char* password = nullptr;     ///< nullptr for open network
    uint8_t channel = 1;
    uint8_t maxConnections = 4;
};

/**
 * @brief Network event callback type
 */
using NetworkEventCallback = std::function<void(NetworkState state)>;

/**
 * @brief Network statistics
 */
struct NetworkStats {
    uint32_t connectAttempts = 0;       ///< Total connection attempts
    uint32_t successfulConnects = 0;    ///< Successful connections
    uint32_t disconnects = 0;           ///< Number of disconnects
    int8_t rssi = 0;                    ///< Signal strength (WiFi only)
    uint32_t uptimeMs = 0;              ///< Time since last connect
};

/**
 * @brief Abstract interface for network connectivity
 *
 * Platform-specific implementations:
 * - ESP32-S3: WiFiDriver_S3 (native WiFi)
 * - ESP32-P4: EthernetDriver_P4 or EspHostedDriver_P4
 */
class INetworkDriver {
public:
    virtual ~INetworkDriver() = default;

    /**
     * @brief Initialize the network driver
     * @return true if initialization succeeded
     */
    virtual bool init() = 0;

    /**
     * @brief Deinitialize and release resources
     */
    virtual void deinit() = 0;

    /**
     * @brief Connect to a network (station mode)
     * @param config Station configuration
     * @return true if connection started successfully
     */
    virtual bool connect(const NetworkStationConfig& config) = 0;

    /**
     * @brief Start access point mode
     * @param config AP configuration
     * @return true if AP started successfully
     */
    virtual bool startAP(const NetworkAPConfig& config) = 0;

    /**
     * @brief Disconnect from current network
     */
    virtual void disconnect() = 0;

    /**
     * @brief Get current connection state
     * @return Current state
     */
    virtual NetworkState getState() const = 0;

    /**
     * @brief Check if connected
     * @return true if connected with IP
     */
    virtual bool isConnected() const = 0;

    /**
     * @brief Get local IP address
     * @param ip Output buffer (4 bytes)
     * @return true if IP is available
     */
    virtual bool getIP(uint8_t* ip) const = 0;

    /**
     * @brief Get local IP as string
     * @param buffer Output buffer
     * @param bufferSize Buffer size
     * @return true if successful
     */
    virtual bool getIPString(char* buffer, size_t bufferSize) const = 0;

    /**
     * @brief Get the network type
     * @return Type of network connection
     */
    virtual NetworkType getType() const = 0;

    /**
     * @brief Get MAC address
     * @param mac Output buffer (6 bytes)
     * @return true if successful
     */
    virtual bool getMAC(uint8_t* mac) const = 0;

    /**
     * @brief Set event callback
     * @param callback Function to call on state changes
     */
    virtual void setEventCallback(NetworkEventCallback callback) = 0;

    /**
     * @brief Get hostname
     * @return Configured hostname
     */
    virtual const char* getHostname() const = 0;

    /**
     * @brief Set hostname
     * @param hostname New hostname
     * @return true if successful
     */
    virtual bool setHostname(const char* hostname) = 0;

    /**
     * @brief Get network statistics
     * @return Current statistics
     */
    virtual const NetworkStats& getStats() const = 0;

    /**
     * @brief Reset statistics counters
     */
    virtual void resetStats() = 0;

    /**
     * @brief Process network events (call from main loop)
     *
     * Some implementations may need periodic polling.
     * Safe to call frequently; implementations should be non-blocking.
     */
    virtual void process() = 0;

    // WiFi-specific methods (no-op for Ethernet)

    /**
     * @brief Scan for available networks (WiFi only)
     * @return Number of networks found, -1 if not supported
     */
    virtual int scanNetworks() { return -1; }

    /**
     * @brief Get RSSI signal strength (WiFi only)
     * @return RSSI in dBm, 0 if not applicable
     */
    virtual int8_t getRSSI() const { return 0; }
};

} // namespace hal
} // namespace lightwaveos
